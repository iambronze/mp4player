#ifndef MEDIA_AUDIO_RESAMPLER_H_
#define MEDIA_AUDIO_RESAMPLER_H_

#include <stdint.h>
#include "base/macros.h"
#include "media/ffmpeg_common.h"

namespace media {
class FFmpegAudioResampler {
public:
 FFmpegAudioResampler();
 virtual ~FFmpegAudioResampler();

 bool Init(AVCodecContext *codec_context,
           int out_sample_rate = 0,//use input samplerate
           int out_channels = 2);

 void UnInit();

 int Resample(const uint8_t **data,
              int src_nb_samples,
              uint8_t ***output_samples);

private:
 void ReleaseBuffer();
 int64_t max_dst_nb_samples_;
 uint8_t **converted_input_samples_;
 int in_sample_rate_;
 int in_channels_;
 int out_sample_rate_;
 int out_channels_;
 SwrContext *resampler_context_;
 DISALLOW_COPY_AND_ASSIGN(FFmpegAudioResampler);
};
}
#endif  // MEDIA_AUDIO_RESAMPLER_H_
