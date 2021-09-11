#ifndef MEDIA_VIDEO_FRAME_QUEUE_H_
#define MEDIA_VIDEO_FRAME_QUEUE_H_

#include <map>
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "media/media_constants.h"
#include <rockchip/mpp_frame.h>

struct AVStream;

namespace media {

/*
 * 因为可能存在B帧,解码器出来的帧顺序不是真正的显示顺序,所以,
 * 我们要按照 PTS 进行排序,std::map可以对主键自动排序
 * 正是我们所需
 */
class VideoFrameQueue {
public:
 explicit VideoFrameQueue(AVStream *stream, size_t max_size);

 virtual ~VideoFrameQueue();

 bool is_writable();

 int64_t startTimestamp();

 void put(MppFrame frame);

 MppFrame get(int64_t render_time);

 void flush();

 size_t size();

 size_t max_size() const;

private:
 AVStream *stream_;
 size_t max_size_;
 std::map<int64_t, MppFrame> frame_list_;
 base::Lock lock_;
 DISALLOW_COPY_AND_ASSIGN(VideoFrameQueue);
};
}

#endif //MEDIA_VIDEO_FRAME_QUEUE_H_
