#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>

#include <cstdlib>

#include "config.h"
#include "debug.h"
#include "editor.h"

using namespace std;

static Editor* peditor;
static void sigwinchHandler(int sig) {
  if (peditor) peditor->refreshScreen();
}

void initTerminalWindowChangeWatch() {
  struct sigaction sa;

  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = sigwinchHandler;

  if (sigaction(SIGWINCH, &sa, NULL) == -1)
    DLOG("SIGWINCH cannot be monitored");
}

int main(int argc, char** argv) {
  DLOG("peditor start");

  Config config{};

  if (argc == 2) config.setFileName(argv[1]);

  Editor editor{config};
  peditor = &editor;

  initTerminalWindowChangeWatch();

  editor.init();
  editor.runLoop();

  DLOG("peditor end");

  return EXIT_SUCCESS;
}
