#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include <set>
#include <list>
#include <memory>
#include <utility>
#include "base/threading/thread.h"
#include "base/posix/eintr_wrapper.h"

namespace base {
namespace {
const char kQuit = 1;
const char kRunTask = 2;

void IgnoreSigPipeSignalOnCurrentThread() {
  sigset_t sigpipe_mask;
  sigemptyset(&sigpipe_mask);
  sigaddset(&sigpipe_mask, SIGPIPE);
  pthread_sigmask(SIG_BLOCK, &sigpipe_mask, nullptr);
}

bool SetNonBlocking(int fd) {
  const int flags = fcntl(fd, F_GETFL);
  return (flags & O_NONBLOCK) || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

pthread_key_t g_queue_ptr_tls = 0;

void InitializeTls() {
  pthread_key_create(&g_queue_ptr_tls, nullptr);
}

pthread_key_t GetQueuePtrTls() {
  static pthread_once_t init_once = PTHREAD_ONCE_INIT;
  pthread_once(&init_once, &InitializeTls);
  return g_queue_ptr_tls;
}

TimeTicks CalculateDelayedRuntime(TimeDelta delay) {
  TimeTicks delayed_run_time;
  if (delay > TimeDelta())
    delayed_run_time = TimeTicks::Now() + delay;
  return delayed_run_time;
}
}

class Thread::PendingTask {
public:
 explicit PendingTask(std::unique_ptr<QueuedTask> task, const base::TimeDelta &delay)
     : task_(std::move(task)),
       delayed_run_time_(CalculateDelayedRuntime(delay)),
       e_(nullptr) {}

 virtual ~PendingTask() {
   if (e_) {
     event_free(e_);
     e_ = nullptr;
   }
 }

 void Run() {
   if (task_) {
     task_->Run();
     task_.reset();
   }
 }
 std::unique_ptr<QueuedTask> task_;
 base::TimeTicks delayed_run_time_;
 struct event *e_;
 DISALLOW_COPY_AND_ASSIGN(PendingTask);
};

class Thread::MessagePump {
public:
 explicit MessagePump(Thread *thread)
     : thread_(thread),
       is_active_(true),
       event_base_(nullptr),
       read_event_(nullptr) {}

 virtual ~MessagePump() = default;

 void Init(int wakeup_pipe_out) {
   event_base_ = event_base_new();
   read_event_ = event_new(event_base_,
                           wakeup_pipe_out,
                           EV_READ | EV_PERSIST,
                           &MessagePump::OnWakeup,
                           this);
   event_add(read_event_, nullptr);
   pthread_setspecific(GetQueuePtrTls(), this);
 }

 void CleanUp() {
   pthread_setspecific(GetQueuePtrTls(), nullptr);

   for (auto task : delayed_tasks_)
     delete task;
   delayed_tasks_.clear();

   if (read_event_) {
     event_free(read_event_);
     read_event_ = nullptr;
   }
   if (event_base_) {
     event_base_free(event_base_);
     event_base_ = nullptr;
   }
 }

 void Run() const {
   while (is_active_) {
     event_base_loop(event_base_, EVLOOP_NO_EXIT_ON_EMPTY);
   }
 }

 static void OnWakeup(evutil_socket_t listener, short event, void *arg) {
   auto ctx = static_cast<MessagePump *>(pthread_getspecific(GetQueuePtrTls()));
   char buf;
   int nread = HANDLE_EINTR(read(listener, &buf, sizeof(buf)));
   if (buf == kQuit) {
     ctx->is_active_ = false;
     event_base_loopbreak(ctx->event_base_);
   } else if (buf == kRunTask) {
     for (;;) {
       Thread::TaskQueue task_queue;
       ctx->thread_->ReloadWorkQueue(&task_queue);
       if (task_queue.empty())
         break;
       do {
         std::unique_ptr<PendingTask> pending_task(std::move(task_queue.front()));
         task_queue.pop();
         if (!pending_task->delayed_run_time_.is_null()) {
           ctx->AddToDelayedWorkQueue(std::move(pending_task));
         } else {
           pending_task->Run();
         }
       } while (!task_queue.empty());
     }
   }
 }

 static void RunTimer(evutil_socket_t listener, short event, void *arg) {
   auto ctx = static_cast<MessagePump *>(pthread_getspecific(GetQueuePtrTls()));
   auto task = reinterpret_cast<PendingTask *>(arg);
   task->Run();
   ctx->delayed_tasks_.remove(task);
   delete task;
 }

 void AddToDelayedWorkQueue(std::unique_ptr<PendingTask> task) {
   base::TimeTicks now = base::TimeTicks::Now();
   if (task->delayed_run_time_ <= now) {
     task->Run();
   } else {
     base::TimeDelta delay = task->delayed_run_time_ - now;
     task->e_ = event_new(event_base_, -1, EV_TIMEOUT, &MessagePump::RunTimer, task.get());
     timeval tv = {static_cast<__time_t>(delay.InSeconds()),
                   static_cast<__suseconds_t>(delay.InMicroseconds() % Time::kMicrosecondsPerSecond)};
     event_add(task->e_, &tv);
     delayed_tasks_.push_back(task.release());
   }
 }

 Thread *const thread_;

 bool is_active_;

 struct event_base *event_base_;

 struct event *read_event_;

 std::list<PendingTask *> delayed_tasks_;

private:
 DISALLOW_COPY_AND_ASSIGN(MessagePump);
};

Thread::Thread(const std::string &name,
               base::ThreadPriority priority)
    : wakeup_pipe_in_(-1),
      wakeup_pipe_out_(-1),
      task_scheduled_(false),
      running_(false),
      thread_(new DelegateSimpleThread(this, name, SimpleThread::Options(priority))) {
  int fds[2];
  pipe(fds);
  SetNonBlocking(fds[0]);
  SetNonBlocking(fds[1]);
  wakeup_pipe_out_ = fds[0];
  wakeup_pipe_in_ = fds[1];
}

Thread::~Thread() {
  Stop();
}

void Thread::Start() {
  if (!running_) {
    thread_->Start();
    running_ = true;
  }
}

void Thread::Stop() {
  if (thread_) {
    if (running_) {
      struct timespec ts{};
      char message = kQuit;
      while (write(wakeup_pipe_in_, &message, sizeof(message)) != sizeof(message)) {
        ts.tv_sec = 0;
        ts.tv_nsec = 1000000;
        nanosleep(&ts, nullptr);
      }
    }
    thread_->Join();
    thread_.reset();
    IgnoreSigPipeSignalOnCurrentThread();
    close(wakeup_pipe_in_);
    close(wakeup_pipe_out_);
    wakeup_pipe_in_ = -1;
    wakeup_pipe_out_ = -1;
    running_ = false;
  }
}

void Thread::ScheduleWork() {
  if (wakeup_pipe_in_ != -1) {
    uint8_t message = kRunTask;
    HANDLE_EINTR(::write(wakeup_pipe_in_, &message, 1));
    task_scheduled_ = true;
  }
}

void Thread::PostTask(std::unique_ptr<QueuedTask> task) {
  PostDelayedTask(std::move(task), TimeDelta());
}

void Thread::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                             const base::TimeDelta &delay) {
  std::unique_ptr<PendingTask> pending_task(
      new PendingTask(std::move(task), delay));

  base::AutoLock l(lock_);
  bool was_empty = incoming_queue_.empty();
  incoming_queue_.push(std::move(pending_task));
  if (!task_scheduled_ && was_empty) {
    ScheduleWork();
  }
}

void Thread::Run() {
  MessagePump pump(this);
  pump.Init(wakeup_pipe_out_);

  pump.Run();

  pump.CleanUp();
}

void Thread::ReloadWorkQueue(TaskQueue *work_queue) {
  base::AutoLock l(lock_);
  if (incoming_queue_.empty()) {
    task_scheduled_ = false;
  } else {
    incoming_queue_.swap(*work_queue);
  }
}

struct event_base *Thread::Base() {
  auto ctx = static_cast<MessagePump *>(pthread_getspecific(GetQueuePtrTls()));
  return ctx ? ctx->event_base_ : nullptr;
}

bool Thread::IsCurrent() const {
  return PlatformThread::CurrentId() == thread_->tid();
}

Thread *Thread::Current() {
  auto ctx = static_cast<MessagePump *>(pthread_getspecific(GetQueuePtrTls()));
  return ctx ? ctx->thread_ : nullptr;
}
}  // namespace base
