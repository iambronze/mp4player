#ifndef MEDIA_MP4_DATASET_H_
#define MEDIA_MP4_DATASET_H_

#include <memory>
#include "base/macros.h"
#include "media/ffmpeg_common.h"
#include "base/synchronization/lock.h"

namespace media {
//https://github.com/gameltb/rnbox/blob/0d453cf3f325b55cf88e4c77bd78b06263f2aab6/external/android-emu/android-emu/android/mp4/MP4Dataset.h
//mp4 file demuxe

class PacketQueue;

enum class DemuxResult {
 UNKNOWN,
 OK,
 AV_EOF,
};

class Mp4Dataset {
public:
 Mp4Dataset();
 virtual ~Mp4Dataset();

 static std::unique_ptr<Mp4Dataset>
 create(const std::string& file);

 DemuxResult demuxNextPacket();

 int seek(double timestamp);

 int rewind();

 void setAudioPacketQueue(PacketQueue *audio_queue);

 void setVideoPacketQueue(PacketQueue *video_queue);

 int getAudioStreamIndex() const;

 int getVideoStreamIndex() const;

 bool seekable() const;

 AVStream *getAudioStream();

 AVStream *getVideoStream();

 AVFormatContext *getFormatContext();

 void clearFormatContext();
private:
 int init(const std::string& file);

 AVFormatContext *format_ctx_;
 int audio_stream_idx_ = -1;
 int video_stream_idx_ = -1;
 bool enable_seek_ = false;
 PacketQueue *audio_queue_{};
 PacketQueue *video_queue_{};
 base::Lock lock_;
 DISALLOW_COPY_AND_ASSIGN(Mp4Dataset);
};
}
#endif  // MEDIA_MP4_DATASET_H_
