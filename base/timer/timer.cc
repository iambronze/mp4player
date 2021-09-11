#include <memory>
#include "base/timer/timer.h"
#include "base/threading/thread.h"

namespace base {

Timer::Timer(bool is_repeating)
    : is_repeating_(is_repeating),
      e_(nullptr) {}

Timer::~Timer() {
  Stop();
  if (e_) {
    event_free(e_);
    e_ = nullptr;
  }
}

void Timer::Start(std::unique_ptr<QueuedTask> task,
                  const TimeDelta &delay) {
  Stop();

  struct event_base *ev = Thread::Base();
  if (!ev) return;

  int event_mask = is_repeating_ ? EV_PERSIST : 0;

  if (!e_) {
    e_ = event_new(ev, -1, EV_TIMEOUT | event_mask, &Timer::RunTimer, this);
  } else {
    event_assign(e_, ev, -1, EV_TIMEOUT | event_mask, &Timer::RunTimer, this);
  }
  timeval tv = {static_cast<__time_t>(delay.InSeconds()),
                static_cast<__suseconds_t>(delay.InMicroseconds() % Time::kMicrosecondsPerSecond)};
  event_add(e_, &tv);
  task_ = std::move(task);
}

void Timer::Stop() {
  if (e_) {
    event_del(e_);
  }
  task_.reset();
}

void Timer::RunTimer(evutil_socket_t listener, short event, void *arg) {
  auto timer = reinterpret_cast<Timer *>(arg);
  timer->task_->Run();
}
}  // namespace base
