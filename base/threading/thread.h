#ifndef BASE_THREADING_THREAD_H_
#define BASE_THREADING_THREAD_H_

#include <string>
#include "base/synchronization/lock.h"
#include "base/threading/simple_thread.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"

namespace base {

class Thread : public PlatformThread::Delegate {
public:
 explicit Thread(const std::string &name);

 ~Thread() override;

 bool Start();

 bool StartWithOptions(const SimpleThread::Options &options);

 void Stop();

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

 // Returns the name of this thread (for display in debugger too).
 const std::string &thread_name() const { return name_; }

 // The native thread handle.
 PlatformThreadHandle thread_handle() { return thread_; }

 PlatformThreadId GetThreadId() const;

 bool IsRunning() const;

private:
 void ThreadMain() override;

 void StopSoon();

 std::string name_;

 bool stopping_;

 mutable base::Lock running_lock_;

 bool running_;

 PlatformThreadHandle thread_;

 PlatformThreadId id_;

 mutable WaitableEvent id_event_;

 mutable base::Lock thread_lock_;

 MessageLoop *message_loop_;

 mutable WaitableEvent start_event_;

 DISALLOW_COPY_AND_ASSIGN(Thread);
};
}  // namespace base

#endif  // BASE_THREADING_THREAD_H_
