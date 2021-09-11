#ifndef MEDIA_AUDIO_FRAME_QUEUE_H_
#define MEDIA_AUDIO_FRAME_QUEUE_H_

#include <queue>
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include <rkmedia/rkmedia_api.h>

struct AVStream;

namespace media {

class AudioFrameQueue {
public:
 explicit AudioFrameQueue(AVStream *stream, size_t max_size);

 virtual ~AudioFrameQueue();

 bool is_writable();

 int64_t startTimestamp();

 void put(MEDIA_BUFFER mb);

 MEDIA_BUFFER get(int64_t render_time);

 void flush();

 size_t size();

 size_t max_size() const;

private:
 AVStream *stream_;
 size_t max_size_;
 std::queue<MEDIA_BUFFER> frame_list_;
 base::Lock lock_;
 DISALLOW_COPY_AND_ASSIGN(AudioFrameQueue);
};
}

#endif //MEDIA_AUDIO_FRAME_QUEUE_H_
