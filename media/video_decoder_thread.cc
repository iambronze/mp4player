#include "base/logging.h"
#include "media/video_decoder_thread.h"
#include "media/mp4_dataset.h"
#include "media/mpp_decoder.h"
#include "media/video_frame_queue.h"
#include "media/packet_queue.h"
#include "media/video_player.h"

namespace media {

VideoDecoderThread::VideoDecoderThread(VideoPlayer *player,
                                       Mp4Dataset *dataset,
                                       PacketQueue *input_queue,
                                       VideoFrameQueue *output_queue)
    : player_(player),
      dataset_(dataset),
      input_queue_(input_queue),
      output_queue_(output_queue),
      avbsf_(nullptr),
      next_pts_(0),
      keep_running_(true),
      thread_(new base::DelegateSimpleThread(this, "VDThread")) {
  thread_->Start();
}

VideoDecoderThread::~VideoDecoderThread() {
  keep_running_ = false;
  if (thread_) {
    thread_->Join();
    thread_.reset();
  }
}

void VideoDecoderThread::Run() {
  InitDecoder();
  DecodeLoop();
  UnInitDecoder();
}

void VideoDecoderThread::InitDecoder() {
  AVStream *stream = dataset_->getVideoStream();

  MppCodingType coding_type = MPP_VIDEO_CodingUnused;
  if (stream->codecpar->codec_id == AV_CODEC_ID_HEVC) {
    coding_type = MPP_VIDEO_CodingHEVC;
  } else if (stream->codecpar->codec_id == AV_CODEC_ID_H264) {
    coding_type = MPP_VIDEO_CodingAVC;
  } else {
    player_->OnMediaError(Error_VideoCodecUnsupported);
    return;
  }

  size_t buffer_size = output_queue_->max_size();
  if (buffer_size < FRAMEGROUP_MAX_FRAMES) {
    buffer_size = FRAMEGROUP_MAX_FRAMES;
  }
  std::unique_ptr<RKMppDecoder> decoder(new RKMppDecoder(coding_type, buffer_size + 2));
  if (decoder->Init()) {
    decoder_ = std::move(decoder);
  } else {
    LOG(ERROR) << "create video decoder failed";
    player_->OnMediaError(Error_VideoCodecCreateFailed);
  }
  std::string bsf_name;
  if (stream->codecpar->codec_id == AV_CODEC_ID_H264) {
    bsf_name = "h264_mp4toannexb";
  } else if (stream->codecpar->codec_id == AV_CODEC_ID_HEVC) {
    bsf_name = "hevc_mp4toannexb";
  }
  if (!bsf_name.empty()) {
    const struct AVBitStreamFilter *bsfptr = av_bsf_get_by_name(bsf_name.c_str());
    av_bsf_alloc(bsfptr, &avbsf_);
    avcodec_parameters_copy(avbsf_->par_in, stream->codecpar);
    av_bsf_init(avbsf_);
  }

  AVRational frame_rate = av_guess_frame_rate(dataset_->getFormatContext(), stream, nullptr);
  double fps = frame_rate.num && frame_rate.den ? av_q2d(frame_rate) : 30.0f;
  frame_duration_ = base::TimeDelta::FromSecondsD(1.0 / fps);
}

void VideoDecoderThread::UnInitDecoder() {
  output_queue_->flush();
  decoder_.reset();
  if (avbsf_) {
    av_bsf_free(&avbsf_);
    avbsf_ = nullptr;
  }
}

void VideoDecoderThread::DecodeLoop() {
  while (keep_running_) {
    AVPacket *pkt = FetchPacket();

    bool eos_reached = false;

    if (pkt) {
      if (pkt->data == PacketQueue::kFlushPkt.data) {
        DLOG(INFO) << "Got video flush packet";
        //flush decoder
        if (decoder_) {
          decoder_->Flush();
        }
        output_queue_->flush();
        next_pts_ = 0;
        player_->OnFlushCompleted(dataset_->getVideoStreamIndex());
        continue;
      }

      if (!pkt->data) {
        //读到文件末尾了,我们需要将 end of stream packet 写入解码器
        //并等待解码器输出 eos帧,则说明解码器已经输出所有的帧,此刻可以正常关闭解码器
        DLOG(INFO) << "Got video EOS packet";
        SendInput(pkt, &eos_reached);
      } else {
        if (avbsf_) {
          //H264,H265要处理之后才能送到解码器
          int err = av_bsf_send_packet(avbsf_, pkt);
          if (err < 0) {
            av_packet_unref(pkt);
            av_packet_free(&pkt);
            continue;
          }
          while ((err = av_bsf_receive_packet(avbsf_, pkt)) == 0) {
            SendInput(pkt, &eos_reached);
          }
        } else {
          SendInput(pkt, &eos_reached);
        }
      }
    } else {
      if (!ProcessOneOutputBuffer(&eos_reached)) {
        usleep(5000);
      }
    }
    if (eos_reached) {
      decoder_->Flush();
    }
  }
}

void VideoDecoderThread::SendInput(AVPacket *pkt, bool *eos_reached) {
  MppPacket mpp_packet = MakeMppPacket(pkt);
  if (mpp_packet) {
    DecodePacket(mpp_packet, eos_reached);
    mpp_packet_deinit(&mpp_packet);
  }
  av_packet_unref(pkt);
  av_packet_free(&pkt);
}

AVPacket *VideoDecoderThread::FetchPacket() {
  AVPacket *pkt = input_queue_->get();
  while (!pkt) {
    DemuxResult result = dataset_->demuxNextPacket();
    if (result != DemuxResult::OK)
      break; //AV_EOF or UNKNOWN
    pkt = input_queue_->get();
  }
  return pkt;
}

bool VideoDecoderThread::ProcessOneOutputBuffer(bool *eos_reached) {
  if (!decoder_)
    return false;

  MppFrame frame = decoder_->FetchOutput();
  if (!frame)
    return false;

  if (mpp_frame_get_eos(frame)) {
    *eos_reached = true;
    DLOG(INFO) << "Received a EOS frame";
  }
  if (!SendFrame(frame)) {
    mpp_frame_deinit(&frame);
  }
  return true;
}

bool VideoDecoderThread::DecodePacket(MppPacket mpp_packet, bool *eos_reached) {
  if (!decoder_)
    return false;

  bool sent_packet = false, frames_remaining = true;
  while (!sent_packet || frames_remaining) {
    if (!sent_packet) {
      const int result = decoder_->SendInput(mpp_packet);
      if (result < 0 && result != MPP_ERR_BUFFER_FULL) {
        return false;
      }
      sent_packet = (result != MPP_ERR_BUFFER_FULL);
    }

    MppFrame frame = decoder_->FetchOutput();
    if (!frame) {
      frames_remaining = false;
      continue;
    }

    if (mpp_frame_get_eos(frame)) {
      *eos_reached = true;
      DLOG(INFO) << "Received a EOS frame";
    }
    if (!SendFrame(frame)) {
      mpp_frame_deinit(&frame);
    }
  }
  return true;
}

bool VideoDecoderThread::SendFrame(MppFrame frame) {
  int64_t pts = mpp_frame_get_pts(frame);
  if (pts == static_cast<int64_t>(AV_NOPTS_VALUE)) {
    pts = next_pts_;
    mpp_frame_set_pts(frame, pts);
  }
  if (pts != static_cast<int64_t>(AV_NOPTS_VALUE)) {
    next_pts_ = pts + frame_duration_.InMicroseconds();
  }

  while (keep_running_) {
    if (!output_queue_->is_writable()) {
      usleep(5000);
      continue;
    }
    output_queue_->put(frame);
    return true;
  }
  return false;
}

MppPacket VideoDecoderThread::MakeMppPacket(AVPacket *packet) {
  MppPacket mpp_packet = nullptr;
  MPP_RET ret = mpp_packet_init(&mpp_packet, packet->data, packet->size);
  if (ret != MPP_OK) {
    LOG(ERROR) << "mpp_packet_init failed, ret: " << ret;
    return nullptr;
  }

  if (packet->dts != static_cast<int64_t>(AV_NOPTS_VALUE)) {
    auto dts = media::ConvertFromTimeBase(dataset_->getVideoStream()->time_base, packet->dts);
    mpp_packet_set_dts(mpp_packet, dts.InMicroseconds());
  } else {
    mpp_packet_set_dts(mpp_packet, packet->dts);
  }
  if (packet->pts != static_cast<int64_t>(AV_NOPTS_VALUE)) {
    auto pts = media::ConvertFromTimeBase(dataset_->getVideoStream()->time_base, packet->pts);
    mpp_packet_set_pts(mpp_packet, pts.InMicroseconds());
  } else {
    mpp_packet_set_pts(mpp_packet, packet->pts);
  }

  if (!packet->data) {
    mpp_packet_set_eos(mpp_packet);
    DLOG(INFO) << "Send EOS to decoder";
  }
  return mpp_packet;
}
}