#include "media/audio_render.h"
#include "base/logging.h"

namespace media {
#define AO_CHANNEL_ID 0

RKAudioRender::RKAudioRender()
    : initialize_(false) {}

RKAudioRender::~RKAudioRender() {
  UnInit();
}

bool RKAudioRender::Init(const std::string &device_name,
                         int sample_rate,
                         int channels,
                         int64_t nb_samples) {
  if (initialize_)
    return true;

  AO_CHN_ATTR_S ao_chn_attr_s;
  memset(&ao_chn_attr_s, 0, sizeof(AO_CHN_ATTR_S));
  ao_chn_attr_s.pcAudioNode = (char *) device_name.c_str();
  ao_chn_attr_s.enSampleFormat = RK_SAMPLE_FMT_S16;
  ao_chn_attr_s.u32NbSamples = nb_samples;
  ao_chn_attr_s.u32Channels = channels;
  ao_chn_attr_s.u32SampleRate = sample_rate;
  int ret = RK_MPI_AO_SetChnAttr(AO_CHANNEL_ID, &ao_chn_attr_s);
  if (ret) {
    LOG(ERROR) << "RK_MPI_AO_SetChnAttr failed:" << ret;
    return false;
  }
  ret = RK_MPI_AO_EnableChn(AO_CHANNEL_ID);
  if (ret) {
    LOG(ERROR) << "RK_MPI_AO_EnableChn failed:" << ret;
    return false;
  }
  initialize_ = true;
  return true;
}

void RKAudioRender::UnInit() {
  if (initialize_) {
    RK_MPI_AO_ClearChnBuf(AO_CHANNEL_ID);
    RK_MPI_AO_DisableChn(AO_CHANNEL_ID);
    initialize_ = false;
  }
}

bool RKAudioRender::SetVolume(int volume) const {
  if (!initialize_)
    return false;
  int ret = RK_MPI_AO_SetVolume(AO_CHANNEL_ID, volume);
  return ret == 0;
}

int RKAudioRender::SendInput(MEDIA_BUFFER mb) const {
  if (!initialize_)
    return -1;
  return RK_MPI_SYS_SendMediaBuffer(RK_ID_AO, AO_CHANNEL_ID, mb);
}

}