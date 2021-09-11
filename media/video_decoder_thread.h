#ifndef MEDIA_VIDEO_DECODER_THREAD_H_
#define MEDIA_VIDEO_DECODER_THREAD_H_

#include <memory>
#include "base/macros.h"
#include "base/threading/simple_thread.h"
#include "base/synchronization/lock.h"
#include "media/ffmpeg_common.h"
#include <rkmedia/rkmedia_api.h>
#include <rockchip/mpp_packet.h>

namespace media {
class Mp4Dataset;
class PacketQueue;
class VideoFrameQueue;
class RKMppDecoder;
class VideoPlayer;

class VideoDecoderThread
    : public base::DelegateSimpleThread::Delegate {
public:
 explicit VideoDecoderThread(VideoPlayer *player,
                             Mp4Dataset *dataset,
                             PacketQueue *input_queue,
                             VideoFrameQueue *output_queue);

 virtual ~VideoDecoderThread() override;

private:
 void Run() override;

 void InitDecoder();

 void UnInitDecoder();

 void DecodeLoop();

 AVPacket *FetchPacket();

 bool ProcessOneOutputBuffer(bool *eos_reached);

 bool DecodePacket(MppPacket mpp_packet, bool *eos_reached);

 MppPacket MakeMppPacket(AVPacket *packet);

 bool SendFrame(MppFrame frame);

 void SendInput(AVPacket *pkt, bool *eos_reached);

 VideoPlayer *player_;
 Mp4Dataset *dataset_;
 PacketQueue *input_queue_;
 VideoFrameQueue *output_queue_;
 AVBSFContext *avbsf_;
 int64_t next_pts_;
 base::TimeDelta frame_duration_;
 bool keep_running_;
 std::unique_ptr<RKMppDecoder> decoder_;
 std::unique_ptr<base::DelegateSimpleThread> thread_;
 DISALLOW_COPY_AND_ASSIGN(VideoDecoderThread);
};
}
#endif  // MEDIA_VIDEO_DECODER_THREAD_H_
