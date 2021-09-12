#include "base/logging.h"
#include "media/video_player.h"
#include "media/mp4_dataset.h"
#include "media/audio_render.h"
#include "media/mpp_decoder.h"
#include "media/audio_frame_queue.h"
#include "media/video_frame_queue.h"
#include "media/packet_queue.h"
#include "media/audio_decoder_thread.h"
#include "media/video_decoder_thread.h"
#include "media/media_constants.h"
#include <functional>

namespace media {

VideoPlayer::VideoPlayer(Delegate *delegate,
                         Mp4Dataset *dataset,
                         bool enable_audio,
                         int volume,
                         bool loop,
                         double buffer_time)
    : delegate_(delegate),
      dataset_(dataset),
      enable_audio_(enable_audio),
      volume_(volume),
      loop_(loop),
      buffer_time_(buffer_time),
      mute_(false),
      thread_(new base::Thread("VideoPlayer")) {
  if (buffer_time_ < 0.2) buffer_time_ = 0.2;
  base::SimpleThread::Options options;
  options.set_priority(base::ThreadPriority::REALTIME_AUDIO);
  thread_->StartWithOptions(options);
  thread_->PostTask(std::bind(&VideoPlayer::OnStart, this));
}

VideoPlayer::~VideoPlayer() {
  if (thread_) {
    thread_->PostTask(std::bind(&VideoPlayer::OnStop, this));
    thread_->Stop();
    thread_.reset();
  }
}

void VideoPlayer::Seek(double timestamp) {
  if (!thread_->IsCurrent()) {
    thread_->PostTask(std::bind(&VideoPlayer::Seek, this, timestamp));
  } else {
    int err = dataset_->seek(timestamp);
    if (err >= 0) {
      /*
       * seek flow:
       * 1)stop render timer
       * 2)clear all output buffer
       * 3)add stream pending counter
       * 4)waiting decoder thread flush decoder
       * 5)restart render
       */
      io_timer_->Stop();

      if (video_output_queue_) {
        //flush很重要,万一decoder thread 阻塞,清空缓冲区就可以立即唤醒
        video_output_queue_->flush();
        ++render_state_.stream_seek_pending;
      }
      if (audio_output_queue_) {
        //flush很重要,万一decoder thread 阻塞,清空缓冲区就可以立即唤醒
        audio_output_queue_->flush();
        ++render_state_.stream_seek_pending;
      }
    } else {
      OnMediaError(Error_SeekFailed);
    }
  }
}

void VideoPlayer::SetVolume(int volume) {
  if (!thread_->IsCurrent()) {
    thread_->PostTask(std::bind(&VideoPlayer::SetVolume, this, volume));
  } else {
    if (audio_render_) {
      audio_render_->SetVolume(volume);
    }
  }
}

void VideoPlayer::Mute(bool enable) {
  if (!thread_->IsCurrent()) {
    thread_->PostTask(std::bind(&VideoPlayer::Mute, this, enable));
  } else {
    mute_ = enable;
  }
}

void VideoPlayer::Pause() {
  if (!thread_->IsCurrent()) {
    thread_->PostTask(std::bind(&VideoPlayer::Pause, this));
  } else {
    io_timer_->Stop();
  }
}

void VideoPlayer::Resume() {
  if (!thread_->IsCurrent()) {
    thread_->PostTask(std::bind(&VideoPlayer::Resume, this));
  } else {
    /*
     * 这里需要注意,因为我们采用当前时间来修正定时器的误差,所以,在resume之后,基准时间已经不准了
     * 所以,需要修正: 用当前时间减去已经播放的时间
     */
    render_state_.BasetimeCalibration();
    OnRender();
  }
}

void VideoPlayer::OnStart() {
  InitVideo();
  InitAudio();
  InitAudioRender();
  io_timer_.reset(new base::Timer(false));
  ManageTimer(base::TimeDelta::FromMicroseconds(kRenderPollDelay));
}

void VideoPlayer::OnStop() {
  io_timer_.reset();
  //在销毁解码线程之前,先让UI线程释放 mppframe,否则会导致RK解码器异常
  delegate_->OnMediaFrameArrival(nullptr);

  video_decoder_thread_.reset();
  audio_decoder_thread_.reset();
  audio_render_.reset();
  audio_input_queue_.reset();
  video_input_queue_.reset();
  video_output_queue_.reset();
  audio_output_queue_.reset();
  delegate_->OnMediaStop();
}

void VideoPlayer::InitAudio() {
  AVStream *stream = dataset_->getAudioStream();

  if (!enable_audio_ || !stream) {
    LOG(WARNING) << "No audio to play or disable play audio";
    return;
  }

  //根据帧时长估计缓冲区大小
  base::TimeDelta duration = media::ConvertFromTimeBase(stream->time_base, stream->codecpar->frame_size);
  int count = static_cast<int>(buffer_time_ / duration.InSecondsF()) + 1;
  LOG(INFO) << "audio max buffer count:" << count;
  audio_output_queue_ = base::WrapUnique(new AudioFrameQueue(stream, count));
  audio_input_queue_ = base::WrapUnique(new PacketQueue());
  dataset_->setAudioPacketQueue(audio_input_queue_.get());
  audio_decoder_thread_ = base::WrapUnique(new AudioDecoderThread(this,
                                                                  dataset_,
                                                                  audio_input_queue_.get(),
                                                                  audio_output_queue_.get()));
}

void VideoPlayer::InitVideo() {
  AVStream *stream = dataset_->getVideoStream();
  AVRational frame_rate = av_guess_frame_rate(dataset_->getFormatContext(), stream, nullptr);
  double fps = frame_rate.num && frame_rate.den ? av_q2d(frame_rate) : 30.0f;
  int count = static_cast<int>(fps * buffer_time_);
  LOG(INFO) << "video max buffer count:" << count;
  video_output_queue_ = base::WrapUnique(new VideoFrameQueue(stream, count));
  video_input_queue_ = base::WrapUnique(new PacketQueue());
  dataset_->setVideoPacketQueue(video_input_queue_.get());
  video_decoder_thread_ = base::WrapUnique(new VideoDecoderThread(this,
                                                                  dataset_,
                                                                  video_input_queue_.get(),
                                                                  video_output_queue_.get()));
}

void VideoPlayer::InitAudioRender() {
  if (!audio_output_queue_)
    return;

  std::unique_ptr<RKAudioRender> audio_render;
  auto stream = dataset_->getAudioStream();
  int sample_rate = stream->codecpar->sample_rate;
  int64_t frame_size = stream->codecpar->frame_size;
  if (frame_size < 1) frame_size = 1024;
  std::unique_ptr<RKAudioRender> render = base::WrapUnique(new RKAudioRender());
  if (!render->Init("default",
                    sample_rate,
                    kAudioChannels,
                    frame_size * kAudioChannels)) {
    render.reset();
    return;
  }
  if (volume_ >= 0) {
    render->SetVolume(volume_);
  }
  audio_render_ = std::move(render);
}

void VideoPlayer::OnRender() {
  if (!render_state_.started) {
    //我们要缓冲指定时间的视频帧,一是为了后面播放更为流畅,二是如果存在B帧,需要缓冲排序
    if (video_output_queue_->is_writable()) {
      ManageTimer(base::TimeDelta::FromMicroseconds(kRenderPollDelay));
      return;
    }
    render_state_.started = true;
    DLOG(INFO) << "render started";
  }

  if (render_state_.render_time == AV_NOPTS_VALUE) {
    /*
     * 正常开始的播放,或者重新播放,或者 seek 之后播放
     * 我们要重新初始化播放状态
     * 我们要根据视频缓冲区第一帧的时间戳来作为当前播放时间戳,在seek之后,第一帧时间戳不一定为 0
     * 我们播放时间戳要比第一帧时间戳略大
     */
    int64_t timestamp = video_output_queue_->startTimestamp();
    int64_t remainder = timestamp % kRenderPollDelay;
    //转换为 kRenderPollDelay 的整数倍
    render_state_.render_time = (timestamp / kRenderPollDelay) * kRenderPollDelay;
    if (remainder > 0) render_state_.render_time += kRenderPollDelay;
    //重新校准基准时间: 当前时间 - 已经播放的时间 (这里指 seek 之后的时间)
    render_state_.BasetimeCalibration();
    DLOG(INFO) << "render timer reset";
  }

  if (audio_render_) {
    while (true) {
      MEDIA_BUFFER audio_buffer = audio_output_queue_->get(render_state_.render_time);
      if (!audio_buffer)
        break;
      DLOG(INFO) << "Render Audio frame PTS:" << RK_MPI_MB_GetTimestamp(audio_buffer);
      if (mute_) {
        memset(RK_MPI_MB_GetPtr(audio_buffer), 0, RK_MPI_MB_GetSize(audio_buffer));
      }
      audio_render_->SendInput(audio_buffer);
      RK_MPI_MB_ReleaseBuffer(audio_buffer);
    }
  }

  bool eos_reached = false;

  while (true) {
    MppFrame video_frame = video_output_queue_->get(render_state_.render_time);
    if (!video_frame)
      break;
    int64_t pts = mpp_frame_get_pts(video_frame);
    DLOG(INFO) << "Render Video frame PTS:" << pts;
    if (mpp_frame_get_eos(video_frame)) {
      eos_reached = true;
      mpp_frame_deinit(&video_frame);
    } else {
      delegate_->OnMediaFrameArrival(video_frame);
    }
  }
  //next render time
  render_state_.render_time += kRenderPollDelay;

  if (!eos_reached) {
    //这里要对定时器进行误差修正,尽力保证实际间隔在 kRenderPollDelay
    base::TimeTicks
        expire_time = render_state_.base_time + base::TimeDelta::FromMicroseconds(render_state_.render_time);
    base::TimeTicks now = base::TimeTicks::Now();
    if (expire_time > now) {
      base::TimeDelta delay = expire_time - now;
      DLOG(INFO) << "render delay: " << delay.InMicroseconds();
      ManageTimer(delay);
    } else {
      ManageTimer(base::TimeDelta::FromMicroseconds(1000));
    }
  } else {
    //播放完成
    render_state_.Reset();
    RenderCompleted();
    if (!loop_) {
      delegate_->OnMediaStop();
    } else {
      RewindRender();
    }
  }
}

void VideoPlayer::RenderCompleted() {
  if (audio_input_queue_) {
    audio_input_queue_->flush();
  }
  if (video_input_queue_) {
    video_input_queue_->flush();
  }
  if (audio_output_queue_) {
    audio_output_queue_->flush();
  }
  if (video_output_queue_) {
    video_output_queue_->flush();
  }
}

void VideoPlayer::RewindRender() {
  dataset_->rewind();
  ManageTimer(base::TimeDelta::FromMicroseconds(kRenderPollDelay));
}

void VideoPlayer::ManageTimer(const base::TimeDelta &delay) {
  io_timer_->Start(std::bind(&VideoPlayer::OnRender, this), delay);
}

void VideoPlayer::OnFlushCompleted(int stream_idx) {
  if (!thread_->IsCurrent()) {
    thread_->PostTask(std::bind(&VideoPlayer::OnFlushCompleted, this, stream_idx));
  } else {
    if (render_state_.stream_seek_pending < 1)
      return; //当前没有seek

    --render_state_.stream_seek_pending;
    if (render_state_.stream_seek_pending == 0) {
      render_state_.Reset();
      OnRender();
    }
  }
}

void VideoPlayer::OnMediaError(int err) {
  delegate_->OnMediaError(err);
}
}