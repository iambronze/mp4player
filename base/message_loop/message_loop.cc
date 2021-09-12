#include "base/message_loop/message_loop.h"
#include "base/posix/eintr_wrapper.h"
#include "base/message_loop/incoming_task_queue.h"

#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

extern "C" {
#include <event2/event.h>
#include <event2/listener.h>
}

namespace base {
namespace {
bool SetNonBlocking(int fd) {
  const int flags = fcntl(fd, F_GETFL);
  return (flags & O_NONBLOCK) || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

void IgnoreSigPipeSignalOnCurrentThread() {
  sigset_t sigpipe_mask;
  sigemptyset(&sigpipe_mask);
  sigaddset(&sigpipe_mask, SIGPIPE);
  pthread_sigmask(SIG_BLOCK, &sigpipe_mask, nullptr);
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
}

class MessageLoop::DelayedTask {
public:
 explicit DelayedTask(std::unique_ptr<QueuedTask> task)
     : event_(nullptr), task_(std::move(task)) {}

 virtual ~DelayedTask() {
   if (event_) {
     event_free(event_);
     event_ = nullptr;
   }
 }

 void Run() {
   if (task_) {
     task_->Run();
     task_.reset();
   }
 }
 struct event *event_;
private:
 std::unique_ptr<QueuedTask> task_;
 DISALLOW_COPY_AND_ASSIGN(DelayedTask);
};

MessageLoop::MessageLoop()
    : event_base_(event_base_new()),
      wakeup_pipe_in_(-1),
      wakeup_pipe_out_(-1),
      wakeup_event_(nullptr),
      keep_running_(true),
      incoming_task_queue_(new IncomingTaskQueue(this)) {
  if (!Init())
    NOTREACHED();
}

MessageLoop::~MessageLoop() {
  for (auto &i: delayed_tasks_) {
    delete i;
  }
  delayed_tasks_.clear();

  event_del(wakeup_event_);
  event_free(wakeup_event_);

  IgnoreSigPipeSignalOnCurrentThread();

  if (wakeup_pipe_in_ >= 0) {
    if (IGNORE_EINTR(close(wakeup_pipe_in_)) < 0)
      DPLOG(ERROR) << "close";
  }
  if (wakeup_pipe_out_ >= 0) {
    if (IGNORE_EINTR(close(wakeup_pipe_out_)) < 0)
      DPLOG(ERROR) << "close";
  }
  incoming_task_queue_->WillDestroyCurrentMessageLoop();
  incoming_task_queue_.reset();
  event_base_free(event_base_);
  if (current() == this)
    pthread_setspecific(GetQueuePtrTls(), nullptr);
}

bool MessageLoop::Init() {
  evutil_socket_t fds[2];
  if (pipe(fds)) {
    close(fds[0]);
    close(fds[1]);
    DLOG(ERROR) << "pipe() failed, errno: " << errno;
    return false;
  }
  if (!SetNonBlocking(fds[0])) {
    DLOG(ERROR) << "SetNonBlocking for pipe fd[0] failed, errno: " << errno;
    return false;
  }
  if (!SetNonBlocking(fds[1])) {
    DLOG(ERROR) << "SetNonBlocking for pipe fd[1] failed, errno: " << errno;
    return false;
  }
  wakeup_pipe_out_ = fds[0];
  wakeup_pipe_in_ = fds[1];
  wakeup_event_ = event_new(event_base_,
                            wakeup_pipe_out_,
                            EV_READ | EV_PERSIST,
                            &MessageLoop::OnWakeup,
                            this);
  if (event_add(wakeup_event_, nullptr))
    return false;
  return true;
}

void MessageLoop::BindToCurrentThread() {
  pthread_setspecific(GetQueuePtrTls(), this);
  incoming_task_queue_->StartScheduling();
}

void MessageLoop::Run() {
  while (keep_running_) {
    event_base_loop(event_base_, EVLOOP_NO_EXIT_ON_EMPTY);
  }
}

void MessageLoop::QuitNow() {
  keep_running_ = false;
  event_base_loopbreak(event_base_);
}

void MessageLoop::ScheduleWork() {
  char buf = 0;
  int nwrite = HANDLE_EINTR(write(wakeup_pipe_in_, &buf, 1));
  DCHECK(nwrite == 1 || errno == EAGAIN)
  << "[nwrite:" << nwrite << "] [errno:" << errno << "]";
}

MessageLoop *MessageLoop::current() {
  return static_cast<MessageLoop *>(pthread_getspecific(GetQueuePtrTls()));
}

event_base *MessageLoop::base() {
  return event_base_;
}

void MessageLoop::OnWakeup(evutil_socket_t socket, short flags, void *context) {
  char buf;
  int nread = HANDLE_EINTR(read(socket, &buf, 1));
  DCHECK_EQ(nread, 1);

  auto ptr = reinterpret_cast<MessageLoop *>(context);
  for (;;) {
    TaskQueue work_queue;
    ptr->ReloadWorkQueue(&work_queue);
    if (work_queue.empty())
      break;
    do {
      std::unique_ptr<PendingTask> pending_task(std::move(work_queue.front()));
      work_queue.pop();
      if (!pending_task->delayed_run_time_.is_null()) {
        ptr->AddToDelayedWorkQueue(std::move(pending_task));
      } else {
        pending_task->Run();
      }
    } while (!work_queue.empty());
  }
}

void MessageLoop::PostDelayedTask(std::unique_ptr<QueuedTask> task,
                                  TimeDelta delay) {
  incoming_task_queue_->AddToIncomingQueue(std::move(task), delay);
}

void MessageLoop::ReloadWorkQueue(TaskQueue *work_queue) {
  incoming_task_queue_->ReloadWorkQueue(work_queue);
}

void MessageLoop::AddToDelayedWorkQueue(std::unique_ptr<PendingTask> task) {
  base::TimeTicks now = base::TimeTicks::Now();
  if (task->delayed_run_time_ <= now) {
    task->Run();
  } else {
    base::TimeDelta delay = task->delayed_run_time_ - now;
    auto delayed_task = new DelayedTask(std::move(task->task_));
    delayed_task->event_ = event_new(event_base_, -1, EV_TIMEOUT, &MessageLoop::RunTimer, delayed_task);
    timeval tv = {static_cast<__time_t>(delay.InSeconds()),
                  static_cast<__suseconds_t>(delay.InMicroseconds() % Time::kMicrosecondsPerSecond)};
    event_add(delayed_task->event_, &tv);
    delayed_tasks_.push_back(delayed_task);
  }
}

void MessageLoop::RunTimer(evutil_socket_t socket, short flags, void *context) {
  auto task = reinterpret_cast<DelayedTask *>(context);
  task->Run();
  auto ptr = MessageLoop::current();
  ptr->delayed_tasks_.remove(task);
  delete task;
}
}