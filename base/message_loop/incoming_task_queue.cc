// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/incoming_task_queue.h"
#include "base/message_loop/message_loop.h"

namespace base {

IncomingTaskQueue::IncomingTaskQueue(MessageLoop *message_loop)
    : message_loop_(message_loop),
      message_loop_scheduled_(false),
      is_ready_for_scheduling_(false) {
}

bool IncomingTaskQueue::AddToIncomingQueue(
    std::unique_ptr<QueuedTask> task,
    TimeDelta delay) {
  AutoLock locked(incoming_queue_lock_);
  std::unique_ptr<PendingTask> pending_task(
      new PendingTask(std::move(task), CalculateDelayedRuntime(delay)));
  return PostPendingTask(std::move(pending_task));
}

void IncomingTaskQueue::ReloadWorkQueue(TaskQueue *work_queue) {
  // Make sure no tasks are lost.
  DCHECK(work_queue->empty());

  // Acquire all we can from the inter-thread queue with one lock acquisition.
  AutoLock lock(incoming_queue_lock_);
  if (incoming_queue_.empty()) {
    // If the loop attempts to reload but there are no tasks in the incoming
    // queue, that means it will go to sleep waiting for more work. If the
    // incoming queue becomes nonempty we need to schedule it again.
    message_loop_scheduled_ = false;
  } else {
    incoming_queue_.Swap(work_queue);
  }
}

void IncomingTaskQueue::WillDestroyCurrentMessageLoop() {
  AutoLock lock(incoming_queue_lock_);
  message_loop_ = nullptr;
}

void IncomingTaskQueue::StartScheduling() {
  AutoLock lock(incoming_queue_lock_);
  DCHECK(!is_ready_for_scheduling_);
  DCHECK(!message_loop_scheduled_);
  is_ready_for_scheduling_ = true;
  if (!incoming_queue_.empty())
    ScheduleWork();
}

IncomingTaskQueue::~IncomingTaskQueue() {
  DCHECK(!message_loop_);
}

TimeTicks IncomingTaskQueue::CalculateDelayedRuntime(TimeDelta delay) {
  TimeTicks delayed_run_time;
  if (delay > TimeDelta())
    delayed_run_time = TimeTicks::Now() + delay;
  else DCHECK_EQ(delay.InMilliseconds(), 0) << "delay should not be negative";
  return delayed_run_time;
}

bool IncomingTaskQueue::PostPendingTask(std::unique_ptr<PendingTask> task) {
  if (!message_loop_) {
    task.reset();
    return false;
  }

  bool was_empty = incoming_queue_.empty();
  incoming_queue_.push(std::move(task));

  if (is_ready_for_scheduling_ &&
      !message_loop_scheduled_ && was_empty) {
    ScheduleWork();
  }

  return true;
}

void IncomingTaskQueue::ScheduleWork() {
  DCHECK(is_ready_for_scheduling_);
  message_loop_->ScheduleWork();
  message_loop_scheduled_ = true;
}

}  // namespace base
