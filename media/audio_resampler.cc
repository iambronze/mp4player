#include "media/audio_resampler.h"
#include "base/logging.h"

namespace media {

FFmpegAudioResampler::FFmpegAudioResampler()
    : max_dst_nb_samples_(0),
      converted_input_samples_(nullptr),
      in_sample_rate_(0),
      in_channels_(0),
      out_sample_rate_(0),
      out_channels_(0),
      resampler_context_(nullptr) {}

FFmpegAudioResampler::~FFmpegAudioResampler() {
  UnInit();
}

void FFmpegAudioResampler::UnInit() {
  ReleaseBuffer();
  if (resampler_context_) {
    swr_free(&resampler_context_);
    resampler_context_ = nullptr;
  }
}

void FFmpegAudioResampler::ReleaseBuffer() {
  if (converted_input_samples_) {
    av_freep(&converted_input_samples_[0]);
    av_freep(&converted_input_samples_);
    converted_input_samples_ = nullptr;
  }
}

bool FFmpegAudioResampler::Init(AVCodecContext *codec_context,
                                int out_sample_rate,
                                int out_channels) {
  if (resampler_context_)
    return true;
  in_sample_rate_ = codec_context->sample_rate;
  in_channels_ = av_get_channel_layout_nb_channels(codec_context->channel_layout);

  out_sample_rate_ = out_sample_rate;
  if (out_sample_rate_ < 1)
    out_sample_rate_ = in_sample_rate_;

  out_channels_ = out_channels;
  if (out_channels_ < 1) {
    out_channels_ = 2;
  }
  int64_t out_channel_layout = av_get_default_channel_layout(out_channels);

  resampler_context_ = swr_alloc_set_opts(nullptr,
                                          out_channel_layout,
                                          AV_SAMPLE_FMT_S16,
                                          codec_context->sample_rate,
                                          (int64_t) codec_context->channel_layout,
                                          codec_context->sample_fmt,
                                          codec_context->sample_rate,
                                          0,
                                          nullptr);
  if (!resampler_context_) {
    LOG(ERROR) << "swr_alloc_set_opts failed";
    return false;
  }
  int err = swr_init(resampler_context_);
  if (err < 0) {
    LOG(ERROR) << "swr_init failed:" << err << ",err:" << AVErrorToString(err);
    UnInit();
    return false;
  }

  err = av_samples_alloc_array_and_samples(&converted_input_samples_,
                                           nullptr,
                                           out_channels_,
                                           8192,
                                           AV_SAMPLE_FMT_S16,
                                           1);
  if (err < 0) {
    LOG(ERROR) << "av_samples_alloc_array_and_samples failed:" << err << ",err:" << AVErrorToString(err);
    UnInit();
    return false;
  }
  max_dst_nb_samples_ = 8192;

  return true;
}

int FFmpegAudioResampler::Resample(const uint8_t **data,
                                   int src_nb_samples,
                                   uint8_t ***output_samples) {
  if (!resampler_context_)
    return -1;
  /* compute destination number of samples */
  int64_t dst_nb_samples = av_rescale_rnd(swr_get_delay(resampler_context_, in_sample_rate_) + src_nb_samples,
                                          out_sample_rate_,
                                          in_sample_rate_,
                                          AV_ROUND_UP);
  if (dst_nb_samples <= 0)
    return -1;

  if (dst_nb_samples > max_dst_nb_samples_) {
    ReleaseBuffer();
    if (av_samples_alloc_array_and_samples(&converted_input_samples_,
                                           nullptr,
                                           out_channels_,
                                           (int) dst_nb_samples,
                                           AV_SAMPLE_FMT_S16,
                                           1) < 0) {
      return AVERROR(ENOMEM);
    }
    max_dst_nb_samples_ = dst_nb_samples;
  }

  /* convert to destination format */
  int err = swr_convert(resampler_context_,
                        converted_input_samples_,
                        (int) dst_nb_samples,
                        data,
                        src_nb_samples);

  if (err < 0) {
    return err;
  }
  *output_samples = converted_input_samples_;
  return err;
}

}