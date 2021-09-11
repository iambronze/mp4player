#include <cerrno>
#include <stdio.h>
#include "media/mpp_decoder.h"
#include "base/logging.h"

namespace media {

RKMppDecoder::RKMppDecoder(MppCodingType coding_type, size_t max_buffer_size)
    : coding_type_(coding_type),
      max_buffer_size_(max_buffer_size),
      ctx_(nullptr),
      mpi_(nullptr),
      frame_group_(nullptr) {}

RKMppDecoder::~RKMppDecoder() {
  UnInit();
}

bool RKMppDecoder::Init() {
  if (ctx_)
    return true;

  MPP_RET ret = mpp_create(&ctx_, &mpi_);
  if (MPP_OK != ret) {
    LOG(ERROR) << "mpp_create failed: " << ret;
    return false;
  }

  ret = mpp_init(ctx_, MPP_CTX_DEC, coding_type_);
  if (ret != MPP_OK) {
    LOG(ERROR) << "mpp_init failed:" << ret << ",with type:" << coding_type_;
    return false;
  }

  MppParam param;
  int mode = MPP_POLL_NON_BLOCK;
  param = &mode;
  ret = mpi_->control(ctx_, MPP_SET_OUTPUT_BLOCK, param);
  if (ret != MPP_OK) {
    LOG(ERROR) << "MPP_SET_OUTPUT_BLOCK failed: " << ret;
    return false;
  }

  mode = MPP_POLL_NON_BLOCK;
  param = &mode;
  ret = mpi_->control(ctx_, MPP_SET_INPUT_BLOCK, param);
  if (ret != MPP_OK) {
    LOG(ERROR) << "MPP_SET_INPUT_BLOCK failed: " << ret;
    return false;
  }

  ret = mpp_buffer_group_get_internal(&frame_group_, MPP_BUFFER_TYPE_ION);
  if (ret != MPP_OK) {
    LOG(ERROR) << "mpp_buffer_group_get_internal failed: " << ret;
    return false;
  }
  ret = mpi_->control(ctx_, MPP_DEC_SET_EXT_BUF_GROUP, frame_group_);
  if (ret != MPP_OK) {
    LOG(ERROR) << "MPP_DEC_SET_EXT_BUF_GROUP failed: " << ret;
    return false;
  }
  ret = mpp_buffer_group_limit_config(frame_group_, 0, max_buffer_size_);
  if (ret != MPP_OK) {
    LOG(ERROR) << "mpp_buffer_group_limit_config failed: " << ret << ",max buffer size: " << max_buffer_size_;
    return false;
  }
  return true;
}

void RKMppDecoder::UnInit() {
  if (mpi_) {
    mpi_->reset(ctx_);
    mpp_destroy(ctx_);
  }
  if (frame_group_) {
    mpp_buffer_group_clear(frame_group_);
    mpp_buffer_group_put(frame_group_);
    frame_group_ = nullptr;
  }
}

int RKMppDecoder::SendInput(MppPacket packet) {
  if (!ctx_)
    return -EFAULT;
  return mpi_->decode_put_packet(ctx_, packet);
}

MppFrame RKMppDecoder::FetchOutput() {
  if (!ctx_)
    return nullptr;

  Again:
  MppFrame mppframe = nullptr;
  MPP_RET ret = mpi_->decode_get_frame(ctx_, &mppframe);
  if (ret != MPP_OK) {
    if (ret != MPP_ERR_TIMEOUT) {
      DLOG(ERROR) << "decode_get_frame failed: " << ret;
      return nullptr;
    }
  }
  if (!mppframe) {
    return nullptr;
  }
  if (mpp_frame_get_info_change(mppframe)) {
    RK_U32 width = mpp_frame_get_width(mppframe);
    RK_U32 height = mpp_frame_get_height(mppframe);
    RK_U32 vir_width = mpp_frame_get_hor_stride(mppframe);
    RK_U32 vir_height = mpp_frame_get_ver_stride(mppframe);
    ret = mpi_->control(ctx_, MPP_DEC_SET_INFO_CHANGE_READY, nullptr);
    if (ret != MPP_OK) {
      DLOG(ERROR) << "mpp_frame_get_info_change failed: " << ret;
    }
    DLOG(INFO) << "MppDec info change(" << width << "," << height << "," << vir_width << "," << vir_height << ")";
    mpp_frame_deinit(&mppframe);
    goto Again;
  }

  if (mpp_frame_get_discard(mppframe)) {
    DLOG(ERROR) << "Received a discard frame";
    mpp_frame_deinit(&mppframe);
    return nullptr;
  }

  if (mpp_frame_get_errinfo(mppframe)) {
    DLOG(ERROR) << "Received a errinfo frame";
    mpp_frame_deinit(&mppframe);
    return nullptr;
  }
  return mppframe;
}

int RKMppDecoder::Flush() {
  if (!ctx_)
    return -EFAULT;
  return mpi_->reset(ctx_);
}
}
