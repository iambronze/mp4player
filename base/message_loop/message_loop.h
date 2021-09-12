// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_LOOP_H_
#define BASE_MESSAGE_LOOP_MESSAGE_LOOP_H_

#include <queue>
#include <list>
#include "base/time/time.h"
#include "base/callback.h"
#include "base/pending_task.h"

extern "C" {
#include <event2/util.h>
}

struct event_base;
struct event;

namespace base {
class IncomingTaskQueue;

class MessageLoop {
public:
 MessageLoop();
 virtual ~MessageLoop();

 void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                      TimeDelta delay);
 void Run();

 void QuitNow();

 void ScheduleWork();

 static MessageLoop *current();

 event_base *base();

private:
 friend class Thread;

 class DelayedTask;

 bool Init();

 void BindToCurrentThread();

 void ReloadWorkQueue(TaskQueue *work_queue);

 void AddToDelayedWorkQueue(std::unique_ptr<PendingTask> task);

 static void OnWakeup(evutil_socket_t socket, short flags, void *context);

 static void RunTimer(evutil_socket_t socket, short flags, void *context);

 event_base *event_base_;

 evutil_socket_t wakeup_pipe_in_;

 evutil_socket_t wakeup_pipe_out_;

 event *wakeup_event_;

 bool keep_running_;

 std::list<DelayedTask *> delayed_tasks_;

 std::unique_ptr<IncomingTaskQueue> incoming_task_queue_;

 DISALLOW_COPY_AND_ASSIGN(MessageLoop);
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_LOOP_H_
