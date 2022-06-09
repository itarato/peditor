#include <cstdlib>

#include "config.h"
#include "debug.h"
#include "editor.h"

using namespace std;

int main(int argc, char** argv) {
  dlog("peditor start");

  Config config{};

  if (argc == 2) {
    config.setFileName(argv[1]);
  }

  Editor editor{config};

  editor.init();
  editor.runLoop();

  dlog("peditor end");

  return EXIT_SUCCESS;
}
