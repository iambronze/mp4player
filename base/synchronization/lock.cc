#include "base/synchronization/lock.h"

namespace base {
Lock::Lock() {
  pthread_mutex_init(&native_handle_, nullptr);
}

Lock::~Lock() {
  pthread_mutex_destroy(&native_handle_);
}

void Lock::Acquire() {
  pthread_mutex_lock(&native_handle_);
}

void Lock::Release() {
  pthread_mutex_unlock(&native_handle_);
}

bool Lock::Try() {
  int rv = pthread_mutex_trylock(&native_handle_);
  return rv == 0;
}

}  // namespace base

