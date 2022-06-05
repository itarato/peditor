#pragma once

#include <termios.h>
#include <unistd.h>

#include <cctype>

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

      c = readKey();

      if (iscntrl(c)) {
        // Ignore for now.
        printf("CTRL(%d)", uint(c));
      } else {
        printf("%c", c);
      }

      if (c == ctrlKey('q')) {
        break;
      }

      fflush(STDIN_FILENO);
    }
  }
};
