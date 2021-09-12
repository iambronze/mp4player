// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PENDING_TASK_H_
#define BASE_PENDING_TASK_H_

#include <queue>
#include "base/time/time.h"
#include "base/callback.h"

namespace base {

class PendingTask {
public:
 explicit PendingTask(std::unique_ptr<QueuedTask> task,
                      base::TimeTicks delayed_run_time);

 virtual ~PendingTask();

 void Run();

 std::unique_ptr<QueuedTask> task_;
 base::TimeTicks delayed_run_time_;
private:
 DISALLOW_COPY_AND_ASSIGN(PendingTask);
};

class TaskQueue : public std::queue<std::unique_ptr<PendingTask>> {
public:
 void Swap(TaskQueue *queue);
};

}  // namespace base

#endif  // BASE_PENDING_TASK_H_
