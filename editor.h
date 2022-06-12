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

  int cursorX{0};
  int cursorY{0};
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
    dlog("Loading file: %s", fileName.c_str());

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
    dlog("Save file: %s", config.fileName.value().c_str());
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
        dlog("ctrl char: %d", (u_int8_t)tc.simple());
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

            cursorTo(cursorY - 1, oldLineLen);
          }
        } else {
          dlog("Error: cannot backspace on not line row");
        }
      }

      if (tc.simple() == ENTER) {
        auto rowIt = currentLine().begin();
        advance(rowIt, cursorX);
        string newLine(rowIt, currentLine().end());

        auto rowIt2 = currentLine().begin();
        advance(rowIt2, cursorX);
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
    } else if (tc.is_escape()) {
      if (tc.escape() == EscapeChar::Down) cursorDown();
      if (tc.escape() == EscapeChar::Up) cursorUp();
      if (tc.escape() == EscapeChar::Left) cursorLeft();
      if (tc.escape() == EscapeChar::Right) cursorRight();
      if (tc.escape() == EscapeChar::Home) cursorHome();
      if (tc.escape() == EscapeChar::End) cursorEnd();
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

    cursorX = prompt.prefix.size() + prompt.message.size();
  }

  bool onLineRow() {
    return currentRow() >= 0 && currentRow() < (int)lines.size();
  }

  void cursorDown() {
    cursorY++;

    if (onLineRow()) {
      cursorX = min(cursorX, (int)currentLine().size());
    }

    fixCursorPos();
  }

  void cursorUp() {
    cursorY--;

    if (onLineRow()) {
      cursorX = min(cursorX, (int)currentLine().size());
    }

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

  void cursorTo(int row, int col) {
    cursorX = col;
    cursorY = row;
    fixCursorPos();
  }

  void cursorHome() { cursorX = 0; }

  void cursorEnd() { cursorX = currentLine().size(); }

  void fixCursorPos() {
    if (cursorY >= (int)lines.size()) cursorY = lines.size() - 1;
    if (cursorY >= terminalRows()) {
      cursorY = terminalRows() - 1;

      if (currentRow() < (int)lines.size() - 1) verticalScroll++;
    }
    if (cursorY < 0) {
      cursorY = 0;

      if (verticalScroll > 0) verticalScroll--;
    }
    // Now cursorY is either on a line or on 0 when there are no lines.

    if (cursorY < (int)lines.size()) {
      if (cursorX > (int)currentLine().size()) {
        if (currentRow() < (int)lines.size() - 1) {
          if (cursorY >= terminalRows() - 1) {
            verticalScroll++;
          } else {
            cursorY++;
          }
          cursorX = 0;
        } else {
          cursorX = currentLine().size();
        }
      }
      if (cursorX >= terminalCols()) cursorX = terminalCols() - 1;
      if (cursorX < 0) {
        if (currentRow() > 0) {
          if (cursorY <= 0) {
            if (verticalScroll > 0) {
              verticalScroll--;
              cursorX = currentLine().size();
            } else {
              dlog("Error: vscroll not suppose to be 0");
            }
          } else {
            cursorY--;
            cursorX = currentLine().size();
          }
        } else {
          cursorX = 0;
        }
      }
    } else {
      cursorX = 0;
    }
    // Now cursorX is either on a char or on 0 when there are no chars.
  }

  int currentRow() { return verticalScroll + cursorY; }
  int currentCol() { return horizontalScroll + cursorX; }

  string& currentLine() { return lines[currentRow()]; }

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
    setCursorLocation(out, cursorY, cursorX);
    showCursor(out);

    write(STDOUT_FILENO, out.c_str(), out.size());
  }

  void refreshTerminalDimension() {
    terminalDimension = getTerminalDimension();
  }

  void openPrompt(string prefix, Command command) {
    dlog("prompt open cx: %d", cursorX);

    mode = EditorMode::Prompt;
    prompt.reset(prefix, command, cursorX, cursorY);
    cursorX = prompt.prefix.size() + prompt.message.size();
    cursorY = terminalRows() - 1;
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
    cursorX = prompt.previousCursorX;
    cursorY = prompt.previousCursorY;

    dlog("prompt close cx: %d", cursorX);
  }
};
