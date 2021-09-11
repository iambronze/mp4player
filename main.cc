#include <rkmedia/rkmedia_api.h>
#include "media/packet_queue.h"
#include "main_app.h"
#include <rga/RgaApi.h>
#include <csignal>

int main(int argc, char *argv[]) {
  signal(SIGPIPE, SIG_IGN);
  RK_MPI_SYS_Init();
  media::PacketQueue::Init();
  app::MainApp app(argc, argv);
  app.initMainView();
  app::MainApp::exec();

  c_RkRgaDeInit();
  return 0;
}
