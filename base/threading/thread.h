#ifndef BASE_THREADING_THREAD_H_
#define BASE_THREADING_THREAD_H_

#include <string>
#include <queue>
#include <event2/event.h>
#include <event2/listener.h>
#include "base/synchronization/lock.h"
#include "base/threading/simple_thread.h"
#include "base/callback.h"

namespace base {

class Timer;

class Thread
    : public DelegateSimpleThread::Delegate {
public:
 explicit Thread(const std::string &name,
                 base::ThreadPriority priority = base::ThreadPriority::NORMAL);

 ~Thread() override;

 void Start();

 void Stop();

 static Thread *Current();

 bool IsCurrent() const;

 void PostTask(std::unique_ptr<QueuedTask> task);

 void PostDelayedTask(std::unique_ptr<QueuedTask> task,
                      const base::TimeDelta &delay);

 // std::enable_if is used here to make sure that calls to PostTask() with
 // std::unique_ptr<SomeClassDerivedFromQueuedTask> would not end up being
 // caught by this template.
 template<class Closure,
     typename std::enable_if<!std::is_convertible<
         Closure,
         std::unique_ptr<QueuedTask>>::value>::type * = nullptr>
 void PostTask(Closure &&closure) {
   PostTask(NewClosure(std::forward<Closure>(closure)));
 }

 // See documentation above for performance expectations.
 template<class Closure,
     typename std::enable_if<!std::is_convertible<
         Closure,
         std::unique_ptr<QueuedTask>>::value>::type * = nullptr>
 void PostDelayedTask(Closure &&closure, const base::TimeDelta &delay) {
   PostDelayedTask(NewClosure(std::forward<Closure>(closure)), delay);
 }
private:
 class PendingTask;
 class MessagePump;

 friend class Timer;

 typedef std::queue<std::unique_ptr<PendingTask>> TaskQueue;

 void Run() override;

 void ScheduleWork();

 void ReloadWorkQueue(TaskQueue *work_queue);

 static struct event_base *Base();

 int wakeup_pipe_in_;

 int wakeup_pipe_out_;

 bool task_scheduled_;

 bool running_;

 base::Lock lock_;

 TaskQueue incoming_queue_;

 std::unique_ptr<DelegateSimpleThread> thread_;
 DISALLOW_COPY_AND_ASSIGN(Thread);
};
}  // namespace base

#endif  // BASE_THREADING_THREAD_H_
