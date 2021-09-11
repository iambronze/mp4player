// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <errno.h>
#include <sched.h>
#include <stddef.h>

#include "base/logging.h"
#include "base/threading/platform_thread_internal_posix.h"

#include <pthread.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>

namespace base {

namespace internal {

namespace {
const struct sched_param kRealTimePrio = {8};
}  // namespace

const ThreadPriorityToNiceValuePair kThreadPriorityToNiceValueMap[4] = {
    {ThreadPriority::BACKGROUND, 10},
    {ThreadPriority::NORMAL, 0},
    {ThreadPriority::DISPLAY, -8},
    {ThreadPriority::REALTIME_AUDIO, -10},
};

bool SetCurrentThreadPriorityForPlatform(ThreadPriority priority) {
  return priority == ThreadPriority::REALTIME_AUDIO &&
      pthread_setschedparam(pthread_self(), SCHED_RR, &kRealTimePrio) == 0;
}

bool GetCurrentThreadPriorityForPlatform(ThreadPriority *priority) {
  int maybe_sched_rr = 0;
  struct sched_param maybe_realtime_prio = {0};
  if (pthread_getschedparam(pthread_self(), &maybe_sched_rr,
                            &maybe_realtime_prio) == 0 &&
      maybe_sched_rr == SCHED_RR &&
      maybe_realtime_prio.sched_priority == kRealTimePrio.sched_priority) {
    *priority = ThreadPriority::REALTIME_AUDIO;
    return true;
  }
  return false;
}

}  // namespace internal

// static
void PlatformThread::SetName(const std::string &name) {
  // On linux we can get the thread names to show up in the debugger by setting
  // the process name for the LWP.  We don't want to do this for the main
  // thread because that would rename the process, causing tools like killall
  // to stop working.
  if (PlatformThread::CurrentId() == getpid())
    return;

  // http://0pointer.de/blog/projects/name-your-threads.html
  // Set the name for the LWP (which gets truncated to 15 characters).
  // Note that glibc also has a 'pthread_setname_np' api, but it may not be
  // available everywhere and it's only benefit over using prctl directly is
  // that it can set the name of threads other than the current thread.
  int err = prctl(PR_SET_NAME, name.c_str());
  // We expect EPERM failures in sandboxed processes, just ignore those.
  if (err < 0 && errno != EPERM)
    DPLOG(ERROR) << "prctl(PR_SET_NAME)";
}

size_t GetDefaultThreadStackSize(const pthread_attr_t &attributes) {
  // ThreadSanitizer bloats the stack heavily. Evidence has been that the
  // default stack size isn't enough for some browser tests.
  return 2 * (1 << 23);  // 2 times 8192K (the default stack size on Linux).
}

}  // namespace base
