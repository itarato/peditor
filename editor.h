#pragma once

#include <termios.h>
#include <unistd.h>

#include <cctype>

#include "debug.h"
#include "terminal_util.h"
#include "utility.h"

struct Editor {
  Editor() {}

  void init() {
    preserveTermiosOriginalState();
    enableRawMode();
  }

  void runLoop() {
    char c;

    for (;;) {
      clearScreen();
      drawLineDecoration();
      resetCursorLocation();

      c = readKey();

      if (iscntrl(c)) {
        // Ignore for now.
        dlog("ctrl char: %d", uint(c));
      } else {
        printf("%c", c);
        dlog("char: %c", c);
      }

      if (c == ctrlKey('q')) {
        break;
      }

      fflush(STDIN_FILENO);
    }
  }

  void drawLineDecoration() {
    pair<int, int> dim = terminalDimension();
    resetCursorLocation();

    for (int i = 0; i < dim.first; i++) {
      write(STDOUT_FILENO, "~", 1);
      if (i < dim.first - 1) {
        write(STDOUT_FILENO, "\n\r", 2);
      }
    }
  }
};
