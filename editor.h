#pragma once

#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

#include "command.h"
#include "config.h"
#include "debug.h"
#include "terminal_util.h"
#include "text_manipulator.h"
#include "utility.h"

using namespace std;

#define UNDO_LIMIT 64

enum class EditorMode {
  TextEdit,
  Prompt,
};

enum class PromptCommand {
  Nothing,
  SaveFileAs,
};

struct Prompt {
  string prefix{};
  PromptCommand command{PromptCommand::Nothing};
  string message{};
  int previousCursorX{0};
  int previousCursorY{0};

  void reset(string newPrefix, PromptCommand newCommand, int newPreviousCursorX,
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
  int xMemory{0};
  int leftMargin{0};
  int bottomMargin{1};

  pair<int, int> terminalDimension{};

  vector<string> lines{};

  bool quitRequested{false};

  EditorMode mode{EditorMode::TextEdit};

  Prompt prompt{};

  optional<SelectionEdge> selectionStart{nullopt};
  optional<SelectionEdge> selectionEnd{nullopt};

  deque<Command> undos;
  deque<Command> redos;

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

    if (lines.empty()) lines.emplace_back("");
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
      updateMargins();

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
      switch (tc.simple()) {
        case ctrlKey('q'):
          requestQuit();
          break;
        case ctrlKey('s'):
          saveFile();
          break;
        case ctrlKey('o'):
          openPrompt("Filename > ", PromptCommand::SaveFileAs);
          break;
        case ctrlKey('d'):
          deleteLine();
          break;
        case ctrlKey('z'):
          undo();
          break;
        case ctrlKey('r'):
          redo();
          break;
        case BACKSPACE:
          insertBackspace();
          break;
        case CTRL_BACKSPACE:
          insertCtrlBackspace();
          break;
        case ENTER:
          insertEnter();
          break;
        case TAB:
          insertTab();
          break;
        default:
          if (iscntrl(tc.simple())) {
            DLOG("Unhandled simple ctrl char: %d", (u_int8_t)tc.simple());
          } else {
            insertCharacter(tc.simple());
          }
          break;
      }
    } else if (tc.is_escape()) {
      switch (tc.escape()) {
        case EscapeChar::Down:
          cursorDown();
          break;
        case EscapeChar::Up:
          cursorUp();
          break;
        case EscapeChar::Left:
          cursorLeft();
          break;
        case EscapeChar::Right:
          cursorRight();
          break;
        case EscapeChar::Home:
          cursorHome();
          break;
        case EscapeChar::End:
          cursorEnd();
          break;
        case EscapeChar::PageUp:
          cursorPageUp();
          break;
        case EscapeChar::PageDown:
          cursorPageDown();
          break;
        case EscapeChar::CtrlLeft:
          cursorWordJumpLeft();
          break;
        case EscapeChar::CtrlRight:
          cursosWordJumpRight();
          break;
        case EscapeChar::CtrlUp:
          scrollUp();
          break;
        case EscapeChar::CtrlDown:
          scrollDown();
          break;
        case EscapeChar::ShiftUp:
          cursorSelectUp();
          break;
        case EscapeChar::ShiftDown:
          cursorSelectDown();
          break;
        case EscapeChar::ShiftLeft:
          cursorSelectLeft();
          break;
        case EscapeChar::ShiftRight:
          cursorSelectRight();
          break;
        case EscapeChar::Delete:
          insertDelete();
          break;
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

  void insertCharacter(char c) {
    if (hasActiveSelection()) insertBackspace();

    if (currentRow() < (int)lines.size() &&
        currentCol() <= (int)currentLine().size()) {
      execCommand(Command::makeInsertChar(currentRow(), currentCol(), c));

      cursorRight();
    }
  }

  inline void requestQuit() { quitRequested = true; }

  void insertBackspace() {
    if (hasActiveSelection()) {
      SelectionRange selection{selectionStart.value(), selectionEnd.value()};

      // Remove all lines
      vector<LineSelection> lineSelections = selection.lineSelections();

      int lineAdjustmentOffset{0};

      for (auto& lineSelection : lineSelections) {
        if (lineSelection.isFullLine()) {
          lineAdjustmentOffset++;
          execCommand(Command::makeDeleteLine(selection.startRow + 1,
                                              lines[selection.startRow + 1]));
        } else {
          int start =
              lineSelection.isLeftBounded() ? lineSelection.startCol : 0;
          int end =
              lineSelection.isRightBounded()
                  ? lineSelection.endCol
                  : lines[lineSelection.lineNo - lineAdjustmentOffset].size();
          execCommand(Command::makeDeleteSlice(
              lineSelection.lineNo - lineAdjustmentOffset, start,
              lines[lineSelection.lineNo - lineAdjustmentOffset].substr(
                  start, end - start)));
        }
      }

      if (selection.isMultiline()) {
        execCommand(Command::makeMergeLine(selection.startRow,
                                           lines[selection.startRow].size()));
      }

      // Put cursor to beginning
      cursorTo(selection.startRow, selection.startCol);
      endSelection();
    } else if (currentCol() <= (int)currentLine().size() && currentCol() > 0) {
      execCommand(Command::makeDeleteChar(currentRow(), currentCol() - 1,
                                          currentLine()[currentCol() - 1]));
      cursorLeft();
    } else if (currentCol() == 0 && currentRow() > 0) {
      int oldLineLen = lines[currentRow() - 1].size();
      execCommand(Command::makeMergeLine(previousRow(), previousLine().size()));
      cursorTo(__cursorY - 1, oldLineLen);
    }
  }

  void insertCtrlBackspace() {
    if (currentCol() > 0) {
      int colStart = prevWordJumpLocation(currentLine(), currentCol()) + 1;
      if (currentCol() - colStart >= 0) {
        currentLine().erase(colStart, currentCol() - colStart);
        setCol(colStart);
      }
    } else {
      cursorLeft();
    }

    saveXMemory();
  }

  void insertDelete() {
    if (hasActiveSelection()) insertBackspace();

    if (currentCol() < (int)currentLine().size()) {
      currentLine().erase(currentCol(), 1);
    } else if (currentRow() < (int)lines.size() - 1) {
      currentLine().append(lines[currentRow() + 1]);

      auto lineIt = lines.begin();
      advance(lineIt, currentRow() + 1);
      lines.erase(lineIt);
    }
  }

  void insertEnter() {
    if (hasActiveSelection()) insertBackspace();

    auto rowIt = currentLine().begin();
    advance(rowIt, currentCol());
    string newLine(rowIt, currentLine().end());

    auto rowIt2 = currentLine().begin();
    advance(rowIt2, currentCol());
    currentLine().erase(rowIt2, currentLine().end());

    int tabsLen = prefixTabOrSpaceLength(currentLine());
    if (tabsLen > 0) newLine.insert(0, tabsLen, ' ');

    auto lineIt = lines.begin();
    advance(lineIt, currentRow() + 1);
    lines.insert(lineIt, newLine);

    setCol(tabsLen);
    saveXMemory();
    cursorDown();
  }

  void insertTab() {
    if (currentCol() <= (int)currentLine().size()) {
      int spacesToFill = config.tabSize - (currentCol() % config.tabSize);
      for (int i = 0; i < spacesToFill; i++) {
        currentLine().insert(currentCol(), 1, ' ');
        cursorRight();
      }
    }
  }

  void deleteLine() {
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

  void execCommand(Command&& cmd) {
    undos.push_back(cmd);
    DLOG("EXEC cmd: %d", cmd.type);

    TextManipulator::execute(&cmd, &lines);

    redos.clear();
    while (undos.size() > UNDO_LIMIT) undos.pop_front();
  }

  void undo() {
    if (undos.empty()) return;

    Command cmd = undos.back();
    undos.pop_back();

    DLOG("UNDO cmd: %d", cmd.type);

    TextManipulator::reverse(&cmd, &lines);
    fixCursorPos();

    redos.push_back(cmd);
  }

  void redo() {
    if (redos.empty()) return;

    Command cmd = redos.back();
    redos.pop_back();

    DLOG("REDO cmd: %d", cmd.type);

    TextManipulator::execute(&cmd, &lines);
    fixCursorPos();

    undos.push_back(cmd);
  }

  void cursorWordJumpLeft() {
    if (isBeginningOfCurrentLine()) {
      cursorLeft();
    } else {
      setCol(prevWordJumpLocation(currentLine(), currentCol()));
    }
  }

  void cursosWordJumpRight() {
    if (isEndOfCurrentLine()) {
      cursorRight();
    } else {
      setCol(nextWordJumpLocation(currentLine(), currentCol()));
    }
  }

  void cursorSelectUp() {
    if (!hasActiveSelection()) startSelectionInCurrentPosition();
    cursorUp(true);
    endSelectionUpdatePositionToCurrent();
  }

  void cursorSelectDown() {
    if (!hasActiveSelection()) startSelectionInCurrentPosition();
    cursorDown(true);
    endSelectionUpdatePositionToCurrent();
  }

  void cursorSelectLeft() {
    if (!hasActiveSelection()) startSelectionInCurrentPosition();
    cursorLeft(true);
    endSelectionUpdatePositionToCurrent();
  }

  void cursorSelectRight() {
    if (!hasActiveSelection()) startSelectionInCurrentPosition();
    cursorRight(true);
    endSelectionUpdatePositionToCurrent();
  }

  void cursorDown(bool keepSelection = false) {
    __cursorY++;
    restoreXMemory();
    fixCursorPos();

    if (!keepSelection) endSelection();
  }

  void cursorUp(bool keepSelection = false) {
    __cursorY--;
    restoreXMemory();
    fixCursorPos();

    if (!keepSelection) endSelection();
  }

  void cursorLeft(bool keepSelection = false) {
    __cursorX--;

    if (currentCol() < 0) {
      __cursorY--;
      if (onLineRow()) setTextAreaCursorX(currentLine().size());
    }

    fixCursorPos();
    saveXMemory();

    if (!keepSelection) endSelection();
  }

  void cursorRight(bool keepSelection = false) {
    __cursorX++;

    if (currentCol() > (int)currentLine().size()) {
      __cursorY++;
      if (onLineRow()) setTextAreaCursorX(0);
    }

    fixCursorPos();
    saveXMemory();

    if (!keepSelection) endSelection();
  }

  void cursorPageDown() {
    __cursorY += textAreaRows();
    restoreXMemory();
    fixCursorPos();
  }

  void cursorPageUp() {
    __cursorY -= textAreaRows();
    restoreXMemory();
    fixCursorPos();
  }

  void scrollUp() {
    verticalScroll--;
    fixCursorPos();
  }

  void scrollDown() {
    verticalScroll++;
    fixCursorPos();
  }

  void cursorTo(int row, int col) {
    setTextAreaCursorX(col);
    __cursorY = row;

    fixCursorPos();
  }

  void cursorHome() {
    setCol(0);
    saveXMemory();
  }

  void cursorEnd() { setCol(currentLine().size()); }

  void fixCursorPos() {
    // Decide which line (row) we should be on.
    if (currentRow() < 0) {
      __cursorY -= currentRow();
    } else if (currentRow() >= (int)lines.size()) {
      __cursorY -= currentRow() - (int)lines.size() + 1;
    }

    // Decide which char (col).
    if (currentCol() < 0) {
      __cursorX -= currentCol();
    } else if (currentCol() > (int)currentLine().size()) {
      __cursorX -= currentCol() - currentLine().size();
    }

    // Fix vertical scroll.
    if (__cursorY < 0) {
      verticalScroll = currentRow();
      __cursorY = 0;
    } else if (__cursorY >= textAreaRows()) {
      verticalScroll = currentRow() - textAreaRows() + 1;
      __cursorY = textAreaRows() - 1;
    }

    // Fix horizontal scroll.
    if (textAreaCursorX() < 0) {
      horizontalScroll = currentCol();
      setTextAreaCursorX(0);
    } else if (textAreaCursorX() > textAreaCols()) {
      horizontalScroll = currentCol() - textAreaCols() - 1;
      setTextAreaCursorX(textAreaCols() - 1);
    }
  }

  int currentRow() { return verticalScroll + __cursorY; }
  int previousRow() { return verticalScroll + __cursorY - 1; }
  int nextRow() { return verticalScroll + __cursorY + 1; }

  // Text area related -> TODO: rename
  int currentCol() { return horizontalScroll + textAreaCursorX(); }

  inline string& currentLine() { return lines[currentRow()]; }
  inline string& previousLine() { return lines[previousRow()]; }
  inline string& nextLine() { return lines[nextRow()]; }

  inline void restoreXMemory() { setTextAreaCursorX(xMemory); }
  inline void saveXMemory() { xMemory = textAreaCursorX(); }

  inline bool isEndOfCurrentLine() {
    return currentCol() >= (int)currentLine().size();
  }
  inline bool isBeginningOfCurrentLine() { return currentCol() <= 0; }

  inline int terminalRows() { return terminalDimension.first; }
  inline int terminalCols() { return terminalDimension.second; }

  inline int textAreaCols() { return terminalCols() - leftMargin; }
  inline int textAreaRows() { return terminalRows() - bottomMargin; }
  inline int textAreaCursorX() { return __cursorX - leftMargin; }
  inline int textAreaCursorY() { return __cursorY; }
  inline void setTextAreaCursorX(int newTextAreaCursor) {
    __cursorX = newTextAreaCursor + leftMargin;
  }

  string decorateLine(string& line, TokenAnalyzer* ta, int lineNo) {
    string out{};

    int offset{0};
    auto lineIt = line.begin();

    vector<SyntaxColorInfo> markers = ta->colorizeTokens(line);

    auto selection = lineSelectionRange(lineNo);
    if (selection.has_value()) {
      markers.emplace_back(selection.value().first, BACKGROUND_REVERSE);
      markers.emplace_back(selection.value().second, RESET_REVERSE);
    }

    sort(markers.begin(), markers.end(),
         [](SyntaxColorInfo& lhs, SyntaxColorInfo& rhs) {
           return lhs.pos < rhs.pos;
         });

    for (auto& color : markers) {
      string prefix = "\x1b[";
      prefix.append(color.code);
      prefix.push_back('m');

      copy(lineIt, lineIt + (color.pos - offset), back_inserter(out));
      advance(lineIt, color.pos - offset);
      offset = color.pos;

      out.append(prefix);
    }

    copy(lineIt, line.end(), back_inserter(out));

    return out;
  }

  void drawLines(string& out) {
    resetCursorLocation(out);

    SyntaxHighlightConfig syntaxHighlightConfig;
    TokenAnalyzer ta{syntaxHighlightConfig};

    for (int lineNo = verticalScroll; lineNo < verticalScroll + textAreaRows();
         lineNo++) {
      if (size_t(lineNo) < lines.size()) {
        // TODO: include horizontalScroll
        string& line = lines[lineNo];
        string decoratedLine = decorateLine(line, &ta, lineNo);

        char formatBuf[32];
        sprintf(formatBuf, "\x1b[33m%%%dd\x1b[0m ", leftMargin - 1);

        char marginBuf[32];
        sprintf(marginBuf, formatBuf, lineNo);

        out.append(marginBuf);

        out.append(decoratedLine);
      } else {
        out.push_back('~');
      }

      out.append("\n\r");
    }

    switch (mode) {
      case EditorMode::Prompt:
        out.append(prompt.prefix);
        out.append(prompt.message);
        break;
      case EditorMode::TextEdit:
        string statusLine = generateStatusLine();
        out.append("\x1b[7m");
        out.append(statusLine);
        out.append("\x1b[27m");
        break;
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

  string generateStatusLine() {
    string out{};

    int rowPosPercentage = 100 * currentRow() / lines.size();

    char buf[2048];
    sprintf(buf,
            " pEditor v0 | File: %s | Textarea: %dx%d | Cursor: %dx %dy | %d%%",
            config.fileName.value_or("<no file>").c_str(), textAreaCols(),
            textAreaRows(), textAreaCursorX(), textAreaCursorY(),
            rowPosPercentage);

    out.append(buf);

    if ((int)out.size() > terminalCols()) {
      out.erase(terminalCols(), out.size() - terminalCols());
    } else {
      string filler(terminalCols() - (int)out.size(), ' ');
      out.append(filler);
    }

    return out;
  }

  void openPrompt(string prefix, PromptCommand command) {
    mode = EditorMode::Prompt;
    prompt.reset(prefix, command, __cursorX, __cursorY);
    __cursorX = prompt.prefix.size() + prompt.message.size();
    __cursorY = terminalRows() - 1;
  }

  void finalizeAndClosePrompt() {
    switch (prompt.command) {
      case PromptCommand::SaveFileAs:
        config.fileName = optional<string>(prompt.message);
        saveFile();
        break;
      case PromptCommand::Nothing:
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
    } else if (horizontalScroll + textAreaCols() < newCol) {
      horizontalScroll = newCol - textAreaCols() + 1;
    }

    setTextAreaCursorX(newCol - horizontalScroll);
  }

  void updateMargins() {
    int newLeftMargin = ceil(log10(lines.size() + 1)) + 1;
    __cursorX += newLeftMargin - leftMargin;
    leftMargin = newLeftMargin;
  }

  bool hasActiveSelection() { return selectionStart.has_value(); }
  void startSelectionInCurrentPosition() {
    selectionStart = optional<SelectionEdge>({currentRow(), currentCol()});
  }
  void endSelectionUpdatePositionToCurrent() {
    selectionEnd = optional<SelectionEdge>({currentRow(), currentCol()});
  }
  bool isPositionInSelection(int row, int col) {
    if (!hasActiveSelection()) return false;

    if (row < selectionStart.value().row) return false;
    if (row == selectionStart.value().row && col < selectionStart.value().col)
      return false;
    if (row == selectionEnd.value().row && col > selectionEnd.value().col)
      return false;
    if (row > selectionEnd.value().row) return false;

    return true;
  }
  optional<pair<int, int>> lineSelectionRange(int row) {
    if (!hasActiveSelection()) return nullopt;

    SelectionRange selection{selectionStart.value(), selectionEnd.value()};

    if (row < selection.startRow) return nullopt;
    if (row > selection.endRow) return nullopt;

    int start;
    int end;

    if (row == selection.startRow) {
      start = selection.startCol;
    } else {
      start = 0;
    }

    if (row == selection.endRow) {
      end = selection.endCol;
    } else {
      end = lines[row].size();
    }

    return pair<int, int>({start, end});
  }
  void endSelection() {
    selectionStart = nullopt;
    selectionEnd = nullopt;
  }
};
