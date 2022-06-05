#include <cstdlib>

#include "debug.h"
#include "editor.h"

using namespace std;

int main() {
  dlog("peditor start");

  Editor editor{};
  editor.init();
  editor.runLoop();

  dlog("peditor end");

  return EXIT_SUCCESS;
}
