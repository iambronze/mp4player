#include <QPushButton>
#include <QHBoxLayout>
#include <QPainter>
#include <QPaintEngine>
#include "ui/video_view.h"
#include <rga/rga.h>
#include <rga/RgaApi.h>
#include "ui/main_window.h"
#include "media/rga_utils.h"
#include "media/ffmpeg_common.h"
#include "base/logging.h"

namespace ui {

VideoView::VideoView(const QRect &rect, QGraphicsItem *parent, MainWindow *main_window)
    : QGraphicsObject(parent),
      rect_(rect),
      main_window_(main_window),
      frame_(nullptr) {
  connect(this, &VideoView::signalMediaError, this, &VideoView::InternalMediaError);
  connect(this, &VideoView::signalMediaStop, this, &VideoView::InternalMediaStop);
  connect(this, &VideoView::signalUpdateUI, this, &VideoView::Update);
}

VideoView::~VideoView() {
  stop();
}

QRectF VideoView::boundingRect() const {
  return QRectF(rect_.x(), rect_.y(), rect_.width(), rect_.height());
}

void VideoView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  if (!painter->paintEngine())
    return;

  Q_UNUSED(option);
  Q_UNUSED(widget);

  base::TimeTicks t1 = base::TimeTicks::Now();

  base::AutoLock l(lock_);

  QImage *image = static_cast<QImage *>(painter->paintEngine()->paintDevice());
  if (image->isNull()) {
    return;
  }
  if (!frame_)
    return;
  RK_U32 width = mpp_frame_get_width(frame_);
  RK_U32 height = mpp_frame_get_height(frame_);
  RK_U32 h_stride = mpp_frame_get_hor_stride(frame_);
  RK_U32 v_stride = mpp_frame_get_ver_stride(frame_);
  MppBuffer buffer = mpp_frame_get_buffer(frame_);
  MppFrameFormat fmt = mpp_frame_get_fmt(frame_);
  RgaSURF_FORMAT rga_fmt = media::mpp_format_to_rga_format(fmt);
  int64_t pts = mpp_frame_get_pts(frame_);

  if (buffer && rga_fmt != RK_FORMAT_UNKNOWN) {
    QRect src_rect(0, 0, width, height);
    media::rgaDrawImage(reinterpret_cast<uchar *>(mpp_buffer_get_ptr(buffer)),
                        rga_fmt,
                        src_rect,
                        h_stride,
                        v_stride,
                        image->bits(),
                        RK_FORMAT_BGRA_8888,
                        rect_,
                        image->width(),
                        image->height(),
                        0,
                        0);

    LOG(INFO) << "render video pts:" << pts << ",interval: " << (base::TimeTicks::Now() - t1).InMicroseconds();
  }
}

bool VideoView::start(const std::string &file, bool enable_audio, int volume, bool loop) {
  stop();
  dataset_ = media::Mp4Dataset::create(file);
  player_.reset(new media::VideoPlayer(this, dataset_.get(), enable_audio, volume, loop, 0.8));
  return true;
}

void VideoView::stop() {
  if (player_) {
    player_.reset();
  }
}

void VideoView::pause() {
  if (player_) {
    player_->Pause();
  }
}

void VideoView::resume() {
  if (player_) {
    player_->Resume();
  }
}

void VideoView::mute(bool enable) {
  if (player_) {
    player_->Mute(enable);
  }
}

bool VideoView::isSeekable() {
  if (!dataset_)
    return false;
  return dataset_->seekable();
}

bool VideoView::seek(double percent) {
  if (!player_)
    return false;

  AVStream *stream = dataset_->getVideoStream();
  base::TimeDelta time = media::ConvertFromTimeBase(stream->time_base, stream->duration);
  double value = percent * time.InSecondsF();
  player_->Seek(value);
  return true;
}

void VideoView::setVolume(int volume) {
  if (player_) {
    player_->SetVolume(volume);
  }
}

void VideoView::OnMediaError(int err) {
  emit signalMediaError(err);
}

void VideoView::OnMediaStop() {
  emit signalMediaStop();
}

void VideoView::OnMediaFrameArrival(MppFrame frame) {
  base::AutoLock l(lock_);
  CleanUp();
  frame_ = frame;
  emit signalUpdateUI();
}

void VideoView::CleanUp() {
  if (frame_) {
    mpp_frame_deinit(&frame_);
    frame_ = nullptr;
  }
}

void VideoView::Update() {
  main_window_->PaintNow(&rect_);
}

void VideoView::InternalMediaError(int err) {

}

void VideoView::InternalMediaStop() {
  stop();
}
}
