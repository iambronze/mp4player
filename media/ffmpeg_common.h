// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FFMPEG_FFMPEG_COMMON_H_
#define MEDIA_FFMPEG_FFMPEG_COMMON_H_

#include <stdint.h>

// Used for FFmpeg error codes.
#include <cerrno>

#include "base/time/time.h"
#include "media/ffmpeg_deleters.h"

// Include FFmpeg header files.
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/log.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>
}  // extern "C"

namespace media {

// The following implement the deleters declared in ffmpeg_deleters.h (which
// contains the declarations needed for use with |scoped_ptr| without #include
// "pollution").

inline void ScopedPtrAVFree::operator()(void *x) const {
  av_free(x);
}

inline void ScopedPtrAVFreePacket::operator()(void *x) const {
  AVPacket *packet = static_cast<AVPacket *>(x);
  av_packet_unref(packet);
  delete packet;
}

inline void ScopedPtrAVFreeContext::operator()(void *x) const {
  AVCodecContext *codec_context = static_cast<AVCodecContext *>(x);
  avcodec_free_context(&codec_context);
}

inline void ScopedPtrAVFreeFrame::operator()(void *x) const {
  AVFrame *frame = static_cast<AVFrame *>(x);
  av_frame_free(&frame);
}

// Converts an int64_t timestamp in |time_base| units to a base::TimeDelta.
// For example if |timestamp| equals 11025 and |time_base| equals {1, 44100}
// then the return value will be a base::TimeDelta for 0.25 seconds since that
// is how much time 11025/44100ths of a second represents.
base::TimeDelta ConvertFromTimeBase(const AVRational &time_base,
                                    int64_t timestamp);

// Converts a base::TimeDelta into an int64_t timestamp in |time_base| units.
// For example if |timestamp| is 0.5 seconds and |time_base| is {1, 44100}, then
// the return value will be 22050 since that is how many 1/44100ths of a second
// represent 0.5 seconds.
int64_t ConvertToTimeBase(const AVRational &time_base,
                          const base::TimeDelta &timestamp);

std::string AVErrorToString(int errnum);

}  // namespace media

#endif  // MEDIA_FFMPEG_FFMPEG_COMMON_H_
