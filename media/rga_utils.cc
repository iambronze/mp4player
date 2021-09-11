#include "rga_utils.h"

namespace media {

namespace {

int rga_supported = 1;
int rga_initialized = 0;

int rgaPrepareInfo(void *buf,
                   RgaSURF_FORMAT format,
                   QRect rect,
                   int sw,
                   int sh,
                   rga_info_t *info) {
  memset(info, 0, sizeof(rga_info_t));
  info->fd = -1;
  info->virAddr = buf;
  info->mmuFlag = 1;
  return rga_set_rect(&info->rect, rect.x(), rect.y(), rect.width(), rect.height(), sw, sh, format);
}
}

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
                 unsigned int blend) {
  rga_info_t srcInfo;
  rga_info_t dstInfo;

  memset(&srcInfo, 0, sizeof(rga_info_t));
  memset(&dstInfo, 0, sizeof(rga_info_t));

  if (!rga_supported)
    return -1;

  if (!rga_initialized) {
    if (c_RkRgaInit() < 0) {
      rga_supported = 0;
      return -1;
    }
    rga_initialized = 1;
  }

  if (rgaPrepareInfo(src, src_format, srcRect, src_sw, src_sh, &srcInfo) < 0)
    return -1;

  if (rgaPrepareInfo(dst, dst_format, dstRect, dst_sw, dst_sh, &dstInfo) < 0)
    return -1;

  srcInfo.rotation = rotate;
  if (blend) srcInfo.blend = blend;

  return c_RkRgaBlit(&srcInfo, &dstInfo, nullptr);
}

RgaSURF_FORMAT mpp_format_to_rga_format(MppFrameFormat fmt) {
  if (fmt == MPP_FMT_YUV420P)
    return RK_FORMAT_YCbCr_420_P; //YUV420P
  if (fmt == MPP_FMT_YUV420SP)
    return RK_FORMAT_YCbCr_420_SP; //NV12
  if (fmt == MPP_FMT_YUV420SP_VU)
    return RK_FORMAT_YCrCb_420_SP; //NV21
  return RK_FORMAT_UNKNOWN;
}
}