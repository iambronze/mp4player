#include "media/audio_frame_queue.h"
#include "media/ffmpeg_common.h"
#include "base/logging.h"

namespace media {

AudioFrameQueue::AudioFrameQueue(AVStream *stream, size_t max_size)
    : stream_(stream),
      max_size_(max_size) {}

AudioFrameQueue::~AudioFrameQueue() {
  flush();
}

bool AudioFrameQueue::is_writable() {
  base::AutoLock l(lock_);
  return frame_list_.size() < max_size_;
}

int64_t AudioFrameQueue::startTimestamp() {
  base::AutoLock l(lock_);
  if (frame_list_.empty())
    return AV_NOPTS_VALUE;
  MEDIA_BUFFER mb = frame_list_.front();
  return static_cast<int64_t>(RK_MPI_MB_GetTimestamp(mb));
}

void AudioFrameQueue::put(MEDIA_BUFFER mb) {
  base::AutoLock l(lock_);
  frame_list_.push(mb);
}

MEDIA_BUFFER AudioFrameQueue::get(int64_t render_time) {
  base::AutoLock l(lock_);
  if (frame_list_.empty())
    return nullptr;

  MEDIA_BUFFER mb = frame_list_.front();
  auto pts = static_cast<int64_t>(RK_MPI_MB_GetTimestamp(mb));
  base::TimeDelta timestamp = media::ConvertFromTimeBase(stream_->time_base, pts);
  if (timestamp.InMicroseconds() <= render_time) {
    frame_list_.pop();
    DLOG(INFO) << "AudioFrameQueue size: " << frame_list_.size();
    return mb;
  }
  return nullptr;
}

void AudioFrameQueue::flush() {
  base::AutoLock l(lock_);
  while (!frame_list_.empty()) {
    MEDIA_BUFFER mb = frame_list_.front();
    frame_list_.pop();
    RK_MPI_MB_ReleaseBuffer(mb);
  }
}

size_t AudioFrameQueue::size() {
  base::AutoLock l(lock_);
  return frame_list_.size();
}

size_t AudioFrameQueue::max_size() const {
  return max_size_;
}
}
