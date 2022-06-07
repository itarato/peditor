#include <cstdlib>

#include "debug.h"
#include "editor.h"

using namespace std;

int main(int argc, char** argv) {
  dlog("peditor start");

  Editor editor{};
  editor.init();

  if (argc == 2) {
    editor.loadFile(argv[1]);
  }

  editor.runLoop();

  dlog("peditor end");

  return EXIT_SUCCESS;
}
