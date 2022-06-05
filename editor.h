#pragma once

#include <termios.h>
#include <unistd.h>

#include <cctype>

#include "debug.h"
#include "terminal_util.h"
#include "utility.h"

struct Editor {
  int cursorX{0};
  int cursorY{0};

  Editor() {}

  void init() {
    preserveTermiosOriginalState();
    enableRawMode();
  }

  void runLoop() {
    char c;

    for (;;) {
      refreshScreen();

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

      if (c == ctrlKey('s')) cursorY++;
      if (c == ctrlKey('w')) cursorY--;
      if (c == ctrlKey('a')) cursorX--;
      if (c == ctrlKey('d')) cursorX++;

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

  void refreshScreen() {
    hideCursor();
    clearScreen();
    drawLineDecoration();
    setCursorLocation(cursorY, cursorX);
    showCursor();
  }
};
