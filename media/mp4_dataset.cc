#include "media/mp4_dataset.h"
#include "media/packet_queue.h"
#include "base/logging.h"

namespace media {
namespace {

AVPacket *make_eos_packet(int stream_idx) {
  AVPacket *pkt = av_packet_alloc();
  av_init_packet(pkt);
  pkt->data = nullptr;
  pkt->size = 0;
  pkt->stream_index = stream_idx;
  return pkt;
}
}

std::unique_ptr<Mp4Dataset> Mp4Dataset::create(const std::string &file) {
  std::unique_ptr<Mp4Dataset> dataset(new Mp4Dataset());
  if (dataset->init(file) < 0) {
    LOG(ERROR) << "Failed to initialize mp4 dataset";
    return nullptr;
  }
  return dataset;
}

Mp4Dataset::Mp4Dataset()
    : format_ctx_(nullptr) {}

Mp4Dataset::~Mp4Dataset() {
  clearFormatContext();
}

int Mp4Dataset::init(const std::string &file) {
  AVFormatContext *input_ctx = nullptr;
  int err = avformat_open_input(&input_ctx, file.c_str(), NULL, NULL);
  if (err) {
    LOG(ERROR) << "avformat_open_input:" << err << ",err:" << AVErrorToString(err);
    return -1;
  }
  format_ctx_ = input_ctx;
  err = avformat_find_stream_info(format_ctx_, NULL);
  if (err < 0) {
    LOG(ERROR) << "avformat_find_stream_info:" << err << ",err:" << AVErrorToString(err);
    return -1;
  }
  av_dump_format(format_ctx_, 0, file.c_str(), false);

  for (int i = 0; i < format_ctx_->nb_streams; i++) {
    if (format_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      audio_stream_idx_ = i;
      LOG(INFO) << "Found audio stream:" << format_ctx_->streams[i]->codecpar->codec_id;
    } else if (format_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_stream_idx_ = i;
      LOG(INFO) << "Found video stream:" << format_ctx_->streams[i]->codecpar->codec_id;
    }
  }
  // We expect a dataset to at least have one video stream
  if (video_stream_idx_ == -1) {
    LOG(ERROR) << "No video stream found";
    return -1;
  }

  for (int i = 0; i < 100; ++i) {
    std::unique_ptr<AVPacket, ScopedPtrAVFreePacket> packet(av_packet_alloc());
    int ret = av_read_frame(format_ctx_, packet.get());
    if (ret >= 0) {
      if (video_stream_idx_ >= 0 && video_stream_idx_ == packet->stream_index) {
        if (packet->dts != static_cast<int64_t>(AV_NOPTS_VALUE)
            && packet->pts != static_cast<int64_t>(AV_NOPTS_VALUE)) {
          enable_seek_ = true;
          av_packet_unref(packet.get());
          break;
        }
      }
    }
    av_packet_unref(packet.get());
  }
  return avformat_seek_file(format_ctx_,
                            -1,
                            INT64_MIN,
                            0,
                            INT64_MAX,
                            0);
}

DemuxResult Mp4Dataset::demuxNextPacket() {
  base::AutoLock l(lock_);
  std::unique_ptr<AVPacket, ScopedPtrAVFreePacket> packet(av_packet_alloc());
  int ret = av_read_frame(format_ctx_, packet.get());
  if (ret >= 0) {
    if (audio_queue_ && audio_stream_idx_ >= 0
        && audio_stream_idx_ == packet->stream_index) {
      audio_queue_->put(packet.release());

    } else if (video_queue_ && video_stream_idx_ >= 0
        && video_stream_idx_ == packet->stream_index) {
      video_queue_->put(packet.release());
    } else {
      av_packet_unref(packet.get());
    }
    return DemuxResult::OK;
  }

  if (ret == AVERROR_EOF || avio_feof(format_ctx_->pb)) {
    if (audio_queue_ && audio_stream_idx_ >= 0) {
      AVPacket *pkt = make_eos_packet(audio_stream_idx_);
      audio_queue_->put(pkt);
    }
    if (video_queue_ && video_stream_idx_ >= 0) {
      AVPacket *pkt = make_eos_packet(video_stream_idx_);
      video_queue_->put(pkt);
    }
    return DemuxResult::AV_EOF;
  }
  return DemuxResult::UNKNOWN;
}

int Mp4Dataset::seek(double timestamp) {
  base::AutoLock l(lock_);
  if (!enable_seek_) {
    return -1;
  }
  auto seek_time = static_cast<int64_t>(timestamp * base::Time::kMicrosecondsPerSecond);
  int ret = av_seek_frame(format_ctx_, -1, seek_time, AVSEEK_FLAG_BACKWARD);
  if (ret < 0) {
    LOG(ERROR) << "av_seek_frame:" << ret << ",err:" << AVErrorToString(ret);
    return ret;
  }
  if (audio_queue_ && audio_stream_idx_ >= 0) {
    audio_queue_->flush();
  }
  if (video_queue_ && video_stream_idx_ >= 0) {
    video_queue_->flush();
  }
  return 0;
}

int Mp4Dataset::rewind() {
  base::AutoLock l(lock_);
  int ret = avformat_seek_file(format_ctx_,
                               -1,
                               INT64_MIN,
                               0,
                               INT64_MAX,
                               0);
  if (ret < 0) {
    LOG(ERROR) << "avformat_seek_file:" << ret << ",err:" << AVErrorToString(ret);
    return ret;
  }
  return 0;
}

void Mp4Dataset::setAudioPacketQueue(PacketQueue *audio_queue) {
  audio_queue_ = audio_queue;
}

void Mp4Dataset::setVideoPacketQueue(PacketQueue *video_queue) {
  video_queue_ = video_queue;
}

int Mp4Dataset::getAudioStreamIndex() const {
  return audio_stream_idx_;
}

int Mp4Dataset::getVideoStreamIndex() const {
  return video_stream_idx_;
}

bool Mp4Dataset::seekable() const {
  return enable_seek_;
}

AVFormatContext *Mp4Dataset::getFormatContext() {
  return format_ctx_;
}

AVStream *Mp4Dataset::getAudioStream() {
  if (audio_stream_idx_ < 0)
    return nullptr;
  return format_ctx_->streams[audio_stream_idx_];
}

AVStream *Mp4Dataset::getVideoStream() {
  if (video_stream_idx_ < 0)
    return nullptr;
  return format_ctx_->streams[video_stream_idx_];
}

void Mp4Dataset::clearFormatContext() {
  if (format_ctx_) {
    if (format_ctx_->iformat) {
      //input context
      avformat_close_input(&format_ctx_);
    } else {
      //output context
      if (format_ctx_->pb) {
        avio_closep(&format_ctx_->pb);
      }
    }
    avformat_free_context(format_ctx_);
    format_ctx_ = nullptr;
  }
}
}