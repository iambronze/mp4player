// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/pending_task.h"

namespace base {

PendingTask::PendingTask(std::unique_ptr<QueuedTask> task,
                         base::TimeTicks delayed_run_time)
    : task_(std::move(task)),
      delayed_run_time_(delayed_run_time) {}

PendingTask::~PendingTask() = default;

void PendingTask::Run() {
  if (task_) {
    task_->Run();
    task_.reset();
  }
}

void TaskQueue::Swap(TaskQueue *queue) {
  c.swap(queue->c);
}
}  // namespace base
