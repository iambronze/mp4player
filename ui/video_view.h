#ifndef UI_VIDEO_VIEW_H
#define UI_VIDEO_VIEW_H

#include <memory>
#include <queue>
#include <QGraphicsObject>
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "media/video_player.h"
#include "media/mp4_dataset.h"

namespace ui {
class MainWindow;

class VideoView : public QGraphicsObject,
                  public media::VideoPlayer::Delegate {
Q_OBJECT
public:
 explicit VideoView(const QRect &rect,
                    QGraphicsItem *parent = 0,
                    MainWindow *main_window = nullptr);
 ~VideoView();

 bool start(const std::string &file, bool enable_audio, int volume, bool loop);

 void stop();

 void pause();

 void resume();

 void mute(bool enable);

 bool isSeekable();

 bool seek(double percent);

 void setVolume(int volume);

protected:
 QRectF boundingRect() const override;

 void paint(QPainter *painter,
            const QStyleOptionGraphicsItem *option,
            QWidget *widget = 0) override;
private:
signals:
 void signalMediaError(int err);
 void signalMediaStop();
 void signalUpdateUI();

private slots:
 void InternalMediaError(int err);
 void InternalMediaStop();
 void Update();
private:

 void OnMediaError(int err) override;

 void OnMediaStop() override;

 void OnMediaFrameArrival(MppFrame mb) override;

 void CleanUp();

 QRect rect_;

 MainWindow *main_window_;

 MppFrame frame_;

 base::Lock lock_;

 std::unique_ptr<media::Mp4Dataset> dataset_;
 std::unique_ptr<media::VideoPlayer> player_;

 Q_DISABLE_COPY(VideoView);
};
}

#endif // UI_VIDEO_VIEW_H
