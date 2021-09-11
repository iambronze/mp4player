#include "media/audio_decoder.h"
#include "base/logging.h"

namespace media {

FFmpegAudioDecoder::FFmpegAudioDecoder() = default;

FFmpegAudioDecoder::~FFmpegAudioDecoder() {
  UnInit();
}

bool FFmpegAudioDecoder::Init(AVStream *stream) {
  UnInit();
  int ret;
  AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
  if (!codec) {
    LOG(ERROR) << "avcodec_find_decoder failed: " << stream->codecpar->codec_id;
    return false;
  }
  codec_context_.reset(avcodec_alloc_context3(codec));
  if (!codec_context_) {
    LOG(ERROR) << "avcodec_alloc_context3 failed";
    return false;
  }
  if ((ret = avcodec_parameters_to_context(codec_context_.get(), stream->codecpar)) < 0) {
    LOG(ERROR) << "avcodec_parameters_to_context failed:" << ret << ",err:" << AVErrorToString(ret);
    return false;
  }
  if ((ret = avcodec_open2(codec_context_.get(), codec, nullptr)) < 0) {
    LOG(ERROR) << "avcodec_open2 failed:" << ret << ",err:" << AVErrorToString(ret);
    return false;
  }
  return true;
}

void FFmpegAudioDecoder::UnInit() {
  codec_context_.reset();
}

int FFmpegAudioDecoder::SendInput(const AVPacket *pkt) const {
  return avcodec_send_packet(codec_context_.get(), pkt);
}

int FFmpegAudioDecoder::FetchOutput(AVFrame *frame) const {
  return avcodec_receive_frame(codec_context_.get(), frame);
}

void FFmpegAudioDecoder::Flush() const {
  if (codec_context_) {
    avcodec_flush_buffers(codec_context_.get());
  }
}

AVCodecContext *FFmpegAudioDecoder::codec_context() {
  return codec_context_.get();
}
}