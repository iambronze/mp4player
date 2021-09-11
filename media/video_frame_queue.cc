#include "base/logging.h"
#include "media/video_frame_queue.h"
#include "media/ffmpeg_common.h"
#include "base/logging.h"

namespace media {
VideoFrameQueue::VideoFrameQueue(AVStream *stream, size_t max_size)
    : stream_(stream),
      max_size_(max_size) {}

VideoFrameQueue::~VideoFrameQueue() {
  flush();
}

bool VideoFrameQueue::is_writable() {
  base::AutoLock l(lock_);
  return frame_list_.size() < max_size_;
}

int64_t VideoFrameQueue::startTimestamp() {
  base::AutoLock l(lock_);
  if (frame_list_.empty())
    return AV_NOPTS_VALUE;
  return frame_list_.begin()->first;
}

void VideoFrameQueue::put(MppFrame frame) {
  base::AutoLock l(lock_);
  int64_t pts = mpp_frame_get_pts(frame);
  auto iter = frame_list_.find(pts);
  if (iter != frame_list_.end()) {
    //可能存在 PTS 重复,我们把早期的销毁,保存后来的帧
    mpp_frame_deinit(&iter->second);
    frame_list_.erase(iter);
  }
  frame_list_.insert(std::make_pair(pts, frame));
}

MppFrame VideoFrameQueue::get(int64_t render_time) {
  base::AutoLock l(lock_);
  if (frame_list_.empty())
    return nullptr;

  if (frame_list_.begin()->first <= render_time) {
    MppFrame frame = frame_list_.begin()->second;
    frame_list_.erase(frame_list_.begin());
    DLOG(INFO) << "VideoFrameQueue size: " << frame_list_.size();
    return frame;
  }
  if (frame_list_.size() == 1) {
    MppFrame frame = frame_list_.begin()->second;
    if (mpp_frame_get_eos(frame)) {
      frame_list_.clear();
      return frame;
    }
  }
  return nullptr;
}

void VideoFrameQueue::flush() {
  base::AutoLock l(lock_);
  for (auto &i : frame_list_) {
    mpp_frame_deinit(&i.second);
  }
  frame_list_.clear();
}

size_t VideoFrameQueue::size() {
  base::AutoLock l(lock_);
  return frame_list_.size();
}

size_t VideoFrameQueue::max_size() const {
  return max_size_;
}
}
