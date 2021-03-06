#ifndef BASE_TIMER_TIMER_H_
#define BASE_TIMER_TIMER_H_

#include <string>
#include "base/macros.h"
#include "base/time/time.h"
#include "base/callback.h"

extern "C" {
#include <event2/util.h>
}

struct event;

namespace base {

class Timer {
public:
 explicit Timer(bool is_repeating);

 virtual ~Timer();

 void Start(std::unique_ptr<QueuedTask> task,
            const TimeDelta &delay);

 template<class Closure,
     typename std::enable_if<!std::is_convertible<
         Closure,
         std::unique_ptr<QueuedTask>>::value>::type * = nullptr>
 void Start(Closure &&closure, const base::TimeDelta &delay) {
   Start(NewClosure(std::forward<Closure>(closure)), delay);
 }

 void Stop();

private:

 static void RunTimer(evutil_socket_t listener, short event, void *arg);

 bool is_repeating_;

 struct event *event_;

 std::unique_ptr<QueuedTask> task_;
 DISALLOW_COPY_AND_ASSIGN(Timer);
};
}  // namespace base

#endif  // BASE_TIMER_TIMER_H_
