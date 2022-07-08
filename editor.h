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
#include "text_view.h"
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

  int leftMargin{0};
  int bottomMargin{1};

  pair<int, int> terminalDimension{};

  bool quitRequested{false};

  EditorMode mode{EditorMode::TextEdit};

  Prompt prompt{};

  vector<string> internalClipboard{};

  FileWatcher fileWatcher{};

  optional<string> searchTerm{nullopt};

  vector<TextView> textViews{};

  Editor(Config config) : config(config) {}

  void init() {
    textViews.emplace_back();

    preserveTermiosOriginalState();
    enableRawMode();
    refreshTerminalDimension();

    // To have a non zero content to begin with.
    activeTextView()->lines.emplace_back("");

    loadFile();
  }

  inline TextView* activeTextView() { return &(textViews[0]); }

  void loadFile() {
    activeTextView()->lines.clear();

    if (activeTextView()->fileName.has_value()) {
      DLOG("Loading file: %s", activeTextView()->fileName.value().c_str());

      ifstream f(activeTextView()->fileName.value());

      if (!f.is_open()) {
        DLOG("File %s does not exists. Creating one.",
             activeTextView()->fileName.value().c_str());
        f.open("a");
      } else {
        for (string line; getline(f, line);) {
          activeTextView()->lines.emplace_back(line);
        }
      }

      f.close();

      fileWatcher.watch(activeTextView()->fileName.value());
    } else {
      DLOG("Cannot load file - config does not have any.");
    }

    activeTextView()->reloadKeywordList();
    if (activeTextView()->lines.empty())
      activeTextView()->lines.emplace_back("");
    cursorTo(0, 0);
  }

  void saveFile() {
    activeTextView()->saveFile();
    fileWatcher.ignoreEventCycle();
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
        case ctrlKey('n'):
          jumpToNextSearchHit();
          break;
        case ctrlKey('b'):
          jumpToPrevSearchHit();
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
        case EscapeChar::AltLT:
          lineMoveBackward();
          break;
        case EscapeChar::AltGT:
          lineMoveForward();
          break;
        case EscapeChar::AltShiftLT:
          lineIndentLeft();
          break;
        case EscapeChar::AltShiftGT:
          lineIndentRight();
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

    activeTextView()->__cursorX =
        prompt.prefix.size() + prompt.message.size() + 1;
  }

  bool onLineRow() {
    return currentRow() >= 0 &&
           currentRow() < (int)activeTextView()->lines.size();
  }

  void insertCharacter(char c) {
    if (hasActiveSelection()) insertBackspace();

    if (currentRow() < (int)activeTextView()->lines.size() &&
        currentCol() <= currentLineSize()) {
      execCommand(Command::makeInsertChar(currentRow(), currentCol(), c));

      cursorRight();
    }
  }

  inline void requestQuit() { quitRequested = true; }

  void insertBackspace() {
    if (hasActiveSelection()) {
      SelectionRange selection{activeTextView()->selectionStart.value(),
                               activeTextView()->selectionEnd.value()};

      // Remove all lines
      vector<LineSelection> lineSelections = selection.lineSelections();

      int lineAdjustmentOffset{0};

      for (auto& lineSelection : lineSelections) {
        if (lineSelection.isFullLine()) {
          lineAdjustmentOffset++;
          execCommand(Command::makeDeleteLine(
              selection.startRow + 1,
              activeTextView()->lines[selection.startRow + 1]));
        } else {
          int start =
              lineSelection.isLeftBounded() ? lineSelection.startCol : 0;
          int end =
              lineSelection.isRightBounded()
                  ? lineSelection.endCol
                  : activeTextView()
                        ->lines[lineSelection.lineNo - lineAdjustmentOffset]
                        .size();
          execCommand(Command::makeDeleteSlice(
              lineSelection.lineNo - lineAdjustmentOffset, start,
              activeTextView()
                  ->lines[lineSelection.lineNo - lineAdjustmentOffset]
                  .substr(start, end - start)));
        }
      }

      if (selection.isMultiline()) {
        execCommand(Command::makeMergeLine(
            selection.startRow,
            activeTextView()->lines[selection.startRow].size()));
      }

      // Put cursor to beginning
      cursorTo(selection.startRow, selection.startCol);
      endSelection();
    } else if (currentCol() <= currentLineSize() && currentCol() > 0) {
      execCommand(Command::makeDeleteChar(currentRow(), currentCol() - 1,
                                          currentLine()[currentCol() - 1]));
      cursorLeft();
    } else if (currentCol() == 0 && currentRow() > 0) {
      int oldLineLen = activeTextView()->lines[currentRow() - 1].size();
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
    } else if (currentRow() < (int)activeTextView()->lines.size() - 1) {
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
    if (activeTextView()->lines.size() == 1) {
      execCommand(Command::makeDeleteSlice(0, 0, activeTextView()->lines[0]));
    } else {
      execCommand(Command::makeDeleteLine(currentRow(), currentLine()));
    }

    if (currentRow() >= (int)activeTextView()->lines.size()) {
      cursorUp();
    } else {
      setCol(currentCol());
    }
  }

  bool hasInternalClipboardContent() { return !internalClipboard.empty(); }

  void copyClipboardInternal() {
    if (!hasActiveSelection()) return;

    SelectionRange selection{activeTextView()->selectionStart.value(),
                             activeTextView()->selectionEnd.value()};
    vector<LineSelection> lineSelections = selection.lineSelections();

    internalClipboard.clear();

    for (auto& lineSelection : lineSelections) {
      if (lineSelection.isFullLine()) {
        internalClipboard.push_back(
            activeTextView()->lines[selection.startRow]);
      } else {
        int start = lineSelection.isLeftBounded() ? lineSelection.startCol : 0;
        int end = lineSelection.isRightBounded()
                      ? lineSelection.endCol
                      : activeTextView()->lines[lineSelection.lineNo].size();
        internalClipboard.push_back(
            activeTextView()->lines[selection.startRow].substr(start,
                                                               end - start));
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
    activeTextView()->undos.push_back(cmd);
    DLOG("EXEC cmd: %d", cmd.type);

    TextManipulator::execute(&cmd, &activeTextView()->lines);
    activeTextView()->isDirty = true;

    activeTextView()->redos.clear();
    while (activeTextView()->undos.size() > UNDO_LIMIT)
      activeTextView()->undos.pop_front();
  }

  void undo() {
    if (activeTextView()->undos.empty()) return;

    Command cmd = activeTextView()->undos.back();
    activeTextView()->undos.pop_back();

    TextManipulator::reverse(&cmd, &activeTextView()->lines);
    fixCursorPos();

    activeTextView()->redos.push_back(cmd);
  }

  void redo() {
    if (activeTextView()->redos.empty()) return;

    Command cmd = activeTextView()->redos.back();
    activeTextView()->redos.pop_back();

    TextManipulator::execute(&cmd, &activeTextView()->lines);
    fixCursorPos();

    activeTextView()->undos.push_back(cmd);
  }

  void lineMoveForward() {
    if (hasActiveSelection()) {
      if (activeTextView()->selectionEnd.value().row >=
          (int)activeTextView()->lines.size() - 1)
        return;

      int selectionLen = activeTextView()->selectionEnd.value().row -
                         activeTextView()->selectionStart.value().row + 1;

      lineMoveForward(activeTextView()->selectionStart.value().row,
                      selectionLen);

      activeTextView()->selectionStart = {
          activeTextView()->selectionStart.value().row + 1,
          activeTextView()->selectionStart.value().col};
      activeTextView()->selectionEnd = {
          activeTextView()->selectionEnd.value().row + 1,
          activeTextView()->selectionEnd.value().col};
    } else {
      if (currentRow() >= (int)activeTextView()->lines.size() - 1) return;

      lineMoveForward(currentRow(), 1);
    }

    cursorTo(nextRow(), currentCol());
  }

  void lineMoveForward(int lineNo, int lineCount) {
    for (int offs = lineCount - 1; offs >= 0; offs--) {
      execCommand(Command::makeSwapLine(lineNo + offs));
    }
  }

  void lineMoveBackward() {
    if (hasActiveSelection()) {
      if (activeTextView()->selectionStart.value().row <= 0) return;

      int selectionLen = activeTextView()->selectionEnd.value().row -
                         activeTextView()->selectionStart.value().row + 1;

      lineMoveBackward(activeTextView()->selectionStart.value().row,
                       selectionLen);

      activeTextView()->selectionStart = {
          activeTextView()->selectionStart.value().row - 1,
          activeTextView()->selectionStart.value().col};
      activeTextView()->selectionEnd = {
          activeTextView()->selectionEnd.value().row - 1,
          activeTextView()->selectionEnd.value().col};
    } else {
      if (currentRow() <= 0) return;

      lineMoveBackward(currentRow(), 1);
    }

    cursorTo(previousRow(), currentCol());
  }

  void lineMoveBackward(int lineNo, int lineCount) {
    for (int offs = 0; offs < lineCount; offs++) {
      execCommand(Command::makeSwapLine(lineNo + offs - 1));
    }
  }

  void lineIndentRight() {
    if (hasActiveSelection()) {
      SelectionRange selection{activeTextView()->selectionStart.value(),
                               activeTextView()->selectionEnd.value()};
      vector<LineSelection> lineSelections = selection.lineSelections();
      for (auto& sel : lineSelections) {
        lineIndentRight(sel.lineNo);
      }

      activeTextView()->selectionStart = {
          activeTextView()->selectionStart.value().row,
          activeTextView()->selectionStart.value().col + config.tabSize};
      activeTextView()->selectionEnd = {
          activeTextView()->selectionEnd.value().row,
          activeTextView()->selectionEnd.value().col + config.tabSize};
    } else {
      lineIndentRight(currentRow());
    }

    setCol(currentCol() + config.tabSize);
    saveXMemory();
  }

  void lineIndentRight(int lineNo) {
    string indent(config.tabSize, ' ');
    execCommand(Command::makeInsertSlice(lineNo, 0, indent));
  }

  void lineIndentLeft() {
    if (hasActiveSelection()) {
      SelectionRange selection{activeTextView()->selectionStart.value(),
                               activeTextView()->selectionEnd.value()};
      vector<LineSelection> lineSelections = selection.lineSelections();
      for (auto& sel : lineSelections) {
        int tabsRemoved = lineIndentLeft(sel.lineNo);

        if (sel.lineNo == currentRow()) setCol(currentCol() - tabsRemoved);

        if (sel.lineNo == activeTextView()->selectionStart.value().row) {
          activeTextView()->selectionStart = {
              activeTextView()->selectionStart.value().row,
              activeTextView()->selectionStart.value().col - tabsRemoved};
        }

        if (sel.lineNo == activeTextView()->selectionEnd.value().row) {
          activeTextView()->selectionEnd = {
              activeTextView()->selectionEnd.value().row,
              activeTextView()->selectionEnd.value().col - tabsRemoved};
        }
      }

    } else {
      int tabsRemoved = lineIndentLeft(currentRow());
      setCol(currentCol() - tabsRemoved);
    }

    saveXMemory();
  }

  int lineIndentLeft(int lineNo) {
    auto& line = activeTextView()->lines[lineNo];
    auto it =
        find_if(line.begin(), line.end(), [](auto& c) { return !isspace(c); });
    int leadingTabLen = distance(line.begin(), it);
    int tabsRemoved = min(leadingTabLen, config.tabSize);

    if (tabsRemoved != 0) {
      string indent(tabsRemoved, ' ');
      execCommand(Command::makeDeleteSlice(lineNo, 0, indent));
    }

    return tabsRemoved;
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
    activeTextView()->__cursorY++;
    restoreXMemory();
    fixCursorPos();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();
  }

  void cursorUp() {
    activeTextView()->__cursorY--;
    restoreXMemory();
    fixCursorPos();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();
  }

  void cursorLeft() {
    activeTextView()->__cursorX--;

    if (currentCol() < 0) {
      activeTextView()->__cursorY--;
      if (onLineRow()) setTextAreaCursorX(currentLine().size());
    }

    fixCursorPos();
    saveXMemory();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();
  }

  void cursorRight() {
    activeTextView()->__cursorX++;

    if (currentCol() > currentLineSize()) {
      activeTextView()->__cursorY++;
      if (onLineRow()) setTextAreaCursorX(0);
    }

    fixCursorPos();
    saveXMemory();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();
  }

  void cursorPageDown() {
    activeTextView()->__cursorY += textAreaRows();
    restoreXMemory();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();

    fixCursorPos();
  }

  void cursorPageUp() {
    activeTextView()->__cursorY -= textAreaRows();
    restoreXMemory();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();

    fixCursorPos();
  }

  void scrollUp() {
    activeTextView()->verticalScroll--;
    fixCursorPos();
  }

  void scrollDown() {
    activeTextView()->verticalScroll++;
    fixCursorPos();
  }

  void cursorTo(int row, int col) {
    setTextAreaCursorX(col);
    activeTextView()->__cursorY += row - currentRow();

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

  void jumpToNextSearchHit() {
    if (!searchTerm.has_value()) return;

    auto lineIt = activeTextView()->lines.begin();
    advance(lineIt, currentRow());
    size_t from = currentCol() + 1;

    while (lineIt != activeTextView()->lines.end()) {
      size_t pos = lineIt->find(searchTerm.value(), from);

      if (pos == string::npos) {
        lineIt++;
        from = 0;
      } else {
        cursorTo(distance(activeTextView()->lines.begin(), lineIt), pos);
        return;
      }
    }
  }

  void jumpToPrevSearchHit() {
    if (!searchTerm.has_value()) return;

    auto lineIt = activeTextView()->lines.rbegin();
    advance(lineIt, activeTextView()->lines.size() - currentRow() - 1);
    size_t from;

    if (currentCol() == 0) {
      lineIt++;
      from = lineIt->size();
    } else {
      from = currentCol() - 1;
    }

    while (lineIt != activeTextView()->lines.rend()) {
      size_t pos = lineIt->rfind(searchTerm.value(), from);

      if (pos == string::npos) {
        lineIt++;
        from = lineIt->size();
      } else {
        cursorTo(activeTextView()->lines.size() -
                     distance(activeTextView()->lines.rbegin(), lineIt) - 1,
                 pos);
        return;
      }
    }
  }

  void fixCursorPos() {
    // Decide which line (row) we should be on.
    if (currentRow() < 0) {
      activeTextView()->__cursorY -= currentRow();
    } else if (currentRow() >= (int)activeTextView()->lines.size()) {
      activeTextView()->__cursorY -=
          currentRow() - (int)activeTextView()->lines.size() + 1;
    }

    // Decide which char (col).
    if (currentCol() < 0) {
      activeTextView()->__cursorX -= currentCol();
    } else if (currentCol() > currentLineSize()) {
      activeTextView()->__cursorX -= currentCol() - currentLineSize();
    }

    // Fix vertical scroll.
    if (activeTextView()->__cursorY < 0) {
      activeTextView()->verticalScroll = currentRow();
      setTextAreaCursorY(0);
    } else if (textAreaCursorY() >= textAreaRows()) {
      activeTextView()->verticalScroll = currentRow() - textAreaRows() + 1;
      setTextAreaCursorY(textAreaRows() - 1);
    }

    // Fix horizontal scroll.
    if (textAreaCursorX() < 0) {
      activeTextView()->horizontalScroll = currentCol();
      setTextAreaCursorX(0);
    } else if (textAreaCursorX() >= textAreaCols()) {
      activeTextView()->horizontalScroll = currentCol() - textAreaCols() + 1;
      setTextAreaCursorX(textAreaCols() - 1);
    }
  }

  inline int currentRow() {
    return activeTextView()->verticalScroll + activeTextView()->__cursorY;
  }
  inline int previousRow() {
    return activeTextView()->verticalScroll + activeTextView()->__cursorY - 1;
  }
  inline int nextRow() {
    return activeTextView()->verticalScroll + activeTextView()->__cursorY + 1;
  }

  // Text area related -> TODO: rename
  inline int currentCol() {
    return activeTextView()->horizontalScroll + textAreaCursorX();
  }

  inline string& currentLine() { return activeTextView()->lines[currentRow()]; }
  inline string& previousLine() {
    return activeTextView()->lines[previousRow()];
  }
  inline string& nextLine() { return activeTextView()->lines[nextRow()]; }
  inline int currentLineSize() { return currentLine().size(); }

  inline void restoreXMemory() {
    setTextAreaCursorX(activeTextView()->xMemory);
  }
  inline void saveXMemory() { activeTextView()->xMemory = textAreaCursorX(); }

  inline bool isEndOfCurrentLine() { return currentCol() >= currentLineSize(); }
  inline bool isBeginningOfCurrentLine() { return currentCol() <= 0; }

  inline int terminalRows() { return terminalDimension.first; }
  inline int terminalCols() { return terminalDimension.second; }

  inline int textAreaCols() { return terminalCols() - leftMargin; }
  inline int textAreaRows() { return terminalRows() - bottomMargin; }
  inline int textAreaCursorX() {
    return activeTextView()->__cursorX - leftMargin;
  }
  inline int textAreaCursorY() { return activeTextView()->__cursorY; }
  inline void setTextAreaCursorX(int newTextAreaCursor) {
    activeTextView()->__cursorX = newTextAreaCursor + leftMargin;
  }
  inline void setTextAreaCursorY(int newTextAreaCursor) {
    // There is no top margin currently;
    activeTextView()->__cursorY = newTextAreaCursor;
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

    if (searchTerm.has_value()) {
      auto _searchTermMarkers = searchTermMarkers(line, searchTerm.value());
      copy(_searchTermMarkers.begin(), _searchTermMarkers.end(),
           back_inserter(markers));
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

    SyntaxHighlightConfig syntaxHighlightConfig{&activeTextView()->keywords};
    TokenAnalyzer ta{syntaxHighlightConfig};

    for (int lineNo = activeTextView()->verticalScroll;
         lineNo < activeTextView()->verticalScroll + textAreaRows(); lineNo++) {
      if (size_t(lineNo) < activeTextView()->lines.size()) {
        // TODO: include horizontalScroll
        string& line = activeTextView()->lines[lineNo];
        string decoratedLine = decorateLine(line, &ta, lineNo);

        char formatBuf[32];
        sprintf(formatBuf, "\x1b[33m%%%dd\x1b[0m ", leftMargin - 1);

        char marginBuf[32];
        sprintf(marginBuf, formatBuf, lineNo);

        out.append(marginBuf);

        if (!decoratedLine.empty()) {
          pair<int, int> visibleBorders =
              visibleStrSlice(decoratedLine, activeTextView()->horizontalScroll,
                              textAreaCols());

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
    setCursorLocation(out, activeTextView()->__cursorY,
                      activeTextView()->__cursorX);
    showCursor(out);

    write(STDOUT_FILENO, out.c_str(), out.size());
  }

  void refreshTerminalDimension() {
    terminalDimension = getTerminalDimension();
  }

  string generateStatusLine() {
    string out{};

    int rowPosPercentage = 100 * currentRow() / activeTextView()->lines.size();

    char buf[2048];
    sprintf(
        buf,
        " pEditor v0 | File: %s%s | Textarea: %dx%d | Cursor: %dx %dy | %d%%",
        activeTextView()->fileName.value_or("<no file>").c_str(),
        (activeTextView()->isDirty ? " \x1b[94m(edited)\x1b[39m" : ""),
        textAreaCols(), textAreaRows(), textAreaCursorX(), textAreaCursorY(),
        rowPosPercentage);

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
    prompt.reset(prefix, command, activeTextView()->__cursorX,
                 activeTextView()->__cursorY);
    activeTextView()->__cursorX =
        prompt.prefix.size() + prompt.message.size() + 1;
    activeTextView()->__cursorY = terminalRows() - 1;
  }

  void finalizeAndClosePrompt() {
    closePrompt();

    switch (prompt.command) {
      case PromptCommand::SaveFileAs:
        activeTextView()->fileName = optional<string>(prompt.message);
        saveFile();
        break;
      case PromptCommand::OpenFile:
        activeTextView()->fileName = optional<string>(prompt.message);
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
    activeTextView()->__cursorX = prompt.previousCursorX;
    activeTextView()->__cursorY = prompt.previousCursorY;

    DLOG("prompt close cx: %d", activeTextView()->__cursorX);
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
    } else if (topCommand == "search" || topCommand == "s") {
      string term;
      // FIXME: Not just one, but get all the rest (maybe there are spaces);
      iss >> term;

      if (term.empty()) {
        searchTerm = nullopt;
      } else {
        searchTerm = term;
      }
    } else if (topCommand == "close" || topCommand == "c") {
      activeTextView()->fileName = nullopt;
      loadFile();
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
    if (activeTextView()->horizontalScroll > newCol) {
      activeTextView()->horizontalScroll = newCol;
    } else if (activeTextView()->horizontalScroll + textAreaCols() < newCol) {
      activeTextView()->horizontalScroll = newCol - textAreaCols() + 1;
    }

    setTextAreaCursorX(newCol - activeTextView()->horizontalScroll);
  }

  void updateMargins() {
    int newLeftMargin = ceil(log10(activeTextView()->lines.size() + 1)) + 1;
    activeTextView()->__cursorX += newLeftMargin - leftMargin;
    leftMargin = newLeftMargin;
  }

  bool hasActiveSelection() {
    return activeTextView()->selectionStart.has_value() &&
           activeTextView()->selectionEnd.has_value();
  }
  void toggleSelection() {
    hasActiveSelection() ? endSelection() : startSelectionInCurrentPosition();
  }
  void startSelectionInCurrentPosition() {
    activeTextView()->selectionStart =
        optional<SelectionEdge>({currentRow(), currentCol()});
    endSelectionUpdatePositionToCurrent();
  }
  void endSelectionUpdatePositionToCurrent() {
    activeTextView()->selectionEnd =
        optional<SelectionEdge>({currentRow(), currentCol()});
  }
  bool isPositionInSelection(int row, int col) {
    if (!hasActiveSelection()) return false;

    if (row < activeTextView()->selectionStart.value().row) return false;
    if (row == activeTextView()->selectionStart.value().row &&
        col < activeTextView()->selectionStart.value().col)
      return false;
    if (row == activeTextView()->selectionEnd.value().row &&
        col > activeTextView()->selectionEnd.value().col)
      return false;
    if (row > activeTextView()->selectionEnd.value().row) return false;

    return true;
  }
  optional<pair<int, int>> lineSelectionRange(int row) {
    if (!hasActiveSelection()) return nullopt;

    SelectionRange selection{activeTextView()->selectionStart.value(),
                             activeTextView()->selectionEnd.value()};

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
      end = activeTextView()->lines[row].size();
    }

    return pair<int, int>({start, end});
  }
  void endSelection() {
    activeTextView()->selectionStart = nullopt;
    activeTextView()->selectionEnd = nullopt;
  }
};
