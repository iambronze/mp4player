#include "ui/main_window.h"
#include "ui/video_view.h"
#include <QtWidgets>

namespace ui {
static void setButtonFormat(QBoxLayout *layout, QPushButton *btn, QRect rect) {
  btn->setFixedSize(rect.width() / 5, rect.width() / 10);
  btn->setStyleSheet("QPushButton{font-size:30px}");
  layout->addWidget(btn);
}

MainWindow::MainWindow(QWidget *parent) :
    QGraphicsView(parent),
    video_view_(nullptr),
    play_button_(nullptr),
    stop_button_(nullptr),
    pause_button_(nullptr),
    resume_button_(nullptr),
    seek_button_(nullptr),
    control_Widget_(nullptr) {
  this->setStyleSheet("background: transparent");
  this->setAttribute(Qt::WA_DeleteOnClose, true);
  desktop_rect_ = QApplication::desktop()->availableGeometry();
  resize(desktop_rect_.width(), desktop_rect_.height());
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto scene = new QGraphicsScene(this);
  scene->setItemIndexMethod(QGraphicsScene::NoIndex);
  video_view_ = new VideoView(QRect(0, 0, 800, 480), nullptr, this);
  video_view_->setVisible(true);
  video_view_->setZValue(0);
  scene->addItem(video_view_);

  auto hLayout = new QHBoxLayout;
  hLayout->setMargin(0);
  hLayout->setSpacing(0);

  play_button_ = new QPushButton(tr("play"));
  setButtonFormat(hLayout, play_button_, desktop_rect_);
  stop_button_ = new QPushButton(tr("stop"));
  setButtonFormat(hLayout, stop_button_, desktop_rect_);

  pause_button_ = new QPushButton(tr("pause"));
  setButtonFormat(hLayout, pause_button_, desktop_rect_);

  resume_button_ = new QPushButton(tr("resume"));
  setButtonFormat(hLayout, resume_button_, desktop_rect_);

  seek_button_ = new QPushButton(tr("seek"));
  setButtonFormat(hLayout, seek_button_, desktop_rect_);

  control_Widget_ = new QWidget();
  control_Widget_->setLayout(hLayout);

  control_Widget_->setWindowOpacity(0.8);
  control_Widget_->setGeometry(0, desktop_rect_.height() / 2, 0, desktop_rect_.width() / 10);

  scene->addWidget(control_Widget_);
  scene->setSceneRect(scene->itemsBoundingRect());
  setScene(scene);

  iniSignalSlots();
}

MainWindow::~MainWindow() {
  video_view_ = nullptr;
}

void MainWindow::iniSignalSlots() {
  connect(play_button_, SIGNAL(clicked()), this, SLOT(onStart()));
  connect(stop_button_, SIGNAL(clicked()), this, SLOT(onStop()));
  connect(pause_button_, SIGNAL(clicked()), this, SLOT(onPause()));
  connect(resume_button_, SIGNAL(clicked()), this, SLOT(onResume()));
  connect(seek_button_, SIGNAL(clicked()), this, SLOT(onSeek()));
}

void MainWindow::onStart() {
  video_view_->start("/data/jingxi/media/upload.mp4", true, 20, true);
}

void MainWindow::onStop() {
  video_view_->stop();
}

void MainWindow::onPause() {
  video_view_->pause();
}

void MainWindow::onResume() {
  video_view_->resume();
}

void MainWindow::onSeek() {
  video_view_->seek(0.6);
}

void MainWindow::Update() {
  scene()->update(0, 0, desktop_rect_.width(), desktop_rect_.height());
  update(0, 0, desktop_rect_.width(), desktop_rect_.height());
}

void MainWindow::PaintNow(QRect *rc) {
  if (rc) {
    repaint(*rc);
  } else {
    scene()->update(desktop_rect_);
    update(desktop_rect_);
  }
}

VideoView *MainWindow::video_view() {
  return video_view_;
}

}