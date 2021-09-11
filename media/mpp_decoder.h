#ifndef MEDIA_MPP_DECODER_H_
#define MEDIA_MPP_DECODER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include <rockchip/rk_mpi.h>
#include <rockchip/mpp_buffer.h>
#include <rockchip/mpp_packet.h>

namespace media {

#define FRAMEGROUP_MAX_FRAMES   16

class RKMppDecoder {
public:
 explicit RKMppDecoder(MppCodingType coding_type, size_t max_buffer_size = FRAMEGROUP_MAX_FRAMES);

 virtual ~RKMppDecoder();

 bool Init();

 void UnInit();

 int SendInput(MppPacket packet);

 MppFrame FetchOutput();

 int Flush();

private:
 MppCodingType coding_type_;
 size_t max_buffer_size_;
 MppCtx ctx_;
 MppApi *mpi_;
 MppBufferGroup frame_group_;
 DISALLOW_COPY_AND_ASSIGN(RKMppDecoder);
};
}

#endif //MEDIA_MPP_DECODER_H_
