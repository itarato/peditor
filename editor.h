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
  int horizontalScroll{0};

  pair<int, int> terminalDimension{};

  optional<string> fileName{nullopt};
  vector<string> lines{};

  Editor() {}

  void init() {
    preserveTermiosOriginalState();
    enableRawMode();
    refreshTerminalDimension();
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
      refreshTerminalDimension();

      tc = readKey();

      int currentRow{cursorY + verticalScroll};
      int currentCol{cursorX + horizontalScroll};

      if (tc.is_simple()) {
        if (iscntrl(tc.simple())) {
          // Ignore for now.
          dlog("ctrl char: %d", uint(tc.simple()));
        } else {
          // printf("%c", tc.simple());
          // dlog("char: %c", tc.simple());

          if (currentRow < lines.size() &&
              currentCol < lines[currentRow].size()) {
            lines[currentRow].insert(currentCol, 1, tc.simple());
            cursorRight();
          }
        }

        if (tc.simple() == ctrlKey('q')) {
          break;
        }
      } else if (tc.is_escape()) {
        if (tc.escape() == EscapeChar::Down) cursorDown();
        if (tc.escape() == EscapeChar::Up) cursorUp();
        if (tc.escape() == EscapeChar::Left) cursorLeft();
        if (tc.escape() == EscapeChar::Right) cursorRight();
      }

      fflush(STDIN_FILENO);
    }
  }

  void cursorDown() {
    cursorY++;
    fixCursorPos();
  }

  void cursorUp() {
    cursorY--;
    fixCursorPos();
  }

  void cursorLeft() {
    cursorX--;
    fixCursorPos();
  }

  void cursorRight() {
    cursorX++;
    fixCursorPos();
  }

  void fixCursorPos() {
    if (cursorY >= lines.size()) cursorY = lines.size() - 1;
    if (cursorY > terminalDimension.first) cursorY = terminalDimension.first;
    if (cursorY < 0) cursorY = 0;
    // Now cursorY is either on a line or on 0 when there are no lines.

    if (cursorY < lines.size()) {
      if (cursorX >= lines[cursorY].size()) cursorX = lines[cursorY].size() - 1;
      if (cursorX > terminalDimension.second)
        cursorX = terminalDimension.second;
      if (cursorX < 0) cursorX = 0;
    } else {
      cursorX = 0;
    }
    // Now cursorX is either on a char or on 0 when there are no chars.
  }

  void drawLines() {
    resetCursorLocation();

    for (int lineNo = verticalScroll;
         lineNo < verticalScroll + terminalDimension.first; lineNo++) {
      if (size_t(lineNo) < lines.size()) {
        // TODO: include horizontalScroll
        write(STDOUT_FILENO, lines[lineNo].c_str(), lines[lineNo].size());
      } else {
        write(STDOUT_FILENO, "~", 1);
      }

      if (lineNo < verticalScroll + terminalDimension.first - 1) {
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

  void refreshTerminalDimension() {
    terminalDimension = getTerminalDimension();
  }
};
