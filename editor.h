#pragma once

#include <termios.h>
#include <unistd.h>

#include <cctype>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "debug.h"
#include "terminal_util.h"
#include "utility.h"

using namespace std;

struct Editor {
  int cursorX{0};
  int cursorY{0};
  int verticalScroll{0};

  optional<string> fileName{nullopt};
  vector<string> lines{};

  Editor() {}

  void init() {
    preserveTermiosOriginalState();
    enableRawMode();
  }

  void loadFile(const char *fileName) {
    dlog("Loading file: %s", fileName);
    this->fileName = fileName;

    ifstream f(fileName);

    if (!f.is_open()) {
      reportAndExit("Failed opening file");
    }

    lines.clear();
    for (string line; getline(f, line);) {
      lines.emplace_back(line);
    }
    f.close();
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

  void drawLines() {
    pair<int, int> dim = terminalDimension();
    resetCursorLocation();

    for (int lineNo = verticalScroll; lineNo < verticalScroll + dim.first;
         lineNo++) {
      if (size_t(lineNo) < lines.size()) {
        write(STDOUT_FILENO, lines[lineNo].c_str(), lines[lineNo].size());
      } else {
        write(STDOUT_FILENO, "~", 1);
      }

      if (lineNo < verticalScroll + dim.first - 1) {
        write(STDOUT_FILENO, "\n\r", 2);
      }
    }
  }

  void refreshScreen() {
    hideCursor();
    clearScreen();
    drawLines();
    setCursorLocation(cursorY, cursorX);
    showCursor();
  }
};
