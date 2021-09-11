// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/simple_thread.h"

#include "base/logging.h"
#include "base/threading/platform_thread.h"

namespace base {

SimpleThread::SimpleThread(const std::string &name)
    : name_(name),
      thread_(), event_(true, false), tid_(0), joined_(false) {
}

SimpleThread::SimpleThread(const std::string &name,
                           const Options &options)
    : name_(name), options_(options),
      thread_(), event_(true, false), tid_(0), joined_(false) {
}

SimpleThread::~SimpleThread() {
  DCHECK(HasBeenStarted()) << "SimpleThread was never started.";
  DCHECK(HasBeenJoined()) << "SimpleThread destroyed without being Join()ed.";
}

void SimpleThread::Start() {
  DCHECK(!HasBeenStarted()) << "Tried to Start a thread multiple times.";
  bool success;
  if (options_.priority() == ThreadPriority::NORMAL) {
    success = PlatformThread::Create(options_.stack_size(), this, &thread_);
  } else {
    success = PlatformThread::CreateWithPriority(options_.stack_size(), this,
                                                 &thread_, options_.priority());
  }
  DCHECK(success);
  event_.Wait();  // Wait for the thread to complete initialization.
}

void SimpleThread::Join() {
  DCHECK(HasBeenStarted()) << "Tried to Join a never-started thread.";
  DCHECK(!HasBeenJoined()) << "Tried to Join a thread multiple times.";
  PlatformThread::Join(thread_);
  joined_ = true;
}

bool SimpleThread::HasBeenStarted() {
  return event_.IsSignaled();
}

void SimpleThread::ThreadMain() {
  tid_ = PlatformThread::CurrentId();
  // Construct our full name of the form "name_prefix_/TID".
  PlatformThread::SetName(name_);

  // We've initialized our new thread, signal that we're done to Start().
  event_.Signal();

  Run();
}

DelegateSimpleThread::DelegateSimpleThread(Delegate *delegate,
                                           const std::string &name)
    : SimpleThread(name),
      delegate_(delegate) {
}

DelegateSimpleThread::DelegateSimpleThread(Delegate *delegate,
                                           const std::string &name,
                                           const Options &options)
    : SimpleThread(name, options),
      delegate_(delegate) {
}

DelegateSimpleThread::~DelegateSimpleThread() {
}

void DelegateSimpleThread::Run() {
  DCHECK(delegate_) << "Tried to call Run without a delegate (called twice?)";
  delegate_->Run();
  delegate_ = NULL;
}
}  // namespace base
