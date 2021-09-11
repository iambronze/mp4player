#include "base/logging.h"
#include "media/audio_decoder_thread.h"
#include "media/mp4_dataset.h"
#include "media/audio_decoder.h"
#include "media/ffmpeg_aac_bitstream_converter.h"
#include "media/audio_frame_queue.h"
#include "media/packet_queue.h"
#include "media/audio_resampler.h"
#include "media/video_player.h"
#include "media/media_constants.h"

namespace media {

AudioDecoderThread::AudioDecoderThread(VideoPlayer *player,
                                       Mp4Dataset *dataset,
                                       PacketQueue *input_queue,
                                       AudioFrameQueue *output_queue)
    : player_(player),
      dataset_(dataset),
      input_queue_(input_queue),
      output_queue_(output_queue),
      next_pts_(0),
      keep_running_(true),
      thread_(new base::DelegateSimpleThread(this, "ADThread")) {
  thread_->Start();
}

AudioDecoderThread::~AudioDecoderThread() {
  keep_running_ = false;
  if (thread_) {
    thread_->Join();
    thread_.reset();
  }
}

void AudioDecoderThread::Run() {
  InitDecoder();
  DecodeLoop();
  UnInitDecoder();
}

bool AudioDecoderThread::InitDecoder() {
  AVStream *stream = dataset_->getAudioStream();

  std::unique_ptr<FFmpegAudioDecoder> decoder(new FFmpegAudioDecoder());
  if (decoder->Init(stream)) {
    decoder_ = std::move(decoder);
  } else {
    LOG(ERROR) << "create audio decoder failed";
    player_->OnMediaError(Error_AudioCodecCreateFailed);
    return false;
  }

  if (stream->codecpar->codec_id == AV_CODEC_ID_AAC) {
    //AAC需要转换一下
    bitstream_converter_ = base::WrapUnique(new FFmpegAACBitstreamConverter(stream->codecpar));
  } else {
    LOG(WARNING) << "Decode audio codec[" << stream->codecpar->codec_id << "] maybe fail";
  }

  resampler_ = base::WrapUnique(new FFmpegAudioResampler());
  if (!resampler_->Init(decoder_->codec_context(), 0, kAudioChannels)) {
    LOG(ERROR) << "Create audio resampler failed";
    resampler_.reset();
    player_->OnMediaError(Error_AudioResamplerCreateFailed);
  }
  return true;
}

void AudioDecoderThread::UnInitDecoder() {
  output_queue_->flush();
  resampler_.reset();
  bitstream_converter_.reset();
  decoder_.reset();
}

void AudioDecoderThread::DecodeLoop() {
  while (keep_running_) {
    AVPacket *pkt = FetchPacket();

    if (pkt) {
      if (pkt->data == PacketQueue::kFlushPkt.data) {
        DLOG(INFO) << "Got audio flush packet";
        //flush decoder
        if (decoder_) {
          decoder_->Flush();
        }
        output_queue_->flush();
        next_pts_ = 0;
        player_->OnFlushCompleted(dataset_->getAudioStreamIndex());
        continue;
      }

      if (pkt->data) {
        if (decoder_) {
          if (bitstream_converter_) { //AAC
            if (bitstream_converter_->ConvertPacket(pkt)) {
              DecodeOnePacket(pkt);
            }
          } else {
            //NOT AAC ?
            DecodeOnePacket(pkt);
          }
        }
      } else {
        DLOG(INFO) << "Got audio EOS packet";
        // AAC has no dependent frames so we needn't flush the decoder.
      }
      av_packet_unref(pkt);
      av_packet_free(&pkt);
    } else {
      //应该到文件末尾了,没有数据需要解码,我们要把解码器中的剩余数据读完
      if (!ProcessOutputFrame()) {
        usleep(5000);
      }
    }
  }
}

void AudioDecoderThread::DecodeOnePacket(AVPacket *pkt) {
  bool sent_packet = false, frames_remaining = true;
  while (!sent_packet || frames_remaining) {
    if (!sent_packet) {
      const int result = decoder_->SendInput(pkt);
      if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
        DLOG(ERROR) << "Failed to send packet for decoding: " << result;
        return;
      }
      sent_packet = result != AVERROR(EAGAIN);
    }

    // See if any frames are available. If we receive an EOF or EAGAIN, there
    // should be nothing left to do this pass since we've already provided the
    // only input packet that we have.
    std::unique_ptr<AVFrame, ScopedPtrAVFreeFrame> frame(av_frame_alloc());
    const int result = decoder_->FetchOutput(frame.get());
    if (result == AVERROR_EOF || result == AVERROR(EAGAIN)) {
      frames_remaining = false;

      // TODO(dalecurtis): This should be a DCHECK() or MEDIA_LOG, but since
      // this API is new, lets make it a CHECK first and monitor reports.
      if (result == AVERROR(EAGAIN)) {
        CHECK(sent_packet) << "avcodec_receive_frame() and "
                              "avcodec_send_packet() both returned EAGAIN, "
                              "which is an API violation.";
      }
      continue;
    } else if (result < 0) {
      DLOG(ERROR) << "Failed to decode frame: " << result;
      continue;
    }
    OnNewFrame(frame.get());
    av_frame_unref(frame.get());
  }
}

//没有数据可以输入的时候,我们还要继续读取输出
bool AudioDecoderThread::ProcessOutputFrame() {
  if (!decoder_)
    return false;

  std::unique_ptr<AVFrame, ScopedPtrAVFreeFrame> frame(av_frame_alloc());
  const int result = decoder_->FetchOutput(frame.get());
  if (result >= 0) {
    OnNewFrame(frame.get());
    av_frame_unref(frame.get());
    return true;
  }
  return false;
}

//获得一个解码帧,还需要转码
void AudioDecoderThread::OnNewFrame(AVFrame *frame) {
  if (!resampler_)
    return;

  uint8_t **output_samples = nullptr;
  const int out_nb_samples = resampler_->Resample((const uint8_t **) frame->extended_data,
                                                  frame->nb_samples,
                                                  &output_samples);
  if (out_nb_samples > 0) {
    int buffer_size = av_samples_get_buffer_size(nullptr,
                                                 kAudioChannels,
                                                 out_nb_samples,
                                                 AV_SAMPLE_FMT_S16,
                                                 1);
    if (buffer_size <= 0)
      return;

    MEDIA_BUFFER mb = RK_MPI_MB_CreateAudioBuffer(buffer_size, RK_FALSE);
    memcpy(RK_MPI_MB_GetPtr(mb), (const int16_t *) output_samples[0], buffer_size);
    RK_MPI_MB_SetSize(mb, buffer_size);
    //尝试修复 pts 问题
    if (frame->pts == static_cast<int64_t>(AV_NOPTS_VALUE)) {
      frame->pts = next_pts_;
    }
    if (frame->pts != static_cast<int64_t>(AV_NOPTS_VALUE)) {
      next_pts_ = frame->pts + av_rescale_q(frame->nb_samples,
                                            decoder_->codec_context()->time_base,
                                            dataset_->getAudioStream()->time_base);
    }
    RK_MPI_MB_SetTimestamp(mb, frame->pts);
    if (!SendFrame(mb)) {
      RK_MPI_MB_ReleaseBuffer(mb);
    }
  }
}

/*
 * 这里需要注意,此处可能是阻塞的,如果 render线程没有及时取走解码后的数据
 * 我们需要休眠一会
 */
bool AudioDecoderThread::SendFrame(MEDIA_BUFFER mb) {
  while (keep_running_) {
    if (!output_queue_->is_writable()) {
      usleep(5000);
      continue;
    }
    output_queue_->put(mb);
    return true;
  }
  return false;
}

//从文件读取一帧用于解码
AVPacket *AudioDecoderThread::FetchPacket() {
  AVPacket *pkt = input_queue_->get();
  while (!pkt) {
    DemuxResult result = dataset_->demuxNextPacket();
    if (result != DemuxResult::OK)
      break; //AV_EOF or UNKNOWN
    pkt = input_queue_->get();
  }
  return pkt;
}
}