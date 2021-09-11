#ifndef BASE_SYNCHRONIZATION_WAITABLE_EVENT_H_
#define BASE_SYNCHRONIZATION_WAITABLE_EVENT_H_

#include <cstddef>
#include <pthread.h>
#include "base/macros.h"
#include "base/time/time.h"

namespace base {

class WaitableEvent {
public:
 explicit WaitableEvent(bool manual_reset, bool initially_signaled);

 ~WaitableEvent();

 void Reset();

 void Signal();

 void Wait();

 bool IsSignaled();

 bool TimedWait(const TimeDelta &wait_delta);

 bool TimedWait(int64_t milliseconds);

private:

 static const int kForever = -1;

 pthread_mutex_t event_mutex_{};
 pthread_cond_t event_cond_{};
 const bool is_manual_reset_;
 bool event_status_;
 DISALLOW_COPY_AND_ASSIGN(WaitableEvent);
};
}  // namespace base
#endif  // BASE_SYNCHRONIZATION_WAITABLE_EVENT_H_