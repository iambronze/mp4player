#ifndef BASE_SYNCHRONIZATION_LOCK_H_
#define BASE_SYNCHRONIZATION_LOCK_H_

#include <pthread.h>
#include "base/macros.h"

namespace base {

class Lock {
public:
 Lock();
 ~Lock();
 void Acquire();
 void Release();
 bool Try();

private:
 pthread_mutex_t native_handle_{};
 DISALLOW_COPY_AND_ASSIGN(Lock);
};

class AutoLock {
public:
 explicit AutoLock(Lock &lock) : lock_(lock) {
   lock_.Acquire();
 }

 ~AutoLock() {
   lock_.Release();
 }

private:
 Lock &lock_;
 DISALLOW_COPY_AND_ASSIGN(AutoLock);
};

class AutoUnlock {
public:
 explicit AutoUnlock(Lock &lock) : lock_(lock) {
   lock_.Release();
 }

 ~AutoUnlock() {
   lock_.Acquire();
 }

private:
 Lock &lock_;
 DISALLOW_COPY_AND_ASSIGN(AutoUnlock);
};

}  // namespace base

#endif  // BASE_SYNCHRONIZATION_LOCK_H_
