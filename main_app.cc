#include "main_app.h"

namespace app {

MainApp::MainApp(int &argc, char **argv)
    : QApplication(argc, argv),
      main_window_(nullptr) {
}

MainApp::~MainApp() = default;

bool MainApp::initMainView() {
  if (!main_window_) {
    main_window_.reset(new ui::MainWindow());
    main_window_->show();
  }
  return true;
}
}