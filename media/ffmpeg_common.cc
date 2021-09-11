// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/ffmpeg_common.h"

namespace media {

static const AVRational kMicrosBase = {1, base::Time::kMicrosecondsPerSecond};

base::TimeDelta ConvertFromTimeBase(const AVRational &time_base,
                                    int64_t timestamp) {
  int64_t microseconds = av_rescale_q(timestamp, time_base, kMicrosBase);
  return base::TimeDelta::FromMicroseconds(microseconds);
}

int64_t ConvertToTimeBase(const AVRational &time_base,
                          const base::TimeDelta &timestamp) {
  return av_rescale_q(timestamp.InMicroseconds(), kMicrosBase, time_base);
}

std::string AVErrorToString(int errnum) {
  char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
  av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
  return std::string(errbuf);
}

}  // namespace media
