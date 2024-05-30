#pragma once

#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "command.h"
#include "config.h"
#include "debug.h"
#include "file_watcher.h"
#include "prompt.h"
#include "split_unit.h"
#include "terminal_util.h"
#include "text_manipulator.h"
#include "text_view.h"
#include "utility.h"

using namespace std;

enum class EditorMode {
  TextEdit,
  Prompt,
};

struct Editor {
  Config config;

  int leftMargin{0};
  int bottomMargin{1};
  int topMargin{0};

  vector<SplitUnit> splitUnits;
  int activeSplitUnitIdx{0};

  Point cursor{};

  pair<int, int> terminalDimension{};

  bool quitRequested{false};

  EditorMode mode{EditorMode::TextEdit};

  Prompt prompt{};

  vector<string> clipboard{};

  optional<string> searchTerm{nullopt};

  Editor(Config config) : config(config) {
  }

  void init() {
    preserveTermiosOriginalState();
    enableRawMode();

    updateDimensions();

    newSplitUnit();
  }

  inline SplitUnit* activeSplitUnit() {
    return &(splitUnits[activeSplitUnitIdx]);
  }
  inline TextView* activeTextView() {
    return activeSplitUnit()->activeTextView();
  }

  void saveFile() {
    if (activeTextView()->filePath.has_value()) {
      activeTextView()->saveFile();
    } else {
      openPrompt("New file needs a name > ", PromptCommand::SaveFileAs);
    }
  }

  void loadFile(string filePath) {
    if (filePath.empty()) return;

    activeTextView()->loadFile(filePath);
  }

  void changeActiveView(int idx) {
    activeSplitUnit()->setActiveTextViewIdx(idx);
  }

  void setActiveSplitUnit(int idx) {
    activeSplitUnitIdx = (idx + splitUnits.size()) % splitUnits.size();
  }

  void runLoop() {
    TypedChar tc;

    while (!quitRequested) {
      refreshScreen();

      if (activeTextView()->fileWatcher.hasBeenModified()) {
        openPrompt("File change detected, press (r) for reload > ", PromptCommand::FileHasBeenModified);
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
    TextEditorAction action = config.textEditorActionForKeystroke(tc);

    switch (action) {
      case TextEditorAction::Quit:
        requestQuit();
        break;
      case TextEditorAction::SaveFile:
        saveFile();
        break;
      case TextEditorAction::SaveFileAs:
        openPrompt("Safe file to > ", PromptCommand::SaveFileAs);
        break;
      case TextEditorAction::OpenFile:
        executeOpenFile();
        break;
      case TextEditorAction::MultiPurposeCommand:
        openPrompt("> ", PromptCommand::MultiPurpose);
        break;
      case TextEditorAction::DeleteLine:
        activeTextView()->deleteLine();
        break;
      case TextEditorAction::Undo:
        activeTextView()->undo();
        break;
      case TextEditorAction::Redo:
        activeTextView()->redo();
        break;
      case TextEditorAction::Copy:
        activeTextView()->clipboardCopy(clipboard);
        break;
      case TextEditorAction::Paste:
        activeTextView()->clipboardPaste(clipboard);
        break;
      case TextEditorAction::SelectionToggle:
        activeTextView()->toggleSelection();
        break;
      case TextEditorAction::JumpNextSearchHit:
        jumpToNextSearchHit();
        break;
      case TextEditorAction::JumpPrevSearchHit:
        jumpToPrevSearchHit();
        break;
      case TextEditorAction::Backspace:
        activeTextView()->insertBackspace();
        break;
      case TextEditorAction::WordBackspace:
        activeTextView()->insertCtrlBackspace();
        break;
      case TextEditorAction::Enter:
        activeTextView()->insertEnter();
        break;
      case TextEditorAction::Tab:
        activeTextView()->insertTab(config.tabSize);
        break;
      case TextEditorAction::Type:
        if (iscntrl(tc.simple())) {
          DLOG("Unhandled simple ctrl char: %d", (u_int8_t)tc.simple());
        } else {
          activeTextView()->insertCharacter(tc.simple());
        }
        break;
      case TextEditorAction::CursorDown:
        activeTextView()->cursorDown();
        break;
      case TextEditorAction::CursorUp:
        activeTextView()->cursorUp();
        break;
      case TextEditorAction::CursorLeft:
        activeTextView()->cursorLeft();
        break;
      case TextEditorAction::CursorRight:
        activeTextView()->cursorRight();
        break;
      case TextEditorAction::CursorHome:
        activeTextView()->cursorHome();
        break;
      case TextEditorAction::CursorEnd:
        activeTextView()->cursorEnd();
        break;
      case TextEditorAction::CursorPageUp:
        activeTextView()->cursorPageUp();
        break;
      case TextEditorAction::CursorPageDown:
        activeTextView()->cursorPageDown();
        break;
      case TextEditorAction::CursorWordJumpLeft:
        activeTextView()->cursorWordJumpLeft();
        break;
      case TextEditorAction::CursosWordJumpRight:
        activeTextView()->cursosWordJumpRight();
        break;
      case TextEditorAction::SplitUnitToPrev:
        setActiveSplitUnit(activeSplitUnitIdx - 1);
        break;
      case TextEditorAction::SplitUnitToNext:
        setActiveSplitUnit(activeSplitUnitIdx + 1);
        break;
      case TextEditorAction::ScrollUp:
        activeTextView()->scrollUp();
        break;
      case TextEditorAction::ScrollDown:
        activeTextView()->scrollDown();
        break;
      case TextEditorAction::InsertDelete:
        activeTextView()->insertDelete();
        break;
      case TextEditorAction::LineIndentLeft:
        activeTextView()->lineIndentLeft(config.tabSize);
        break;
      case TextEditorAction::LineIndentRight:
        activeTextView()->lineIndentRight(config.tabSize);
        break;
      case TextEditorAction::LineMoveBackward:
        activeTextView()->lineMoveBackward();
        break;
      case TextEditorAction::LineMoveForward:
        activeTextView()->lineMoveForward();
        break;
      case TextEditorAction::NewTextView:
        newTextView();
        break;
      case TextEditorAction::ChangeActiveView0:
        changeActiveView(0);
        break;
      case TextEditorAction::ChangeActiveView1:
        changeActiveView(1);
        break;
      case TextEditorAction::ChangeActiveView2:
        changeActiveView(2);
        break;
      case TextEditorAction::ChangeActiveView3:
        changeActiveView(3);
        break;
      case TextEditorAction::ChangeActiveView4:
        changeActiveView(4);
        break;
      case TextEditorAction::ChangeActiveView5:
        changeActiveView(5);
        break;
      case TextEditorAction::ChangeActiveView6:
        changeActiveView(6);
        break;
      case TextEditorAction::ChangeActiveView7:
        changeActiveView(7);
        break;
      case TextEditorAction::ChangeActiveView8:
        changeActiveView(8);
        break;
      case TextEditorAction::ChangeActiveView9:
        changeActiveView(9);
        break;
      case TextEditorAction::NewSplitUnit:
        newSplitUnit();
        break;
      case TextEditorAction::CloseTextView:
        closeTextView();
        break;
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
        if (tc.simple() == BACKSPACE && !prompt.rawMessage.empty()) prompt.rawMessage.pop_back();
      } else {
        prompt.rawMessage.push_back(tc.simple());
      }
    }

    cursor.x = prompt.prefix.size() + prompt.messageVisibleSize() + 1;
  }

  inline void requestQuit() {
    quitRequested = true;
  }

  void jumpToNextSearchHit() {
    if (searchTerm.has_value()) activeTextView()->jumpToNextSearchHit(searchTerm.value());
  }

  void jumpToPrevSearchHit() {
    if (searchTerm.has_value()) activeTextView()->jumpToPrevSearchHit(searchTerm.value());
  }

  inline int terminalRows() const {
    return terminalDimension.first;
  }
  inline int terminalCols() const {
    return terminalDimension.second;
  }

  void drawLines(string& out) {
    for (int lineIdx = 0; lineIdx < textViewRows(); lineIdx++) {
      for (int i = 0; i < (int)splitUnits.size(); i++) {
        splitUnits[i].drawLine(out, lineIdx, searchTerm);

        if (i < (int)splitUnits.size() - 1) {
          out.append("\x1b[2m\x1b[90m|\x1b[0m");
        }
      }
      out.append("\n\r");
    }

    switch (mode) {
      case EditorMode::Prompt:
        out.append("\x1b[0m\x1b[44m");
        out.append("\x1b[97m ");
        out.append(prompt.prefix);
        out.append(prompt.message(true));
        out.append(string(terminalCols() - prompt.prefix.size() - prompt.messageVisibleSize() - 1, ' '));
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
    if (mode == EditorMode::TextEdit) {
      int currentSplitUnitIdx = 0;
      int splitUnitXOffset = 0;
      while (currentSplitUnitIdx < activeSplitUnitIdx) {
        splitUnitXOffset += textViewCols(currentSplitUnitIdx++) + 1;
      }

      cursor.x = splitUnitXOffset + activeTextView()->cursor.x + leftMargin + activeTextView()->leftMargin;
      cursor.y = activeTextView()->cursor.y + activeSplitUnit()->topMargin + topMargin;
    } else if (mode == EditorMode::Prompt) {
      // Keep as is.
    } else {
      reportAndExit("Missing mode handling");
    }
  }

  void refreshScreen() {
    string out{};

    updateDimensions();

    // TODO: do something better here, don't check the text view - it might be
    // irrelevant. Make all drawing (split unit, text view, etc) self aware of
    // this - and skip if cannot draw.
    if (activeTextView()->cols <= 1) return;

    hideCursor(out);
    resetCursorLocation(out);
    drawLines(out);

    contextAdjustEditorCursor();
    setCursorLocation(out, cursor.y, cursor.x);

    showCursor(out);

    ssize_t res = write(STDOUT_FILENO, out.c_str(), out.size());
    assert(res >= 0);
  }

  inline int textViewRows() const {
    return terminalDimension.first - bottomMargin - topMargin;
  }
  inline int textViewCols(int idx) const {
    // -1 to account to borders
    int nonLastSplitUnitCols = splitAreaCols() / splitUnits.size() - 1;

    if (idx == (int)splitUnits.size() - 1) {
      return splitAreaCols() - ((nonLastSplitUnitCols + 1) * (splitUnits.size() - 1));
    } else {
      return nonLastSplitUnitCols;
    }

    return splitAreaCols() / splitUnits.size();
  }
  inline int splitAreaCols() const {
    return terminalDimension.second - leftMargin;
  }

  string generateStatusLine() {
    string out{};

    int rowPosPercentage = 100 * activeTextView()->currentRow() / activeTextView()->lines.line_count;

    char buf[2048];
    sprintf(buf, " pEditor v0 | File: %s%s | Textarea: %dx%d | Cursor: %dx %dy | %d%%",
            activeTextView()->filePath.value_or("<no file>").c_str(),
            (activeTextView()->isDirty ? " \x1b[94m(edited)\x1b[39m" : ""), splitAreaCols(), textViewRows(),
            activeTextView()->cursor.x, activeTextView()->cursor.y, rowPosPercentage);

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
    prompt.reset(prefix, command);
    cursor.x = prompt.prefix.size() + prompt.messageVisibleSize() + 1;
    cursor.y = terminalRows() - 1;
  }

  void openPrompt(string prefix, PromptCommand command, vector<string>&& messageOptions) {
    mode = EditorMode::Prompt;
    prompt.reset(prefix, command, move(messageOptions));
    cursor.x = prompt.prefix.size() + prompt.messageVisibleSize() + 1;
    cursor.y = terminalRows() - 1;
  }

  void finalizeAndClosePrompt() {
    closePrompt();

    switch (prompt.command) {
      case PromptCommand::SaveFileAs:
        activeTextView()->filePath = optional<string>(prompt.message());
        saveFile();
        activeTextView()->reloadContent();
        break;
      case PromptCommand::OpenFile:
        loadFile(prompt.message());
        break;
      case PromptCommand::MultiPurpose:
        executeMultiPurposeCommand(prompt.message());
        break;
      case PromptCommand::FileHasBeenModified:
        executeFileHasBeenModifiedPrompt(prompt.message());
        break;
      case PromptCommand::Nothing:
        break;
    }
  }

  inline void closePrompt() {
    mode = EditorMode::TextEdit;
  }

  void executeOpenFile() {
    openPrompt("Open file > ", PromptCommand::OpenFile, directoryFiles());
  }

  void executeMultiPurposeCommand(string raw) {
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

      activeTextView()->cursorTo(lineNo, activeTextView()->currentCol());
    } else if (topCommand == "search" || topCommand == "s") {
      string term;
      iss >> term;

      if (term.empty()) {
        searchTerm = nullopt;
      } else {
        searchTerm = term;
        jumpToNextSearchHit();
      }
    } else if (topCommand == "close" || topCommand == "c") {
      activeTextView()->closeFile();
    } else if (topCommand == "new" || topCommand == "n") {
      newTextView();

      string filePath;
      iss >> filePath;
      if (!filePath.empty()) loadFile(filePath);
    } else if (topCommand == "view" || "v") {
      int idx;
      iss >> idx;

      changeActiveView(idx);
    } else {
      DLOG("Top command <%s> not recognized", topCommand.c_str());
    }
  }

  void executeFileHasBeenModifiedPrompt(string cmd) {
    if (cmd != "r") return;

    activeTextView()->reloadContent();
  }

  void updateDimensions() {
    terminalDimension = getTerminalDimension();
    leftMargin = 0;
    topMargin = 0;
    bottomMargin = 1;

    for (int i = 0; i < (int)splitUnits.size(); i++) {
      splitUnits[i].updateDimensions(textViewCols(i), textViewRows(), hasMultipleSplitUnits());
    }
  }

  inline bool hasMultipleSplitUnits() {
    return splitUnits.size() > 1;
  }

  inline void newTextView() {
    activeSplitUnit()->newTextView();
  }

  void closeTextView() {
    if (activeSplitUnit()->hasMultipleTabs()) {
      activeSplitUnit()->closeTextView();
    } else {
      closeSplitUnit();
    }
  }

  void newSplitUnit() {
    splitUnits.emplace_back();

    setActiveSplitUnit(splitUnits.size() - 1);

    updateDimensions();
  }

  void closeSplitUnit() {
    if (splitUnits.size() <= 1) {
      requestQuit();
      return;
    }

    auto splitIt = splitUnits.begin();
    advance(splitIt, activeSplitUnitIdx);
    splitUnits.erase(splitIt);

    updateDimensions();
    setActiveSplitUnit(activeSplitUnitIdx - 1);
  }
};
