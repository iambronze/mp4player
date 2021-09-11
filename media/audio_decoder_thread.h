#ifndef MEDIA_AUDIO_DECODER_THREAD_H_
#define MEDIA_AUDIO_DECODER_THREAD_H_

#include <memory>
#include "base/macros.h"
#include "base/threading/simple_thread.h"
#include "base/synchronization/lock.h"
#include "media/ffmpeg_common.h"
#include <rkmedia/rkmedia_api.h>

namespace media {
class Mp4Dataset;
class PacketQueue;
class AudioFrameQueue;
class FFmpegAudioDecoder;
class FFmpegAudioResampler;
class FFmpegAACBitstreamConverter;
class VideoPlayer;

class AudioDecoderThread
    : public base::DelegateSimpleThread::Delegate {
public:
 explicit AudioDecoderThread(VideoPlayer *player,
                             Mp4Dataset *dataset,
                             PacketQueue *input_queue,
                             AudioFrameQueue *output_queue);

 virtual ~AudioDecoderThread() override;

private:
 void Run() override;

 void DecodeLoop();

 bool InitDecoder();

 void UnInitDecoder();

 void DecodeOnePacket(AVPacket *pkt);

 void OnNewFrame(AVFrame *frame);

 bool SendFrame(MEDIA_BUFFER mb);

 AVPacket *FetchPacket();

 bool ProcessOutputFrame();

 VideoPlayer *player_;
 Mp4Dataset *dataset_;
 PacketQueue *input_queue_;
 AudioFrameQueue *output_queue_;
 int64_t next_pts_;
 bool keep_running_;
 std::unique_ptr<FFmpegAudioDecoder> decoder_;
 std::unique_ptr<FFmpegAudioResampler> resampler_;
 std::unique_ptr<FFmpegAACBitstreamConverter> bitstream_converter_;
 std::unique_ptr<base::DelegateSimpleThread> thread_;
 DISALLOW_COPY_AND_ASSIGN(AudioDecoderThread);
};
}
#endif  // MEDIA_AUDIO_DECODER_THREAD_H_
