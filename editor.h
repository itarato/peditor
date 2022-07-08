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

  Point cursor{};

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
    activeTextView()->cursorTo(0, 0);
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
          activeTextView()->deleteLine();
          break;
        case ctrlKey('z'):
          activeTextView()->undo();
          break;
        case ctrlKey('r'):
          activeTextView()->redo();
          break;
        case ctrlKey('c'):
          copyClipboardInternal();
          break;
        case ctrlKey('v'):
          pasteClipboardInternal();
          break;
        case ctrlKey('x'):
          activeTextView()->toggleSelection();
          break;
        case ctrlKey('n'):
          jumpToNextSearchHit();
          break;
        case ctrlKey('b'):
          jumpToPrevSearchHit();
          break;
        case BACKSPACE:
          activeTextView()->insertBackspace();
          break;
        case CTRL_BACKSPACE:
          activeTextView()->insertCtrlBackspace();
          break;
        case ENTER:
          activeTextView()->insertEnter();
          break;
        case TAB:
          activeTextView()->insertTab(config.tabSize);
          break;
        default:
          if (iscntrl(tc.simple())) {
            DLOG("Unhandled simple ctrl char: %d", (u_int8_t)tc.simple());
          } else {
            activeTextView()->insertCharacter(tc.simple());
          }
          break;
      }
    } else if (tc.is_escape()) {
      switch (tc.escape()) {
        case EscapeChar::Down:
          activeTextView()->cursorDown();
          break;
        case EscapeChar::Up:
          activeTextView()->cursorUp();
          break;
        case EscapeChar::Left:
          activeTextView()->cursorLeft();
          break;
        case EscapeChar::Right:
          activeTextView()->cursorRight();
          break;
        case EscapeChar::Home:
          activeTextView()->cursorHome();
          break;
        case EscapeChar::End:
          activeTextView()->cursorEnd();
          break;
        case EscapeChar::PageUp:
          activeTextView()->cursorPageUp();
          break;
        case EscapeChar::PageDown:
          activeTextView()->cursorPageDown();
          break;
        case EscapeChar::CtrlLeft:
          activeTextView()->cursorWordJumpLeft();
          break;
        case EscapeChar::CtrlRight:
          activeTextView()->cursosWordJumpRight();
          break;
        case EscapeChar::CtrlUp:
          activeTextView()->scrollUp();
          break;
        case EscapeChar::CtrlDown:
          activeTextView()->scrollDown();
          break;
        case EscapeChar::Delete:
          activeTextView()->insertDelete();
          break;
        case EscapeChar::AltLT:
          activeTextView()->lineMoveBackward();
          break;
        case EscapeChar::AltGT:
          activeTextView()->lineMoveForward();
          break;
        case EscapeChar::AltShiftLT:
          activeTextView()->lineIndentLeft(config.tabSize);
          break;
        case EscapeChar::AltShiftGT:
          activeTextView()->lineIndentRight(config.tabSize);
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

    cursor.x = prompt.prefix.size() + prompt.message.size() + 1;
  }

  bool hasInternalClipboardContent() { return !internalClipboard.empty(); }

  void copyClipboardInternal() {
    if (!activeTextView()->hasActiveSelection()) return;

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

    activeTextView()->endSelection();
  }

  void pasteClipboardInternal() {
    if (!hasInternalClipboardContent()) return;

    for (auto it = internalClipboard.begin(); it != internalClipboard.end();
         it++) {
      if (it != internalClipboard.begin()) {
        activeTextView()->execCommand(Command::makeSplitLine(
            activeTextView()->currentRow(), activeTextView()->currentCol()));
        activeTextView()->cursorTo(activeTextView()->nextRow(), 0);
      }

      activeTextView()->execCommand(Command::makeInsertSlice(
          activeTextView()->currentRow(), activeTextView()->currentCol(), *it));
      activeTextView()->setCol(activeTextView()->currentCol() + it->size());
    }

    activeTextView()->saveXMemory();
  }

  inline void requestQuit() { quitRequested = true; }

  void jumpToNextSearchHit() {
    if (!searchTerm.has_value()) return;

    auto lineIt = activeTextView()->lines.begin();
    advance(lineIt, activeTextView()->currentRow());
    size_t from = activeTextView()->currentCol() + 1;

    while (lineIt != activeTextView()->lines.end()) {
      size_t pos = lineIt->find(searchTerm.value(), from);

      if (pos == string::npos) {
        lineIt++;
        from = 0;
      } else {
        activeTextView()->cursorTo(
            distance(activeTextView()->lines.begin(), lineIt), pos);
        return;
      }
    }
  }

  void jumpToPrevSearchHit() {
    if (!searchTerm.has_value()) return;

    auto lineIt = activeTextView()->lines.rbegin();
    advance(lineIt, activeTextView()->lines.size() -
                        activeTextView()->currentRow() - 1);
    size_t from;

    if (activeTextView()->currentCol() == 0) {
      lineIt++;
      from = lineIt->size();
    } else {
      from = activeTextView()->currentCol() - 1;
    }

    while (lineIt != activeTextView()->lines.rend()) {
      size_t pos = lineIt->rfind(searchTerm.value(), from);

      if (pos == string::npos) {
        lineIt++;
        from = lineIt->size();
      } else {
        activeTextView()->cursorTo(
            activeTextView()->lines.size() -
                distance(activeTextView()->lines.rbegin(), lineIt) - 1,
            pos);
        return;
      }
    }
  }

  inline int terminalRows() { return terminalDimension.first; }
  inline int terminalCols() { return terminalDimension.second; }

  string decorateLine(string& line, TokenAnalyzer* ta, int lineNo) {
    string out{};

    int offset{0};
    auto lineIt = line.begin();

    vector<SyntaxColorInfo> markers = ta->colorizeTokens(line);

    auto selection = activeTextView()->lineSelectionRange(lineNo);
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
         lineNo <
         activeTextView()->verticalScroll + activeTextView()->textAreaRows();
         lineNo++) {
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
                              activeTextView()->textAreaCols());

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

  void contextAdjustEditorCursor() {
    switch (mode) {
      case EditorMode::TextEdit:
        cursor.x = activeTextView()->textAreaCursorX() + leftMargin;
        cursor.y = activeTextView()->textAreaCursorY();
        break;
      case EditorMode::Prompt:
        // Keep as is.
        break;
    }
  }

  void refreshScreen() {
    string out{};

    refreshTerminalDimension();

    // TODO: do something better here, don't check the text view - it might be
    // irrelevant.
    if (activeTextView()->textAreaCols() <= 1) return;

    hideCursor();

    clearScreen(out);
    drawLines(out);

    contextAdjustEditorCursor();
    setCursorLocation(out, cursor.y, cursor.x);

    showCursor(out);

    write(STDOUT_FILENO, out.c_str(), out.size());
  }

  void refreshTerminalDimension() {
    terminalDimension = getTerminalDimension();
    activeTextView()->rows = terminalDimension.first - bottomMargin;
    activeTextView()->cols = terminalDimension.second - leftMargin;
  }

  string generateStatusLine() {
    string out{};

    int rowPosPercentage =
        100 * activeTextView()->currentRow() / activeTextView()->lines.size();

    char buf[2048];
    sprintf(
        buf,
        " pEditor v0 | File: %s%s | Textarea: %dx%d | Cursor: %dx %dy | %d%%",
        activeTextView()->fileName.value_or("<no file>").c_str(),
        (activeTextView()->isDirty ? " \x1b[94m(edited)\x1b[39m" : ""),
        activeTextView()->textAreaCols(), activeTextView()->textAreaRows(),
        activeTextView()->textAreaCursorX(),
        activeTextView()->textAreaCursorY(), rowPosPercentage);

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
    cursor.x = prompt.prefix.size() + prompt.message.size() + 1;
    cursor.y = terminalRows() - 1;
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
    activeTextView()->setTextAreaCursorX(prompt.previousCursorX);
    activeTextView()->setTextAreaCursorY(prompt.previousCursorY);
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

      activeTextView()->cursorTo(lineNo, activeTextView()->currentCol());
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

  void updateMargins() {
    leftMargin = max(1, (int)ceil(log10(activeTextView()->lines.size()))) + 1;
  }
};
