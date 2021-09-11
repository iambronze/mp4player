#ifndef MEDIA_VIDEO_PLAYER_H_
#define MEDIA_VIDEO_PLAYER_H_

#include <memory>
#include "base/macros.h"
#include "base/threading/thread.h"
#include "base/synchronization/lock.h"
#include "base/timer/timer.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "media/ffmpeg_common.h"
#include <rkmedia/rkmedia_api.h>
#include <rockchip/mpp_frame.h>

namespace media {
class Mp4Dataset;
class PacketQueue;
class VideoFrameQueue;
class AudioFrameQueue;
class RKAudioRender;
class AudioDecoderThread;
class VideoDecoderThread;

enum MediaError {
  Error_VideoCodecUnsupported,
  Error_VideoCodecCreateFailed,
  Error_AudioResamplerCreateFailed,
  Error_AudioCodecCreateFailed,
  Error_SeekFailed
};

class VideoPlayer{
public:
 class Delegate {
 public:
  virtual ~Delegate() {}
  virtual void OnMediaError(int err) = 0;
  virtual void OnMediaStop() = 0;
  virtual void OnMediaFrameArrival(MppFrame mb) = 0;
 protected:
  Delegate() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(Delegate);
 };
 explicit VideoPlayer(Delegate *delegate,
                      Mp4Dataset *dataset,
                      bool enable_audio,
                      int volume,
                      bool loop,
                      double buffer_time);

 virtual ~VideoPlayer();

 void Seek(double timestamp);

 void SetVolume(int volume);

 void Mute(bool enable);

 void Pause();

 void Resume();

protected:
 Delegate *delegate() {
   return delegate_;
 }

 void OnFlushCompleted(int stream_idx);

 void OnMediaError(int err);

private:
 friend class VideoDecoderThread;
 friend class AudioDecoderThread;

 void OnStart();

 void OnStop();

 void InitAudio();

 void InitVideo();

 void InitAudioRender();

 void OnRender();

 void ManageTimer(const base::TimeDelta &delay);

 void RenderCompleted();

 void RewindRender();

 Delegate *delegate_;

 Mp4Dataset *dataset_;

 bool enable_audio_;

 int volume_;

 bool loop_;

 double buffer_time_;

 bool mute_;

 struct RenderState {
   bool started;
   int64_t render_time;
   base::TimeTicks base_time; //基准时间,用来消除定时器误差
   int stream_seek_pending;
   RenderState() {
     Reset();
   }
   void Reset() {
     started = false;
     render_time = AV_NOPTS_VALUE;
     base_time = base::TimeTicks();
     stream_seek_pending = 0;
   }

   void BasetimeCalibration() {
     base_time = base::TimeTicks::Now() - base::TimeDelta::FromMicroseconds(render_time);
   }
 };

 RenderState render_state_;

 std::unique_ptr<base::Timer> io_timer_;

 std::unique_ptr<AudioDecoderThread> audio_decoder_thread_;

 std::unique_ptr<VideoDecoderThread> video_decoder_thread_;

 std::unique_ptr<RKAudioRender> audio_render_;

 std::unique_ptr<PacketQueue> video_input_queue_;

 std::unique_ptr<PacketQueue> audio_input_queue_;

 std::unique_ptr<VideoFrameQueue> video_output_queue_;

 std::unique_ptr<AudioFrameQueue> audio_output_queue_;

 std::unique_ptr<base::Thread> thread_;
 DISALLOW_COPY_AND_ASSIGN(VideoPlayer);
};
}
#endif  // MEDIA_VIDEO_PLAYER_H_
