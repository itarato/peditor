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
    TypedChar tc;

    for (;;) {
      refreshScreen();

      tc = readKey();

      if (tc.is_simple()) {
        if (iscntrl(tc.simple())) {
          // Ignore for now.
          dlog("ctrl char: %d", uint(tc.simple()));
        } else {
          printf("%c", tc.simple());
          dlog("char: %c", tc.simple());
        }

        if (tc.simple() == ctrlKey('q')) {
          break;
        }
      } else if (tc.is_escape()) {
        auto dim = terminalDimension();

        if (tc.escape() == EscapeChar::Down && cursorY < dim.first - 1)
          cursorY++;
        if (tc.escape() == EscapeChar::Up && cursorY > 0) cursorY--;
        if (tc.escape() == EscapeChar::Left && cursorX > 0) cursorX--;
        if (tc.escape() == EscapeChar::Right && cursorX < dim.second - 1)
          cursorX++;
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

  void refreshScreen() {
    hideCursor();
    clearScreen();
    drawLineDecoration();
    setCursorLocation(cursorY, cursorX);
    showCursor();
  }
};
