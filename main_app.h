#ifndef APP_MAIN_APP_H
#define APP_MAIN_APP_H

#include <memory>
#include <set>
#include "base/macros.h"
#include "ui/main_window.h"
#include <QApplication>

namespace app {

class MainApp : public QApplication {
Q_OBJECT

public:
 explicit MainApp(int &argc, char **argv);
 ~MainApp() override;

 bool initMainView();

private:

 QScopedPointer<ui::MainWindow, QScopedPointerDeleteLater> main_window_;

 DISALLOW_COPY_AND_ASSIGN(MainApp);
};
}
#endif // APP_MAIN_APP_H
