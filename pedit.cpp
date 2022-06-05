#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <utility>

#include "editor.h"
#include "terminal_util.h"
#include "utility.h"

using namespace std;

int main() {
  Editor editor{};
  editor.init();
  editor.runLoop();

  return EXIT_SUCCESS;
}
