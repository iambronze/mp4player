// Copyright 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

#include <stdio.h>
#include <stdlib.h>

#include <iomanip>

#include <paths.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include "base/posix/safe_strerror.h"

#include <sys/syscall.h>
#include <sys/types.h>

namespace logging {

namespace {

const char* const log_severity_names[] = {
  "INFO",
  "WARNING",
  "ERROR",
  "ERROR_REPORT",
  "FATAL"
};

LogMessageHandlerFunction g_log_message_handler = nullptr;

}  // namespace

void SetLogMessageHandler(LogMessageHandlerFunction log_message_handler) {
  g_log_message_handler = log_message_handler;
}

LogMessageHandlerFunction GetLogMessageHandler() {
  return g_log_message_handler;
}

LogMessage::LogMessage(const char* function,
                       const char* file_path,
                       int line,
                       LogSeverity severity)
    : stream_(),
      file_path_(file_path),
      message_start_(0),
      line_(line),
      severity_(severity) {
  Init(function);
}

LogMessage::LogMessage(const char* function,
                       const char* file_path,
                       int line,
                       std::string* result)
    : stream_(),
      file_path_(file_path),
      message_start_(0),
      line_(line),
      severity_(LOG_FATAL) {
  Init(function);
  stream_ << "Check failed: " << *result << ". ";
  delete result;
}

LogMessage::~LogMessage() {
  stream_ << std::endl;
  std::string str_newline(stream_.str());

  if (g_log_message_handler &&
      g_log_message_handler(
          severity_, file_path_, line_, message_start_, str_newline)) {
    return;
  }

  fprintf(stderr, "%s", str_newline.c_str());
  fflush(stderr);

  if (severity_ == LOG_FATAL) {
#if defined(COMPILER_MSVC)
    __debugbreak();
#if defined(ARCH_CPU_X86_FAMILY)
    __ud2();
#elif defined(ARCH_CPU_ARM64)
    __hlt(0);
#else
#error Unsupported Windows Arch
#endif
#elif defined(ARCH_CPU_X86_FAMILY)
    asm("int3; ud2;");
#elif defined(ARCH_CPU_ARMEL)
    asm("bkpt #0; udf #0;");
#elif defined(ARCH_CPU_ARM64)
    asm("brk #0; hlt #0;");
#else
    __builtin_trap();
#endif
  }
}

void LogMessage::Init(const char* function) {
  std::string file_name(file_path_);
  size_t last_slash = file_name.find_last_of('/');
  if (last_slash != std::string::npos) {
    file_name.assign(file_name.substr(last_slash + 1));
  }
  pid_t pid = getpid();
  pid_t thread = syscall(__NR_gettid);
  stream_ << '['
          << pid
          << ':'
          << thread
          << ':'
          << std::setfill('0');

  timeval tv;
  gettimeofday(&tv, nullptr);
  tm local_time;
  localtime_r(&tv.tv_sec, &local_time);
  stream_ << std::setw(4) << local_time.tm_year + 1900
          << std::setw(2) << local_time.tm_mon + 1
          << std::setw(2) << local_time.tm_mday
          << ','
          << std::setw(2) << local_time.tm_hour
          << std::setw(2) << local_time.tm_min
          << std::setw(2) << local_time.tm_sec
          << '.'
          << std::setw(6) << tv.tv_usec;

  stream_ << ':';

  if (severity_ >= 0) {
    stream_ << log_severity_names[severity_];
  } else {
    stream_ << "VERBOSE" << -severity_;
  }

  stream_ << ' '
          << file_name
          << ':'
          << line_
          << "] ";

  message_start_ = stream_.str().size();
}

ErrnoLogMessage::ErrnoLogMessage(const char* function,
                                 const char* file_path,
                                 int line,
                                 LogSeverity severity,
                                 int err)
    : LogMessage(function, file_path, line, severity),
      err_(err) {
}

ErrnoLogMessage::~ErrnoLogMessage() {
  stream() << ": "
           << base::safe_strerror(err_)
           << " ("
           << err_
           << ")";
}

}  // namespace logging
