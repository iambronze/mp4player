#ifndef UI_MAIN_WINDOW_H
#define UI_MAIN_WINDOW_H

#include <memory>
#include <QMainWindow>
#include <QGraphicsView>
#include <QPushButton>

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

private slots:
 void onStart();
 void onStop();
 void onPause();
 void onResume();
 void onSeek();
private:
 void iniSignalSlots();
 QRect desktop_rect_;
 VideoView *video_view_;
 QPushButton *play_button_;
 QPushButton *stop_button_;
 QPushButton *pause_button_;
 QPushButton *resume_button_;
 QPushButton *seek_button_;
 QWidget *control_Widget_;
 Q_DISABLE_COPY(MainWindow);
};
}
#endif // UI_MAIN_WINDOW_H
