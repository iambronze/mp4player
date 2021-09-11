#ifndef BASE_CALLBACK_H_
#define BASE_CALLBACK_H_

#include <memory>
#include <utility>
#include "base/ptr_util.h"

namespace base {

class QueuedTask {
public:
 QueuedTask() {}
 virtual ~QueuedTask() {}

 virtual void Run() = 0;
};

// Simple implementation of QueuedTask for use with rtc::Bind and lambdas.
template<class Closure>
class ClosureTask : public QueuedTask {
public:
 explicit ClosureTask(Closure &&closure)
     : closure_(std::forward<Closure>(closure)) {}

private:
 void Run() override {
   closure_();
 }

 typename std::remove_const<
     typename std::remove_reference<Closure>::type>::type closure_;
};

// Convenience function to construct closures that can be passed directly
// to methods that support std::unique_ptr<QueuedTask> but not template
// based parameters.
template<class Closure>
static std::unique_ptr<QueuedTask> NewClosure(Closure &&closure) {
  return base::MakeUnique<ClosureTask<Closure>>(std::forward<Closure>(closure));
}
}  // namespace base

#endif  // BASE_CALLBACK_H_
