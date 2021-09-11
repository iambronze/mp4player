// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <memory>

#include "base/logging.h"
#include "base/threading/platform_thread_internal_posix.h"

#include <sys/syscall.h>

namespace base {

size_t GetDefaultThreadStackSize(const pthread_attr_t &attributes);

namespace {

struct ThreadParams {
  ThreadParams()
      : delegate(NULL), joinable(false), priority(ThreadPriority::NORMAL) {}

  PlatformThread::Delegate *delegate;
  bool joinable;
  ThreadPriority priority;
};

void *ThreadFunc(void *params) {
  PlatformThread::Delegate *delegate = nullptr;

  {
    std::unique_ptr<ThreadParams> thread_params(static_cast<ThreadParams *>(params));

    delegate = thread_params->delegate;

    // Threads on linux/android may inherit their priority from the thread
    // where they were created. This explicitly sets the priority of all new
    // threads.
    PlatformThread::SetCurrentThreadPriority(thread_params->priority);
  }

  delegate->ThreadMain();
  return NULL;
}

bool CreateThread(size_t stack_size,
                  bool joinable,
                  PlatformThread::Delegate *delegate,
                  PlatformThreadHandle *thread_handle,
                  ThreadPriority priority) {
  DCHECK(thread_handle);

  pthread_attr_t attributes;
  pthread_attr_init(&attributes);

  // Pthreads are joinable by default, so only specify the detached
  // attribute if the thread should be non-joinable.
  if (!joinable)
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);

  // Get a better default if available.
  if (stack_size == 0)
    stack_size = base::GetDefaultThreadStackSize(attributes);

  if (stack_size > 0)
    pthread_attr_setstacksize(&attributes, stack_size);

  std::unique_ptr<ThreadParams> params(new ThreadParams);
  params->delegate = delegate;
  params->joinable = joinable;
  params->priority = priority;

  pthread_t handle;
  int err = pthread_create(&handle, &attributes, ThreadFunc, params.get());
  bool success = !err;
  if (success) {
    // ThreadParams should be deleted on the created thread after used.
    ignore_result(params.release());
  } else {
    // Value of |handle| is undefined if pthread_create fails.
    handle = 0;
    errno = err;
    PLOG(ERROR) << "pthread_create";
  }
  *thread_handle = PlatformThreadHandle(handle);

  pthread_attr_destroy(&attributes);

  return success;
}

}  // namespace

// static
PlatformThreadId PlatformThread::CurrentId() {
  // Pthreads doesn't have the concept of a thread ID, so we have to reach down
  // into the kernel.
  return syscall(__NR_gettid);
}

// static
PlatformThreadRef PlatformThread::CurrentRef() {
  return PlatformThreadRef(pthread_self());
}

// static
PlatformThreadHandle PlatformThread::CurrentHandle() {
  return PlatformThreadHandle(pthread_self());
}

// static
void PlatformThread::YieldCurrentThread() {
  sched_yield();
}

// static
void PlatformThread::Sleep(TimeDelta duration) {
  struct timespec sleep_time, remaining;

  // Break the duration into seconds and nanoseconds.
  // NOTE: TimeDelta's microseconds are int64s while timespec's
  // nanoseconds are longs, so this unpacking must prevent overflow.
  sleep_time.tv_sec = duration.InSeconds();
  duration -= TimeDelta::FromSeconds(sleep_time.tv_sec);
  sleep_time.tv_nsec = duration.InMicroseconds() * 1000;  // nanoseconds

  while (nanosleep(&sleep_time, &remaining) == -1 && errno == EINTR)
    sleep_time = remaining;
}

// static
const char *PlatformThread::GetName() {
  return nullptr;
}

// static
bool PlatformThread::CreateWithPriority(size_t stack_size, Delegate *delegate,
                                        PlatformThreadHandle *thread_handle,
                                        ThreadPriority priority) {
  return CreateThread(stack_size, true,  // joinable thread
                      delegate, thread_handle, priority);
}

// static
bool PlatformThread::CreateNonJoinable(size_t stack_size, Delegate *delegate) {
  PlatformThreadHandle unused;

  bool result = CreateThread(stack_size, false /* non-joinable thread */,
                             delegate, &unused, ThreadPriority::NORMAL);
  return result;
}

// static
void PlatformThread::Join(PlatformThreadHandle thread_handle) {
  // Joining another thread may block the current thread for a long time, since
  // the thread referred to by |thread_handle| may still be running long-lived /
  // blocking tasks.
  CHECK_EQ(0, pthread_join(thread_handle.platform_handle(), NULL));
}

// static
void PlatformThread::SetCurrentThreadPriority(ThreadPriority priority) {
  if (internal::SetCurrentThreadPriorityForPlatform(priority))
    return;

  // setpriority(2) should change the whole thread group's (i.e. process)
  // priority. However, as stated in the bugs section of
  // http://man7.org/linux/man-pages/man2/getpriority.2.html: "under the current
  // Linux/NPTL implementation of POSIX threads, the nice value is a per-thread
  // attribute". Also, 0 is prefered to the current thread id since it is
  // equivalent but makes sandboxing easier (https://crbug.com/399473).
  const int nice_setting = internal::ThreadPriorityToNiceValue(priority);
  if (setpriority(PRIO_PROCESS, 0, nice_setting)) {
    DVPLOG(1) << "Failed to set nice value of thread ("
              << PlatformThread::CurrentId() << ") to " << nice_setting;
  }
}

// static
ThreadPriority PlatformThread::GetCurrentThreadPriority() {
  // Mirrors SetCurrentThreadPriority()'s implementation.
  ThreadPriority platform_specific_priority;
  if (internal::GetCurrentThreadPriorityForPlatform(
      &platform_specific_priority)) {
    return platform_specific_priority;
  }

  // Need to clear errno before calling getpriority():
  // http://man7.org/linux/man-pages/man2/getpriority.2.html
  errno = 0;
  int nice_value = getpriority(PRIO_PROCESS, 0);
  if (errno != 0) {
    DVPLOG(1) << "Failed to get nice value of thread ("
              << PlatformThread::CurrentId() << ")";
    return ThreadPriority::NORMAL;
  }

  return internal::NiceValueToThreadPriority(nice_value);
}
}  // namespace base
