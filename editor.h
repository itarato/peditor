#pragma once

#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "config.h"
#include "debug.h"
#include "terminal_util.h"
#include "utility.h"

using namespace std;

enum class EditorMode {
  TextEdit,
  Prompt,
};

enum class Command {
  Nothing,
  SaveFileAs,
};

struct Prompt {
  string prefix{};
  Command command{Command::Nothing};
  string message{};
  int previousCursorX{0};
  int previousCursorY{0};

  void reset(string newPrefix, Command newCommand, int newPreviousCursorX,
             int newPreviousCursorY) {
    prefix = newPrefix;
    message.clear();
    command = newCommand;
    previousCursorX = newPreviousCursorX;
    previousCursorY = newPreviousCursorY;
  }
};

struct Editor {
  Config config;

  int __cursorX{0};
  int __cursorY{0};
  int verticalScroll{0};
  int horizontalScroll{0};

  pair<int, int> terminalDimension{};

  vector<string> lines{};

  bool quitRequested{false};

  EditorMode mode{EditorMode::TextEdit};

  Prompt prompt{};

  Editor(Config config) : config(config) {}

  void init() {
    preserveTermiosOriginalState();
    enableRawMode();
    refreshTerminalDimension();

    // To have a non zero content to begin with.
    lines.emplace_back("");

    if (config.fileName.has_value()) {
      loadFile(config.fileName.value());
    }
  }

  void loadFile(string fileName) {
    DLOG("Loading file: %s", fileName.c_str());

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

  void saveFile() {
    DLOG("Save file: %s", config.fileName.value().c_str());
    ofstream f(config.fileName.value(), ios::out | ios::trunc);

    for (int i = 0; i < (int)lines.size(); i++) {
      f << lines[i];

      if (i < (int)lines.size() - 1) {
        f << endl;
      }
    }

    f.close();
  }

  void runLoop() {
    TypedChar tc;

    while (!quitRequested) {
      refreshScreen();
      refreshTerminalDimension();

      tc = readKey();
      if (tc.is_failure()) continue;

      switch (mode) {
        case EditorMode::TextEdit:
          executeTextEditInput(tc);
          break;
        case EditorMode::Prompt:
          executePrompt(tc);
          break;
      }
    }

    clearScreen();
    resetCursorLocation();
  }

  void executeTextEditInput(TypedChar tc) {
    if (tc.is_simple()) {
      if (iscntrl(tc.simple())) {
        // Ignore for now.
        DLOG("ctrl char: %d", (u_int8_t)tc.simple());
      } else {
        if (currentRow() < (int)lines.size() &&
            currentCol() <= (int)currentLine().size()) {
          currentLine().insert(currentCol(), 1, tc.simple());
          cursorRight();
        }
      }

      if (tc.simple() == ctrlKey('q')) quitRequested = true;
      if (tc.simple() == ctrlKey('s')) saveFile();
      if (tc.simple() == ctrlKey('o')) {
        openPrompt("Filename > ", Command::SaveFileAs);
      }

      if (tc.simple() == BACKSPACE) {
        if (onLineRow()) {
          if (currentCol() <= (int)currentLine().size() && currentCol() > 0) {
            currentLine().erase(currentCol() - 1, 1);
            cursorLeft();
          } else if (currentCol() == 0 && currentRow() > 0) {
            int oldLineLen = lines[currentRow() - 1].size();

            lines[currentRow() - 1].append(currentLine());

            auto lineIt = lines.begin();
            advance(lineIt, currentRow());
            lines.erase(lineIt);

            cursorTo(__cursorY - 1, oldLineLen);
          }
        } else {
          DLOG("Error: cannot backspace on not line row");
        }
      }

      if (tc.simple() == CTRL_BACKSPACE) {
        if (onLineRow()) {
          if (currentCol() > 0) {
            int colStart =
                prevWordJumpLocation(currentLine(), currentCol()) + 1;
            if (currentCol() - colStart >= 0) {
              currentLine().erase(colStart, currentCol() - colStart);
              setCol(colStart);
            }
          } else {
            cursorLeft();
          }
        }
      }

      if (tc.simple() == ENTER) {
        auto rowIt = currentLine().begin();
        advance(rowIt, currentCol());
        string newLine(rowIt, currentLine().end());

        auto rowIt2 = currentLine().begin();
        advance(rowIt2, currentCol());
        currentLine().erase(rowIt2, currentLine().end());

        auto lineIt = lines.begin();
        advance(lineIt, currentRow() + 1);
        lines.insert(lineIt, newLine);

        cursorHome();
        cursorDown();
      }

      if (tc.simple() == TAB) {
        if (onLineRow() && currentCol() <= (int)currentLine().size()) {
          int spacesToFill = config.tabSize - (currentCol() % config.tabSize);
          for (int i = 0; i < spacesToFill; i++) {
            currentLine().insert(currentCol(), 1, ' ');
            cursorRight();
          }
        }
      }

      // Delete line.
      if (tc.simple() == ctrlKey('d')) {
        auto lineIt = lines.begin();
        advance(lineIt, currentRow());
        lines.erase(lineIt);

        if (lines.size() == 0) {
          lines.emplace_back("");
        } else if (currentRow() >= (int)lines.size()) {
          setCol(currentCol());
          cursorUp();
        } else {
          setCol(currentCol());
        }
      }
    } else if (tc.is_escape()) {
      if (tc.escape() == EscapeChar::Down) cursorDown();
      if (tc.escape() == EscapeChar::Up) cursorUp();
      if (tc.escape() == EscapeChar::Left) cursorLeft();
      if (tc.escape() == EscapeChar::Right) cursorRight();
      if (tc.escape() == EscapeChar::Home) cursorHome();
      if (tc.escape() == EscapeChar::End) cursorEnd();

      if (tc.escape() == EscapeChar::CtrlLeft) {
        if (isBeginningOfCurrentLine()) {
          cursorLeft();
        } else {
          setCol(prevWordJumpLocation(currentLine(), currentCol()));
        }
      }

      if (tc.escape() == EscapeChar::CtrlRight) {
        if (isEndOfCurrentLine()) {
          cursorRight();
        } else {
          setCol(nextWordJumpLocation(currentLine(), currentCol()));
        }
      }
    }
  }

  void executePrompt(TypedChar tc) {
    if (tc.is_simple()) {
      if (iscntrl(tc.simple())) {
        if (tc.simple() == ESCAPE) {
          closePrompt();
          return;
        }
        if (tc.simple() == ENTER) {
          finalizeAndClosePrompt();
          return;
        }
        if (tc.simple() == BACKSPACE) prompt.message.pop_back();
      } else {
        prompt.message.push_back(tc.simple());
      }
    }

    __cursorX = prompt.prefix.size() + prompt.message.size();
  }

  bool onLineRow() {
    return currentRow() >= 0 && currentRow() < (int)lines.size();
  }

  void cursorDown() {
    __cursorY++;
    fixCursorPos();
  }

  void cursorUp() {
    __cursorY--;
    fixCursorPos();
  }

  void cursorLeft() {
    __cursorX--;

    if (currentCol() < 0) {
      __cursorY--;
      if (onLineRow()) __cursorX = currentLine().size();
    }

    fixCursorPos();
  }

  void cursorRight() {
    __cursorX++;

    if (currentCol() > (int)currentLine().size()) {
      __cursorY++;
      if (onLineRow()) __cursorX = 0;
    }

    fixCursorPos();
  }

  void cursorTo(int row, int col) {
    __cursorX = col;
    __cursorY = row;

    fixCursorPos();
  }

  void cursorHome() { setCol(0); }

  void cursorEnd() { setCol(currentLine().size()); }

  void fixCursorPos() {
    DLOG("FIX TH=%d CY=%d VS=%d", terminalRows(), __cursorY, verticalScroll);

    // Decide which line (row) we should be on.
    if (currentRow() < 0) {
      verticalScroll = 0;
      __cursorY = 0;

      DLOG("A -> CY=%d VS=%d", __cursorY, verticalScroll);
    } else if (currentRow() >= (int)lines.size()) {
      __cursorY -= currentRow() - (int)lines.size() + 1;

      DLOG("B -> CY=%d VS=%d", __cursorY, verticalScroll);
    }

    // Decide which char (col).
    if (currentCol() < 0) {
      __cursorX = 0;
      horizontalScroll = 0;
    } else if (currentCol() > (int)currentLine().size()) {
      __cursorX -= currentCol() - currentLine().size();
    }

    // Fix vertical scroll.
    if (__cursorY < 0) {
      verticalScroll = currentRow();
      __cursorY = 0;

      DLOG("E -> CY=%d VS=%d", __cursorY, verticalScroll);
    } else if (__cursorY >= terminalRows()) {
      verticalScroll = currentRow() - terminalRows() + 1;
      __cursorY = terminalRows() - 1;

      DLOG("F -> CY=%d VS=%d", __cursorY, verticalScroll);
    }

    // Fix horizontal scroll.
    if (__cursorX < 0) {
      horizontalScroll = currentCol();
      __cursorX = 0;
    } else if (__cursorX > terminalCols()) {
      horizontalScroll = currentCol() - terminalCols() - 1;
      __cursorX = terminalCols() - 1;
    }
  }

  int currentRow() { return verticalScroll + __cursorY; }
  int currentCol() { return horizontalScroll + __cursorX; }

  string& currentLine() { return lines[currentRow()]; }

  bool isEndOfCurrentLine() {
    return currentCol() >= (int)currentLine().size();
  }
  bool isBeginningOfCurrentLine() { return currentCol() <= 0; }

  int terminalRows() { return terminalDimension.first; }
  int terminalCols() { return terminalDimension.second; }

  string decorateLine(string& line, vector<SyntaxColorInfo> colors) {
    string out{};

    int offset{0};
    auto lineIt = line.begin();

    for (auto& color : colors) {
      string prefix = "\x1b[";
      prefix.append(color.colorCode);
      prefix.push_back('m');

      string suffix{"\x1b[39m"};

      copy(lineIt, lineIt + (color.start - offset), back_inserter(out));
      advance(lineIt, color.start - offset);
      offset = color.end + 1;

      out.append(prefix);

      copy(lineIt, lineIt + (color.end - color.start + 1), back_inserter(out));
      advance(lineIt, color.end - color.start + 1);

      out.append(suffix);
    }

    copy(lineIt, line.end(), back_inserter(out));

    return out;
  }

  void drawLines(string& out) {
    resetCursorLocation(out);

    SyntaxHighlightConfig syntaxHighlightConfig;
    TokenAnalyzer ta{syntaxHighlightConfig};

    for (int lineNo = verticalScroll; lineNo < verticalScroll + terminalRows();
         lineNo++) {
      if (size_t(lineNo) < lines.size()) {
        // TODO: include horizontalScroll
        string& line = lines[lineNo];
        string decoratedLine = decorateLine(line, ta.colorizeTokens(line));

        out.append(decoratedLine);
      } else {
        out.push_back('~');
      }

      if (lineNo < verticalScroll + terminalRows() - 1) {
        out.append("\n\r");
      }

      if (mode == EditorMode::Prompt) {
        if (lineNo == verticalScroll + terminalRows() - 2) {
          out.append(prompt.prefix);
          out.append(prompt.message);
          break;
        }
      }
    }
  }

  void refreshScreen() {
    string out{};

    hideCursor();
    clearScreen(out);
    drawLines(out);
    setCursorLocation(out, __cursorY, __cursorX);
    showCursor(out);

    write(STDOUT_FILENO, out.c_str(), out.size());
  }

  void refreshTerminalDimension() {
    terminalDimension = getTerminalDimension();
  }

  void openPrompt(string prefix, Command command) {
    DLOG("prompt open cx: %d", __cursorX);

    mode = EditorMode::Prompt;
    prompt.reset(prefix, command, __cursorX, __cursorY);
    __cursorX = prompt.prefix.size() + prompt.message.size();
    __cursorY = terminalRows() - 1;
  }

  void finalizeAndClosePrompt() {
    switch (prompt.command) {
      case Command::SaveFileAs:
        config.fileName = optional<string>(prompt.message);
        saveFile();
        break;
      case Command::Nothing:
        break;
    }

    closePrompt();
  }

  void closePrompt() {
    mode = EditorMode::TextEdit;
    __cursorX = prompt.previousCursorX;
    __cursorY = prompt.previousCursorY;

    DLOG("prompt close cx: %d", __cursorX);
  }

  void setCol(int newCol) {
    // Fix line position first.
    if (newCol > (int)currentLine().size()) {
      newCol = currentLine().size();
    } else if (newCol < 0) {
      newCol = 0;
    }

    // Fix horizontal scroll.
    if (horizontalScroll > newCol) {
      horizontalScroll = newCol;
    } else if (horizontalScroll + terminalCols() < newCol) {
      horizontalScroll = newCol - terminalCols() + 1;
    }

    __cursorX = newCol - horizontalScroll;
  }
};
