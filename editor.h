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
#include <sstream>
#include <string>
#include <vector>

#include "command.h"
#include "config.h"
#include "debug.h"
#include "file_watcher.h"
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
  OpenFile,
  MultiPurpose,
  FileHasBeenModified,
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

  vector<string> internalClipboard{};

  bool isDirty{false};

  FileWatcher fileWatcher{};

  Editor(Config config) : config(config) {}

  void init() {
    preserveTermiosOriginalState();
    enableRawMode();
    refreshTerminalDimension();

    // To have a non zero content to begin with.
    lines.emplace_back("");

    loadFile();
  }

  void loadFile() {
    if (!config.fileName.has_value()) {
      DLOG("Cannot load file - config does not have any.");
      return;
    }

    DLOG("Loading file: %s", config.fileName.value().c_str());

    ifstream f(config.fileName.value());

    if (!f.is_open()) {
      DLOG("File %s does not exists. Creating one.",
           config.fileName.value().c_str());
      f.open("a");
    } else {
      lines.clear();
      for (string line; getline(f, line);) {
        lines.emplace_back(line);
      }
    }

    f.close();

    fileWatcher.watch(config.fileName.value());

    config.reloadKeywordList();
    if (lines.empty()) lines.emplace_back("");
    cursorTo(0, 0);
  }

  void saveFile() {
    DLOG("Save file: %s", config.fileName.value().c_str());
    ofstream f(config.fileName.value(), ios::out | ios::trunc);

    for (int i = 0; i < (int)lines.size(); i++) {
      f << lines[i];

      if (i < (int)lines.size()) {
        f << endl;
      }
    }

    f.close();

    fileWatcher.ignoreEventCycle();

    isDirty = false;
  }

  void runLoop() {
    TypedChar tc;

    while (!quitRequested) {
      updateMargins();

      refreshScreen();

      if (fileWatcher.hasBeenModified()) {
        openPrompt("File change detected, press (r) for reload > ",
                   PromptCommand::FileHasBeenModified);
        continue;
      }

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
        case ctrlKey('w'):
          openPrompt("Safe file to > ", PromptCommand::SaveFileAs);
          break;
        case ctrlKey('o'):
          openPrompt("Open file > ", PromptCommand::OpenFile);
          break;
        case ctrlKey('p'):
          openPrompt("> ", PromptCommand::MultiPurpose);
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
        case ctrlKey('c'):
          copyClipboardInternal();
          break;
        case ctrlKey('v'):
          pasteClipboardInternal();
          break;
        case ctrlKey('x'):
          toggleSelection();
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
        if (tc.simple() == BACKSPACE && !prompt.message.empty())
          prompt.message.pop_back();
      } else {
        prompt.message.push_back(tc.simple());
      }
    }

    __cursorX = prompt.prefix.size() + prompt.message.size() + 1;
  }

  bool onLineRow() {
    return currentRow() >= 0 && currentRow() < (int)lines.size();
  }

  void insertCharacter(char c) {
    if (hasActiveSelection()) insertBackspace();

    if (currentRow() < (int)lines.size() && currentCol() <= currentLineSize()) {
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
    } else if (currentCol() <= currentLineSize() && currentCol() > 0) {
      execCommand(Command::makeDeleteChar(currentRow(), currentCol() - 1,
                                          currentLine()[currentCol() - 1]));
      cursorLeft();
    } else if (currentCol() == 0 && currentRow() > 0) {
      int oldLineLen = lines[currentRow() - 1].size();
      execCommand(Command::makeMergeLine(previousRow(), previousLine().size()));
      cursorTo(previousRow(), oldLineLen);
    }
  }

  void insertCtrlBackspace() {
    if (hasActiveSelection()) {
      insertBackspace();
    } else if (currentCol() > 0) {
      int colStart = prevWordJumpLocation(currentLine(), currentCol()) + 1;
      if (currentCol() - colStart >= 0) {
        execCommand(Command::makeDeleteSlice(
            currentRow(), colStart,
            currentLine().substr(colStart, currentCol() - colStart)));

        setCol(colStart);
      }
    } else {
      cursorLeft();
    }

    saveXMemory();
  }

  void insertDelete() {
    if (hasActiveSelection()) {
      insertBackspace();
    } else if (currentCol() < currentLineSize()) {
      execCommand(Command::makeDeleteChar(currentRow(), currentCol(),
                                          currentLine()[currentCol()]));
    } else if (currentRow() < (int)lines.size() - 1) {
      execCommand(Command::makeMergeLine(currentRow(), currentCol()));
    }
  }

  void insertEnter() {
    if (hasActiveSelection()) insertBackspace();

    execCommand(Command::makeSplitLine(currentRow(), currentCol()));

    int tabsLen = prefixTabOrSpaceLength(currentLine());
    if (tabsLen > 0) {
      string tabs(tabsLen, ' ');
      execCommand(Command::makeInsertSlice(nextRow(), 0, tabs));
    }

    setCol(tabsLen);
    saveXMemory();
    cursorDown();
  }

  void insertTab() {
    if (currentCol() <= currentLineSize()) {
      int spacesToFill = config.tabSize - (currentCol() % config.tabSize);

      if (spacesToFill > 0) {
        string tabs(spacesToFill, ' ');
        execCommand(Command::makeInsertSlice(currentRow(), currentCol(), tabs));
        setCol(currentCol() + spacesToFill);
      }
    }
  }

  void deleteLine() {
    if (lines.size() == 1) {
      execCommand(Command::makeDeleteSlice(0, 0, lines[0]));
    } else {
      execCommand(Command::makeDeleteLine(currentRow(), currentLine()));
    }

    if (currentRow() >= (int)lines.size()) {
      cursorUp();
    } else {
      setCol(currentCol());
    }
  }

  bool hasInternalClipboardContent() { return !internalClipboard.empty(); }

  void copyClipboardInternal() {
    if (!hasActiveSelection()) return;

    SelectionRange selection{selectionStart.value(), selectionEnd.value()};
    vector<LineSelection> lineSelections = selection.lineSelections();

    internalClipboard.clear();

    for (auto& lineSelection : lineSelections) {
      if (lineSelection.isFullLine()) {
        internalClipboard.push_back(lines[selection.startRow]);
      } else {
        int start = lineSelection.isLeftBounded() ? lineSelection.startCol : 0;
        int end = lineSelection.isRightBounded()
                      ? lineSelection.endCol
                      : lines[lineSelection.lineNo].size();
        internalClipboard.push_back(
            lines[selection.startRow].substr(start, end - start));
      }
    }

    endSelection();
  }

  void pasteClipboardInternal() {
    if (!hasInternalClipboardContent()) return;

    for (auto it = internalClipboard.begin(); it != internalClipboard.end();
         it++) {
      if (it != internalClipboard.begin()) {
        execCommand(Command::makeSplitLine(currentRow(), currentCol()));
        cursorTo(nextRow(), 0);
      }

      execCommand(Command::makeInsertSlice(currentRow(), currentCol(), *it));
      setCol(currentCol() + it->size());
    }

    saveXMemory();
  }

  void execCommand(Command&& cmd) {
    undos.push_back(cmd);
    DLOG("EXEC cmd: %d", cmd.type);

    TextManipulator::execute(&cmd, &lines);
    isDirty = true;

    redos.clear();
    while (undos.size() > UNDO_LIMIT) undos.pop_front();
  }

  void undo() {
    if (undos.empty()) return;

    Command cmd = undos.back();
    undos.pop_back();

    TextManipulator::reverse(&cmd, &lines);
    fixCursorPos();

    redos.push_back(cmd);
  }

  void redo() {
    if (redos.empty()) return;

    Command cmd = redos.back();
    redos.pop_back();

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

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();

    saveXMemory();
  }

  void cursosWordJumpRight() {
    if (isEndOfCurrentLine()) {
      cursorRight();
    } else {
      setCol(nextWordJumpLocation(currentLine(), currentCol()));
    }

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();

    saveXMemory();
  }

  void cursorDown() {
    __cursorY++;
    restoreXMemory();
    fixCursorPos();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();
  }

  void cursorUp() {
    __cursorY--;
    restoreXMemory();
    fixCursorPos();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();
  }

  void cursorLeft() {
    __cursorX--;

    if (currentCol() < 0) {
      __cursorY--;
      if (onLineRow()) setTextAreaCursorX(currentLine().size());
    }

    fixCursorPos();
    saveXMemory();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();
  }

  void cursorRight() {
    __cursorX++;

    if (currentCol() > currentLineSize()) {
      __cursorY++;
      if (onLineRow()) setTextAreaCursorX(0);
    }

    fixCursorPos();
    saveXMemory();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();
  }

  void cursorPageDown() {
    __cursorY += textAreaRows();
    restoreXMemory();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();

    fixCursorPos();
  }

  void cursorPageUp() {
    __cursorY -= textAreaRows();
    restoreXMemory();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();

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
    __cursorY += row - currentRow();

    fixCursorPos();
  }

  void cursorHome() {
    setCol(0);
    saveXMemory();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();
  }

  void cursorEnd() {
    setCol(currentLine().size());
    saveXMemory();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();
  }

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
    } else if (currentCol() > currentLineSize()) {
      __cursorX -= currentCol() - currentLineSize();
    }

    // Fix vertical scroll.
    if (__cursorY < 0) {
      verticalScroll = currentRow();
      setTextAreaCursorY(0);
    } else if (textAreaCursorY() >= textAreaRows()) {
      verticalScroll = currentRow() - textAreaRows() + 1;
      setTextAreaCursorY(textAreaRows() - 1);
    }

    // DLOG("BEFORE TAX: %d TAW: %d CX: %d", textAreaCursorX(), textAreaCols(),
    //      __cursorX);

    // Fix horizontal scroll.
    if (textAreaCursorX() < 0) {
      horizontalScroll = currentCol();
      setTextAreaCursorX(0);
    } else if (textAreaCursorX() >= textAreaCols()) {
      horizontalScroll = currentCol() - textAreaCols() + 1;
      setTextAreaCursorX(textAreaCols() - 1);

      // DLOG("AFTER TAX: %d TAW: %d CX: %d HS: %d", textAreaCursorX(),
      //      textAreaCols(), __cursorX, horizontalScroll);
    }
  }

  inline int currentRow() { return verticalScroll + __cursorY; }
  inline int previousRow() { return verticalScroll + __cursorY - 1; }
  inline int nextRow() { return verticalScroll + __cursorY + 1; }

  // Text area related -> TODO: rename
  inline int currentCol() { return horizontalScroll + textAreaCursorX(); }

  inline string& currentLine() { return lines[currentRow()]; }
  inline string& previousLine() { return lines[previousRow()]; }
  inline string& nextLine() { return lines[nextRow()]; }
  inline int currentLineSize() { return currentLine().size(); }

  inline void restoreXMemory() { setTextAreaCursorX(xMemory); }
  inline void saveXMemory() { xMemory = textAreaCursorX(); }

  inline bool isEndOfCurrentLine() { return currentCol() >= currentLineSize(); }
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
  inline void setTextAreaCursorY(int newTextAreaCursor) {
    // There is no top margin currently;
    __cursorY = newTextAreaCursor;
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

    SyntaxHighlightConfig syntaxHighlightConfig{&config.keywords};
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

        if (!decoratedLine.empty()) {
          pair<int, int> visibleBorders =
              visibleStrSlice(decoratedLine, horizontalScroll, textAreaCols());

          if (visibleBorders.first == -1) {
            out.append("\x1b[2m<\x1b[22m");
          } else {
            out.append(decoratedLine.substr(
                visibleBorders.first,
                visibleBorders.second - visibleBorders.first + 1));
          }
        }
      } else {
        out.push_back('~');
      }

      out.append("\n\r");
    }

    switch (mode) {
      case EditorMode::Prompt:
        out.append("\x1b[0m\x1b[44m");
        out.append("\x1b[97m ");
        out.append(prompt.prefix);
        out.append(prompt.message);
        out.append(string(
            terminalCols() - prompt.prefix.size() - prompt.message.size() - 1,
            ' '));
        out.append("\x1b[0m");
        break;
      case EditorMode::TextEdit:
        string statusLine = generateStatusLine();
        out.append("\x1b[0m\x1b[7m");
        out.append(statusLine);
        out.append("\x1b[0m");
        break;
    }
  }

  void refreshScreen() {
    string out{};

    refreshTerminalDimension();

    if (textAreaCols() <= 1) return;

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
    sprintf(
        buf,
        " pEditor v0 | File: %s%s | Textarea: %dx%d | Cursor: %dx %dy | %d%%",
        config.fileName.value_or("<no file>").c_str(),
        (isDirty ? " \x1b[94m(edited)\x1b[39m" : ""), textAreaCols(),
        textAreaRows(), textAreaCursorX(), textAreaCursorY(), rowPosPercentage);

    out.append(buf);

    int visibleLen = visibleCharCount(out);

    if (visibleLen > terminalCols()) {
      int adjustedSafeLen = visibleStrRightCut(out, terminalCols());

      out.erase(adjustedSafeLen, out.size() - adjustedSafeLen);
    } else {
      string filler(terminalCols() - visibleLen, ' ');
      out.append(filler);
    }

    return out;
  }

  void openPrompt(string prefix, PromptCommand command) {
    mode = EditorMode::Prompt;
    prompt.reset(prefix, command, __cursorX, __cursorY);
    __cursorX = prompt.prefix.size() + prompt.message.size() + 1;
    __cursorY = terminalRows() - 1;
  }

  void finalizeAndClosePrompt() {
    closePrompt();

    switch (prompt.command) {
      case PromptCommand::SaveFileAs:
        config.fileName = optional<string>(prompt.message);
        saveFile();
        break;
      case PromptCommand::OpenFile:
        config.fileName = optional<string>(prompt.message);
        loadFile();
        break;
      case PromptCommand::MultiPurpose:
        executeMultiPurposeCommand(prompt.message);
        break;
      case PromptCommand::FileHasBeenModified:
        executeFileHasBeenModifiedPrompt(prompt.message);
        break;
      case PromptCommand::Nothing:
        break;
    }
  }

  void closePrompt() {
    mode = EditorMode::TextEdit;
    __cursorX = prompt.previousCursorX;
    __cursorY = prompt.previousCursorY;

    DLOG("prompt close cx: %d", __cursorX);
  }

  void executeMultiPurposeCommand(string& raw) {
    istringstream iss{raw};
    string topCommand;
    iss >> topCommand;

    if (topCommand == "quit" || topCommand == "exit") {
      requestQuit();
    } else if (topCommand == "tab") {
      int tabSize;
      iss >> tabSize;

      config.tabSize = tabSize;
    } else if (topCommand == "line" || topCommand == "l") {
      int lineNo;
      iss >> lineNo;

      DLOG("Line jump to: %d", lineNo);

      cursorTo(lineNo, currentCol());
    } else {
      DLOG("Top command <%s> not recognized", topCommand.c_str());
    }
  }

  void executeFileHasBeenModifiedPrompt(string& cmd) {
    if (cmd != "r") return;

    loadFile();
  }

  void setCol(int newCol) {
    // Fix line position first.
    if (newCol > currentLineSize()) {
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

  bool hasActiveSelection() {
    return selectionStart.has_value() && selectionEnd.has_value();
  }
  void toggleSelection() {
    hasActiveSelection() ? endSelection() : startSelectionInCurrentPosition();
  }
  void startSelectionInCurrentPosition() {
    selectionStart = optional<SelectionEdge>({currentRow(), currentCol()});
    endSelectionUpdatePositionToCurrent();
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
