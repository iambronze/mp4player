#ifndef MEDIA_PACKET_QUEUE_H_
#define MEDIA_PACKET_QUEUE_H_

#include <queue>
#include <memory>
#include "base/macros.h"
#include "base/time/time.h"
#include "base/synchronization/lock.h"

struct AVPacket;

namespace media {

class PacketQueue {
public:
 static void Init();

 PacketQueue();

 virtual ~PacketQueue();

 void put(AVPacket *pkt);

 AVPacket *get();

 void flush();

public:
 // this is special packet to mark a flush is needed
 // mainly for future use of seeking a video file
 static AVPacket kFlushPkt;

private:
 std::queue<AVPacket *> incoming_packets_;
 base::Lock lock_;
 DISALLOW_COPY_AND_ASSIGN(PacketQueue);
};
}

#endif //MEDIA_PACKET_QUEUE_H_
