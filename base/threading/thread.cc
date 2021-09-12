#include <memory>
#include <utility>
#include <functional>
#include "base/threading/thread.h"

namespace base {

void ThreadQuitHelper() {
  MessageLoop::current()->QuitNow();
}

Thread::Thread(const std::string &name)
    : name_(name),
      stopping_(false),
      running_(false),
      thread_(0),
      id_(kInvalidThreadId),
      message_loop_(nullptr),
      start_event_(true, false) {
}

Thread::~Thread() {
  Stop();
}

bool Thread::IsCurrent() const {
  return PlatformThread::CurrentId() == GetThreadId();
}

bool Thread::Start() {
  SimpleThread::Options options;
  return StartWithOptions(options);
}

bool Thread::StartWithOptions(const SimpleThread::Options &options) {
  id_ = kInvalidThreadId;

  std::unique_ptr<MessageLoop> message_loop(new MessageLoop());
  message_loop_ = message_loop.get();
  start_event_.Reset();

  {
    AutoLock lock(thread_lock_);
    if (!PlatformThread::CreateWithPriority(options.stack_size(), this, &thread_,
                                            options.priority())) {
      DLOG(ERROR) << "failed to create thread";
      message_loop_ = nullptr;
      return false;
    }
  }

  start_event_.Wait();
  // The ownership of message_loop is managemed by the newly created thread
  // within the ThreadMain.
  ignore_result(message_loop.release());

  DCHECK(message_loop_);
  return true;
}

void Thread::Stop() {
  AutoLock l(thread_lock_);
  if (thread_.is_null())
    return;

  StopSoon();

  PlatformThread::Join(thread_);
  thread_ = base::PlatformThreadHandle();
  DCHECK(!message_loop_);
  stopping_ = false;
}

void Thread::StopSoon() {
  // We should only be called on the same thread that started us.
  DCHECK_NE(GetThreadId(), PlatformThread::CurrentId());

  if (stopping_ || !message_loop_)
    return;

  stopping_ = true;
  PostTask(std::bind(&ThreadQuitHelper));
}

PlatformThreadId Thread::GetThreadId() const {
  base::AutoLock l(thread_id_lock_);
  return id_;
}

bool Thread::IsRunning() const {
  if (message_loop_ && !stopping_)
    return true;
  AutoLock lock(running_lock_);
  return running_;
}

void Thread::ThreadMain() {
  {
    base::AutoLock l(thread_id_lock_);
    id_ = PlatformThread::CurrentId();
    DCHECK_NE(kInvalidThreadId, id_);
  }

  PlatformThread::SetName(name_);

  std::unique_ptr<MessageLoop> message_loop(message_loop_);
  message_loop->BindToCurrentThread();

  {
    AutoLock lock(running_lock_);
    running_ = true;
  }

  start_event_.Signal();

  message_loop->Run();

  {
    AutoLock lock(running_lock_);
    running_ = false;
  }
  message_loop_ = nullptr;
}

void Thread::PostTask(std::unique_ptr<QueuedTask> task) {
  PostDelayedTask(std::move(task), TimeDelta());
}

void Thread::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                             const base::TimeDelta &delay) {
  message_loop_->PostDelayedTask(std::move(task), delay);
}
}  // namespace base
