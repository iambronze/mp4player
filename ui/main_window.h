#ifndef UI_MAIN_WINDOW_H
#define UI_MAIN_WINDOW_H

#include <memory>
#include <QMainWindow>
#include <QGraphicsView>

namespace ui {
class VideoView;

class MainWindow : public QGraphicsView {
Q_OBJECT
public:
 explicit MainWindow(QWidget *parent = 0);
 ~MainWindow();

 VideoView *video_view();

 void Update();

 void PaintNow(QRect *rc = nullptr);

private:
 QRect desktop_rect_;
 VideoView *video_view_;
 Q_DISABLE_COPY(MainWindow);
};
}
#endif // UI_MAIN_WINDOW_H
