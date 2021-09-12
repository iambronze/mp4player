// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_INCOMING_TASK_QUEUE_H_
#define BASE_MESSAGE_LOOP_INCOMING_TASK_QUEUE_H_

#include "base/macros.h"
#include "base/pending_task.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/pending_task.h"

namespace base {

class MessageLoop;

class IncomingTaskQueue {
public:
 explicit IncomingTaskQueue(MessageLoop *message_loop);

 virtual ~IncomingTaskQueue();

 bool AddToIncomingQueue(std::unique_ptr<QueuedTask> task,
                         TimeDelta delay);

 void ReloadWorkQueue(TaskQueue *work_queue);

 void WillDestroyCurrentMessageLoop();

 void StartScheduling();

private:

 static TimeTicks CalculateDelayedRuntime(TimeDelta delay);

 bool PostPendingTask(std::unique_ptr<PendingTask> task);

 void ScheduleWork();

 MessageLoop *message_loop_;

 bool message_loop_scheduled_;

 bool is_ready_for_scheduling_;

 base::Lock incoming_queue_lock_;

 TaskQueue incoming_queue_;

 DISALLOW_COPY_AND_ASSIGN(IncomingTaskQueue);
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_INCOMING_TASK_QUEUE_H_
