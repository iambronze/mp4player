#ifndef MEDIA_RGA_UTILS_H_
#define MEDIA_RGA_UTILS_H_

#include "base/macros.h"
#include <QRect>

#include <rga/rga.h>
#include <rga/RgaApi.h>
#include <rockchip/rk_mpi.h>

namespace media {
int rgaDrawImage(void *src,
                 RgaSURF_FORMAT src_format,
                 QRect srcRect,
                 int src_sw,
                 int src_sh,
                 void *dst,
                 RgaSURF_FORMAT dst_format,
                 QRect dstRect,
                 int dst_sw,
                 int dst_sh,
                 int rotate,
                 unsigned int blend);

RgaSURF_FORMAT mpp_format_to_rga_format(MppFrameFormat fmt);
}
#endif  // MEDIA_RGA_UTILS_H_
