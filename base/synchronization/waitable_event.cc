#include "base/synchronization/waitable_event.h"
#include <pthread.h>
#include <ctime>

namespace base {

namespace {

timespec GetTimespec(const int64_t milliseconds_from_now) {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  ts.tv_sec += (milliseconds_from_now / 1000);
  ts.tv_nsec += (milliseconds_from_now % 1000) * 1000000;

  if (ts.tv_nsec >= 1000000000) {
    ts.tv_sec++;
    ts.tv_nsec -= 1000000000;
  }
  return ts;
}
}  // namespace

WaitableEvent::WaitableEvent(bool manual_reset, bool initially_signaled)
    : is_manual_reset_(manual_reset),
      event_status_(initially_signaled) {
  pthread_mutex_init(&event_mutex_, nullptr);
  pthread_condattr_t cond_attr;
  pthread_condattr_init(&cond_attr);
  pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC);
  pthread_cond_init(&event_cond_, &cond_attr);
  pthread_condattr_destroy(&cond_attr);
}

WaitableEvent::~WaitableEvent() {
  pthread_mutex_destroy(&event_mutex_);
  pthread_cond_destroy(&event_cond_);
}

void WaitableEvent::Reset() {
  pthread_mutex_lock(&event_mutex_);
  event_status_ = false;
  pthread_mutex_unlock(&event_mutex_);
}

void WaitableEvent::Signal() {
  pthread_mutex_lock(&event_mutex_);
  event_status_ = true;
  pthread_cond_broadcast(&event_cond_);
  pthread_mutex_unlock(&event_mutex_);
}

bool WaitableEvent::IsSignaled() {
  pthread_mutex_lock(&event_mutex_);
  bool status = event_status_;
  pthread_mutex_unlock(&event_mutex_);
  return status;
}

void WaitableEvent::Wait() {
  TimedWait(kForever);
}

bool WaitableEvent::TimedWait(const TimeDelta &wait_delta) {
  int timeout_ms = static_cast<int>(wait_delta.InMilliseconds());
  return TimedWait(timeout_ms);
}

bool WaitableEvent::TimedWait(int64_t milliseconds) {
  int error = 0;
  struct timespec ts{};
  if (milliseconds != kForever) {
    ts = GetTimespec(milliseconds);
  }
  pthread_mutex_lock(&event_mutex_);

  if (milliseconds != kForever) {
    while (!event_status_ && error == 0) {
      error = pthread_cond_timedwait(&event_cond_, &event_mutex_, &ts);
    }
  } else {
    while (!event_status_ && error == 0)
      error = pthread_cond_wait(&event_cond_, &event_mutex_);
  }

  if (error == 0 && !is_manual_reset_)
    event_status_ = false;

  pthread_mutex_unlock(&event_mutex_);
  return (error == 0);
}

}  // namespace base
