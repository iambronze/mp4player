#include "media/packet_queue.h"
#include "media/ffmpeg_common.h"
#include "base/logging.h"

namespace media {

AVPacket PacketQueue::kFlushPkt = {0};

void PacketQueue::Init() {
  av_init_packet(&kFlushPkt);
  kFlushPkt.data = (uint8_t *) &kFlushPkt;
}

PacketQueue::PacketQueue() = default;

PacketQueue::~PacketQueue() {
  flush();
}

void PacketQueue::put(AVPacket *pkt) {
  base::AutoLock l(lock_);
  incoming_packets_.push(pkt);
}

AVPacket *PacketQueue::get() {
  base::AutoLock l(lock_);
  if (incoming_packets_.empty())
    return nullptr;
  AVPacket *pkt = incoming_packets_.front();
  incoming_packets_.pop();
  DLOG(INFO) << "PacketQueue size: " << incoming_packets_.size();
  return pkt;
}

void PacketQueue::flush() {
  base::AutoLock l(lock_);
  while (!incoming_packets_.empty()) {
    AVPacket *pkt = incoming_packets_.front();
    incoming_packets_.pop();
    if (pkt != &kFlushPkt) {
      av_packet_unref(pkt);
      av_packet_free(&pkt);
    }
  }
  incoming_packets_.push(&kFlushPkt);
}
}
