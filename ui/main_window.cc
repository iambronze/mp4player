#include "ui/main_window.h"
#include "ui/video_view.h"
#include <QtWidgets>

namespace ui {
MainWindow::MainWindow(QWidget *parent) :
    QGraphicsView(parent),
    video_view_(nullptr) {
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
  scene->setSceneRect(scene->itemsBoundingRect());
  setScene(scene);
}

MainWindow::~MainWindow() {
  video_view_ = nullptr;
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