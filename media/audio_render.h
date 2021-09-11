#ifndef MEDIA_AUDIO_RENDER_H_
#define MEDIA_AUDIO_RENDER_H_

#include "base/macros.h"
#include <string>
#include <rkmedia/rkmedia_api.h>

namespace media {
class RKAudioRender {
public:
 RKAudioRender();
 virtual ~RKAudioRender();

 bool Init(const std::string &device_name,
           int sample_rate,
           int channels,
           int64_t nb_samples);

 void UnInit();

 bool SetVolume(int volume) const;

 int SendInput(MEDIA_BUFFER mb) const;
private:
 bool initialize_;
 DISALLOW_COPY_AND_ASSIGN(RKAudioRender);
};
}
#endif  // MEDIA_AUDIO_RENDER_H_
