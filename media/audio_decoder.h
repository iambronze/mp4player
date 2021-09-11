#ifndef MEDIA_AUDIO_DECODER_H_
#define MEDIA_AUDIO_DECODER_H_

#include <stdint.h>
#include <memory>
#include "base/macros.h"
#include "base/time/time.h"
#include "media/ffmpeg_common.h"

namespace media {
class FFmpegAudioDecoder {
public:
 FFmpegAudioDecoder();
 virtual ~FFmpegAudioDecoder();

 bool Init(AVStream *stream);

 void UnInit();

 int SendInput(const AVPacket *pkt) const;

 int FetchOutput(AVFrame *frame) const;

 void Flush() const;

 AVCodecContext *codec_context();

private:
 std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context_;
 DISALLOW_COPY_AND_ASSIGN(FFmpegAudioDecoder);
};
}
#endif  // MEDIA_AUDIO_DECODER_H_
