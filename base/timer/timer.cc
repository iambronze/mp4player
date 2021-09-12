#include <memory>
#include "base/timer/timer.h"
#include "base/threading/thread.h"
#include "base/message_loop/message_loop.h"
#include <event2/event.h>

namespace base {

Timer::Timer(bool is_repeating)
    : is_repeating_(is_repeating),
      event_(nullptr) {
}

Timer::~Timer() {
  Stop();
  if (event_) {
    event_free(event_);
    event_ = nullptr;
  }
}

void Timer::Start(std::unique_ptr<QueuedTask> task,
                  const TimeDelta &delay) {
  Stop();

  struct event_base *ev = MessageLoop::current()->base();
  if (!ev) return;

  int event_mask = is_repeating_ ? EV_PERSIST : 0;

  if (!event_) {
    event_ = event_new(ev, -1, EV_TIMEOUT | event_mask, &Timer::RunTimer, this);
  } else {
    event_assign(event_, ev, -1, EV_TIMEOUT | event_mask, &Timer::RunTimer, this);
  }
  timeval tv = {static_cast<__time_t>(delay.InSeconds()),
                static_cast<__suseconds_t>(delay.InMicroseconds() % Time::kMicrosecondsPerSecond)};
  event_add(event_, &tv);
  task_ = std::move(task);
}

void Timer::Stop() {
  if (event_) {
    event_del(event_);
  }
  task_.reset();
}

void Timer::RunTimer(evutil_socket_t listener, short event, void *arg) {
  auto timer = reinterpret_cast<Timer *>(arg);
  timer->task_->Run();
}
}  // namespace base
