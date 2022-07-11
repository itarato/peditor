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
  Point previousCursor{};

  void reset(string newPrefix, PromptCommand newCommand,
             Point newPreviousCursor) {
    prefix = newPrefix;
    message.clear();
    command = newCommand;
    previousCursor = newPreviousCursor;
  }
};

struct Editor {
  Config config;

  int leftMargin{0};
  int bottomMargin{1};
  int topMargin{0};

  Point cursor{};

  pair<int, int> terminalDimension{};

  bool quitRequested{false};

  EditorMode mode{EditorMode::TextEdit};

  Prompt prompt{};

  vector<string> clipboard{};

  optional<string> searchTerm{nullopt};

  vector<TextView> textViews{};
  int activeTextViewIdx{0};

  Editor(Config config) : config(config) {}

  void init() {
    textViews.emplace_back(textViewCols(), textViewRows());

    preserveTermiosOriginalState();
    enableRawMode();
    refreshTerminalDimension();

    activeTextView()->reloadContent();
  }

  inline TextView* activeTextView() { return &(textViews[activeTextViewIdx]); }

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

  void changeActiveView(int idx) { activeTextViewIdx = idx % textViews.size(); }

  void runLoop() {
    TypedChar tc;

    while (!quitRequested) {
      updateMargins();

      refreshScreen();

      if (activeTextView()->fileWatcher.hasBeenModified()) {
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
        openPrompt("Open file > ", PromptCommand::OpenFile);
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

  inline void requestQuit() { quitRequested = true; }

  void jumpToNextSearchHit() {
    if (searchTerm.has_value())
      activeTextView()->jumpToNextSearchHit(searchTerm.value());
  }

  void jumpToPrevSearchHit() {
    if (searchTerm.has_value())
      activeTextView()->jumpToPrevSearchHit(searchTerm.value());
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

    if (textViews.size() > 1) out.append(generateTextViewsTabsLine());

    SyntaxHighlightConfig syntaxHighlightConfig{&activeTextView()->keywords};
    TokenAnalyzer ta{syntaxHighlightConfig};

    for (int lineNo = activeTextView()->verticalScroll;
         lineNo < activeTextView()->verticalScroll + activeTextView()->rows;
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
                              activeTextView()->cols);

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

      clearRestOfLine(out);
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
        cursor.x = activeTextView()->cursor.x + leftMargin;
        cursor.y = activeTextView()->cursor.y + topMargin;
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
    if (activeTextView()->cols <= 1) return;

    hideCursor(out);
    drawLines(out);

    contextAdjustEditorCursor();
    setCursorLocation(out, cursor.y, cursor.x);

    showCursor(out);

    write(STDOUT_FILENO, out.c_str(), out.size());
  }

  void refreshTerminalDimension() {
    terminalDimension = getTerminalDimension();
    activeTextView()->rows = textViewRows();
    activeTextView()->cols = textViewCols();
  }

  inline int textViewRows() const {
    return terminalDimension.first - bottomMargin - topMargin;
  }

  inline int textViewCols() const {
    return terminalDimension.second - leftMargin;
  }

  string generateStatusLine() {
    string out{};

    int rowPosPercentage =
        100 * activeTextView()->currentRow() / activeTextView()->lines.size();

    char buf[2048];
    sprintf(
        buf,
        " pEditor v0 | File: %s%s | Textarea: %dx%d | Cursor: %dx %dy | %d%%",
        activeTextView()->filePath.value_or("<no file>").c_str(),
        (activeTextView()->isDirty ? " \x1b[94m(edited)\x1b[39m" : ""),
        textViewCols(), textViewRows(), activeTextView()->cursor.x,
        activeTextView()->cursor.y, rowPosPercentage);

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

  string generateTextViewsTabsLine() {
    string out{};

    int maxTitleSize = terminalCols() / textViews.size();

    out.append("\x1b[7m\x1b[90m");

    if (maxTitleSize < 5) {
      // TODO add proper
      out.append("-");
    } else {
      for (int i = 0; i < (int)textViews.size(); i++) {
        if (i == activeTextViewIdx) {
          out.append("\x1b[39m ");
        } else {
          out.append("\x1b[90m ");
        }
        // TODO only use filename part
        out.append(textViews[i]
                       .fileName()
                       .value_or("<no file>")
                       .substr(0, maxTitleSize - 3));

        if (i < (int)textViews.size() - 1) {
          out.append(" \x1b[90m:");
        } else {
          out.append(" \x1b[90m");
        }
      }
    }

    string suffix(terminalCols() - visibleCharCount(out), ' ');
    out.append(suffix);

    out.append("\x1b[0m");
    out.append("\n\r");

    return out;
  }

  void openPrompt(string prefix, PromptCommand command) {
    mode = EditorMode::Prompt;
    prompt.reset(prefix, command, activeTextView()->cursor);
    cursor.x = prompt.prefix.size() + prompt.message.size() + 1;
    cursor.y = terminalRows() - 1;
  }

  void finalizeAndClosePrompt() {
    closePrompt();

    switch (prompt.command) {
      case PromptCommand::SaveFileAs:
        activeTextView()->filePath = optional<string>(prompt.message);
        saveFile();
        break;
      case PromptCommand::OpenFile:
        loadFile(prompt.message);
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
    activeTextView()->cursor = prompt.previousCursor;
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
      activeTextView()->closeFile();
    } else if (topCommand == "new" || topCommand == "n") {
      newTextView();

      string filePath;
      iss >> filePath;
      if (!filePath.empty()) loadFile(filePath);
    } else if (topCommand == "view" || "v") {
      int idx;
      iss >> idx;

      activeTextViewIdx = idx % textViews.size();
    } else {
      DLOG("Top command <%s> not recognized", topCommand.c_str());
    }
  }

  void executeFileHasBeenModifiedPrompt(string& cmd) {
    if (cmd != "r") return;

    activeTextView()->reloadContent();
  }

  inline void updateMargins() {
    leftMargin = max(1, (int)ceil(log10(activeTextView()->lines.size()))) + 1;
    topMargin = textViews.size() > 1 ? 1 : 0;
  }

  void newTextView() {
    textViews.emplace_back(textViewCols(), textViewRows());
    activeTextViewIdx = textViews.size() - 1;
    activeTextView()->reloadContent();
  }
};
