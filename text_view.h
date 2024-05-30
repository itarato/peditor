#pragma once

#include <algorithm>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "command.h"
#include "debug.h"
#include "file_watcher.h"
#include "history.h"
#include "terminal_util.h"
#include "text_manipulator.h"
#include "utility.h"

using namespace std;

static unordered_map<const char*, const char*> fileTypeAssociationMap{
    {".c++", "c++"}, {".cpp", "c++"}, {".hpp", "c++"},   {".h", "c++"},
    {".c", "c++"},   {".rb", "ruby"}, {".hs", "haskell"}};

struct TextView : ITextViewState {
  Point cursor{0, 0};

  int verticalScroll{0};
  int horizontalScroll{0};
  int xMemory{0};
  int leftMargin{0};

  optional<string> filePath{nullopt};
  // TODO: This can be redundant if multiple views exist with same file type.
  // Move to an editor mapped data.
  unordered_set<string> keywords{};

  vector<string> lines{};

  optional<SelectionEdge> selectionStart{nullopt};
  optional<SelectionEdge> selectionEnd{nullopt};

  History history{};

  FileWatcher fileWatcher{};

  vector<vector<SyntaxColorInfo>> syntaxColoring{};

  int cols{0};
  int rows{0};

  bool isDirty{true};

  TokenAnalyzer tokenAnalyzer;

  TextView() : tokenAnalyzer(TokenAnalyzer(SyntaxHighlightConfig(&keywords))) {
    reloadContent();
  }
  TextView(int cols, int rows)
      : cols(cols), rows(rows), tokenAnalyzer(TokenAnalyzer(SyntaxHighlightConfig(&keywords))) {
    reloadContent();
  }

  TextView(TextView&& other) = default;
  TextView& operator=(TextView&&) = default;

  TextView(TextView& other) = delete;
  TextView& operator=(TextView&) = delete;

  Point getCursor() {
    return cursor;
  }
  optional<SelectionEdge> getSelectionStart() {
    return selectionStart;
  }
  optional<SelectionEdge> getSelectionEnd() {
    return selectionEnd;
  }

  inline int textAreaCols() const {
    return cols - leftMargin;
  }
  inline int textAreaRows() const {
    return rows;
  }

  void reloadKeywordList() {
    keywords.clear();

    if (!filePath.has_value()) {
      DLOG("No file, cannot load keyword list.");
      return;
    }

    auto ext = filesystem::path(filePath.value()).extension();
    auto it = find_if(fileTypeAssociationMap.begin(), fileTypeAssociationMap.end(),
                      [&](auto& kv) { return kv.first == ext; });

    if (it == fileTypeAssociationMap.end()) {
      DLOG("Cannot find keyword file for extension: %s", ext.c_str());
      return;
    }

    filesystem::path keywordFilePath{"./config/keywords/"};
    keywordFilePath /= it->second;

    if (!filesystem::exists(keywordFilePath)) {
      DLOG("Missing keyword file %s", keywordFilePath.c_str());
    }

    ifstream f(keywordFilePath);

    if (!f.is_open()) reportAndExit("Failed opening file");

    for (string line; getline(f, line);) {
      keywords.insert(line);
    }
    f.close();

    tokenAnalyzer = TokenAnalyzer(SyntaxHighlightConfig(&keywords));
  }

  bool onLineRow() {
    return currentRow() >= 0 && currentRow() < (int)lines.size();
  }

  void undo() {
    if (history.undos.empty()) return;

    HistoryUnit historyUnit = history.useUndo();

    for (auto cmdIt = historyUnit.commands.rbegin(); cmdIt != historyUnit.commands.rend(); cmdIt++) {
      TextManipulator::reverse(&*cmdIt, &lines);
    }

    selectionStart = historyUnit.beforeSelectionStart;
    selectionEnd = historyUnit.beforeSelectionEnd;
    cursor = historyUnit.beforeCursor;

    reloadSyntaxColoring();
  }

  void redo() {
    if (history.redos.empty()) return;

    HistoryUnit historyUnit = history.useRedo();

    for (auto& cmd : historyUnit.commands) {
      TextManipulator::execute(&cmd, &lines);
    }

    selectionStart = historyUnit.afterSelectionStart;
    selectionEnd = historyUnit.afterSelectionEnd;
    cursor = historyUnit.afterCursor;

    reloadSyntaxColoring();
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
    cursor.y++;
    restoreXMemory();
    fixCursorPos();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();
  }

  void cursorUp() {
    cursor.y--;
    restoreXMemory();
    fixCursorPos();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();
  }

  void cursorLeft() {
    cursor.x--;

    if (currentCol() < 0) {
      cursor.y--;
      if (onLineRow()) cursor.x = currentLine().size();
    }

    fixCursorPos();
    saveXMemory();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();
  }

  void cursorRight() {
    cursor.x++;

    if (currentCol() > currentLineSize()) {
      cursor.y++;
      setCol(0);
    }

    fixCursorPos();
    saveXMemory();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();
  }

  void cursorPageDown() {
    cursor.y += textAreaRows();
    restoreXMemory();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();

    fixCursorPos();
  }

  void cursorPageUp() {
    cursor.y -= textAreaRows();
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
    cursor.x = col;
    cursor.y += row - currentRow();

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
      cursor.y -= currentRow();
    } else if (currentRow() >= (int)lines.size()) {
      cursor.y -= currentRow() - (int)lines.size() + 1;
    }

    // Decide which char (col).
    if (currentCol() < 0) {
      cursor.x -= currentCol();
    } else if (currentCol() > currentLineSize()) {
      cursor.x -= currentCol() - currentLineSize();
    }

    // Fix vertical scroll.
    if (cursor.y < 0) {
      verticalScroll = currentRow();
      cursor.y = 0;
    } else if (cursor.y >= textAreaRows()) {
      verticalScroll = currentRow() - textAreaRows() + 1;
      cursor.y = textAreaRows() - 1;
    }

    // Fix horizontal scroll.
    if (cursor.x < 0) {
      horizontalScroll = currentCol();
      cursor.x = 0;
    } else if (cursor.x >= textAreaCols()) {
      horizontalScroll = currentCol() - textAreaCols() + 1;
      cursor.x = textAreaCols() - 1;
    }
  }

  inline int currentRow() {
    return verticalScroll + cursor.y;
  }
  inline int previousRow() {
    return verticalScroll + cursor.y - 1;
  }
  inline int nextRow() {
    return verticalScroll + cursor.y + 1;
  }

  // Text area related -> TODO: rename
  inline int currentCol() {
    return horizontalScroll + cursor.x;
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

    cursor.x = newCol - horizontalScroll;
  }

  inline string& currentLine() {
    return lines[currentRow()];
  }
  inline string& previousLine() {
    return lines[previousRow()];
  }
  inline string& nextLine() {
    return lines[nextRow()];
  }
  inline int currentLineSize() {
    return currentLine().size();
  }

  inline void restoreXMemory() {
    cursor.x = xMemory;
  }
  inline void saveXMemory() {
    xMemory = cursor.x;
  }

  inline bool isEndOfCurrentLine() {
    return currentCol() >= currentLineSize();
  }
  inline bool isBeginningOfCurrentLine() {
    return currentCol() <= 0;
  }

  void execCommand(Command&& cmd) {
    TextManipulator::execute(&cmd, &lines);

    history.record(move(cmd));

    isDirty = true;

    reloadSyntaxColoring();
  }

  inline void reloadSyntaxColoring() {
    syntaxColoring = tokenAnalyzer.colorizeTokens(lines);
  }

  void clipboardCopy(vector<string>& sharedClipboard) {
    if (!hasActiveSelection()) return;

    SelectionRange selection{selectionStart.value(), selectionEnd.value()};
    vector<LineSelection> lineSelections = selection.lineSelections();

    sharedClipboard.clear();

    for (auto& lineSelection : lineSelections) {
      if (lineSelection.isFullLine()) {
        sharedClipboard.push_back(lines[lineSelection.lineNo]);
      } else {
        int start = lineSelection.isLeftBounded() ? lineSelection.startCol : 0;
        int end = lineSelection.isRightBounded() ? lineSelection.endCol : lines[lineSelection.lineNo].size();
        sharedClipboard.push_back(lines[lineSelection.lineNo].substr(start, end - start));
      }
    }

    endSelection();
  }

  void jumpToNextSearchHit(string& searchTerm) {
    auto lineIt = lines.begin();
    advance(lineIt, currentRow());
    size_t from = currentCol() + 1;

    while (lineIt != lines.end()) {
      size_t pos = lineIt->find(searchTerm, from);

      if (pos == string::npos) {
        lineIt++;
        from = 0;
      } else {
        cursorTo(distance(lines.begin(), lineIt), pos);
        return;
      }
    }
  }

  void jumpToPrevSearchHit(string& searchTerm) {
    auto lineIt = lines.rbegin();
    advance(lineIt, lines.size() - currentRow() - 1);
    size_t from;

    if (currentCol() == 0) {
      lineIt++;
      from = lineIt->size();
    } else {
      from = currentCol() - 1;
    }

    while (lineIt != lines.rend()) {
      size_t pos = lineIt->rfind(searchTerm, from);

      if (pos == string::npos) {
        lineIt++;
        from = lineIt->size();
      } else {
        cursorTo(lines.size() - distance(lines.rbegin(), lineIt) - 1, pos);
        return;
      }
    }
  }

  // TODO: This looks as it should be a TextView function.
  void clipboardPaste(vector<string>& sharedClipboard) {
    history.newBlock(this);

    for (auto it = sharedClipboard.begin(); it != sharedClipboard.end(); it++) {
      if (it != sharedClipboard.begin()) {
        execCommand(Command::makeSplitLine(currentRow(), currentCol()));
        cursorTo(nextRow(), 0);
      }

      execCommand(Command::makeInsertSlice(currentRow(), currentCol(), *it));
      setCol(currentCol() + it->size());
    }

    saveXMemory();

    history.closeBlock(this);
  }

  /***
   * INSERTIONS
   */

  void insertCharacter(char c) {
    if (hasActiveSelection()) insertBackspace();

    if (currentRow() < (int)lines.size() && currentCol() <= currentLineSize()) {
      history.newBlock(this);

      execCommand(Command::makeInsertChar(currentRow(), currentCol(), c));

      cursorRight();

      history.closeBlock(this);
    }
  }

  void insertBackspace() {
    if (hasActiveSelection()) {
      history.newBlock(this);

      SelectionRange selection{selectionStart.value(), selectionEnd.value()};

      // Remove all lines
      vector<LineSelection> lineSelections = selection.lineSelections();

      int lineAdjustmentOffset{0};

      for (auto& lineSelection : lineSelections) {
        if (lineSelection.isFullLine()) {
          lineAdjustmentOffset++;
          execCommand(Command::makeDeleteLine(selection.startRow + 1, lines[selection.startRow + 1]));
        } else {
          int start = lineSelection.isLeftBounded() ? lineSelection.startCol : 0;
          int end = lineSelection.isRightBounded() ? lineSelection.endCol
                                                   : lines[lineSelection.lineNo - lineAdjustmentOffset].size();
          execCommand(
              Command::makeDeleteSlice(lineSelection.lineNo - lineAdjustmentOffset, start,
                                       lines[lineSelection.lineNo - lineAdjustmentOffset].substr(start, end - start)));
        }
      }

      if (selection.isMultiline()) {
        execCommand(Command::makeMergeLine(selection.startRow, lines[selection.startRow].size()));
      }

      // Put cursor to beginning
      cursorTo(selection.startRow, selection.startCol);
      endSelection();

      history.closeBlock(this);
    } else if (currentCol() <= currentLineSize() && currentCol() > 0) {
      history.newBlock(this);
      execCommand(Command::makeDeleteChar(currentRow(), currentCol() - 1, currentLine()[currentCol() - 1]));
      cursorLeft();
      history.closeBlock(this);
    } else if (currentCol() == 0 && currentRow() > 0) {
      history.newBlock(this);

      int oldLineLen = lines[currentRow() - 1].size();
      execCommand(Command::makeMergeLine(previousRow(), previousLine().size()));
      cursorTo(previousRow(), oldLineLen);

      history.closeBlock(this);
    }
  }

  void insertCtrlBackspace() {
    if (hasActiveSelection()) {
      insertBackspace();
    } else if (currentCol() > 0) {
      history.newBlock(this);

      int colStart = prevWordJumpLocation(currentLine(), currentCol()) + 1;
      if (currentCol() - colStart >= 0) {
        execCommand(
            Command::makeDeleteSlice(currentRow(), colStart, currentLine().substr(colStart, currentCol() - colStart)));

        setCol(colStart);
      }

      history.closeBlock(this);
    } else {
      cursorLeft();
    }

    saveXMemory();
  }

  void insertDelete() {
    if (hasActiveSelection()) {
      insertBackspace();
    } else if (currentCol() < currentLineSize()) {
      history.newBlock(this);
      execCommand(Command::makeDeleteChar(currentRow(), currentCol(), currentLine()[currentCol()]));
      history.closeBlock(this);
    } else if (currentRow() < (int)lines.size() - 1) {
      history.newBlock(this);
      execCommand(Command::makeMergeLine(currentRow(), currentCol()));
      history.closeBlock(this);
    }
  }

  void insertEnter() {
    if (hasActiveSelection()) insertBackspace();

    history.newBlock(this);

    execCommand(Command::makeSplitLine(currentRow(), currentCol()));

    int tabsLen = prefixTabOrSpaceLength(currentLine());
    if (tabsLen > 0) {
      string tabs(tabsLen, ' ');
      execCommand(Command::makeInsertSlice(nextRow(), 0, tabs));
    }

    setCol(tabsLen);
    saveXMemory();
    cursorDown();

    history.closeBlock(this);
  }

  void insertTab(int tabSize) {
    if (hasActiveSelection()) insertBackspace();

    if (currentCol() <= currentLineSize()) {
      int spacesToFill = tabSize - (currentCol() % tabSize);

      if (spacesToFill > 0) {
        string tabs(spacesToFill, ' ');

        history.newBlock(this);

        execCommand(Command::makeInsertSlice(currentRow(), currentCol(), tabs));
        setCol(currentCol() + spacesToFill);

        history.closeBlock(this);
      }
    }
  }

  void deleteLine() {
    history.newBlock(this);

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

    history.closeBlock(this);
  }

  void lineMoveForward() {
    history.newBlock(this);

    if (hasActiveSelection()) {
      SelectionRange selection{selectionStart.value(), selectionEnd.value()};

      if (selection.endRow >= (int)lines.size() - 1) {
        history.closeBlock(this);
        return;
      }

      int selectionLen = selection.endRow - selection.startRow + 1;

      lineMoveForward(selection.startRow, selectionLen);

      selectionStart = {selectionStart.value().row + 1, selectionStart.value().col};
      selectionEnd = {selectionEnd.value().row + 1, selectionEnd.value().col};
    } else {
      if (currentRow() >= (int)lines.size() - 1) {
        history.closeBlock(this);
        return;
      }

      lineMoveForward(currentRow(), 1);
    }

    cursorTo(nextRow(), currentCol());

    history.closeBlock(this);
  }

  void lineMoveForward(int lineNo, int lineCount) {
    for (int offs = lineCount - 1; offs >= 0; offs--) {
      execCommand(Command::makeSwapLine(lineNo + offs));
    }
  }

  void lineMoveBackward() {
    history.newBlock(this);

    if (hasActiveSelection()) {
      SelectionRange selection{selectionStart.value(), selectionEnd.value()};
      if (selection.startRow <= 0) {
        history.closeBlock(this);
        return;
      }

      int selectionLen = selection.endRow - selection.startRow + 1;

      lineMoveBackward(selection.startRow, selectionLen);

      selectionStart = {selectionStart.value().row - 1, selectionStart.value().col};
      selectionEnd = {selectionEnd.value().row - 1, selectionEnd.value().col};
    } else {
      if (currentRow() <= 0) {
        history.closeBlock(this);
        return;
      }

      lineMoveBackward(currentRow(), 1);
    }

    cursorTo(previousRow(), currentCol());

    history.closeBlock(this);
  }

  void lineMoveBackward(int lineNo, int lineCount) {
    for (int offs = 0; offs < lineCount; offs++) {
      execCommand(Command::makeSwapLine(lineNo + offs - 1));
    }
  }

  void lineIndentRight(int tabSize) {
    history.newBlock(this);

    if (hasActiveSelection()) {
      SelectionRange selection{selectionStart.value(), selectionEnd.value()};
      vector<LineSelection> lineSelections = selection.lineSelections();
      for (auto& sel : lineSelections) {
        __lineIndentRight(sel.lineNo, tabSize);
      }

      selectionStart = {selectionStart.value().row, selectionStart.value().col + tabSize};
      selectionEnd = {selectionEnd.value().row, selectionEnd.value().col + tabSize};
    } else {
      __lineIndentRight(currentRow(), tabSize);
    }

    setCol(currentCol() + tabSize);
    saveXMemory();

    history.closeBlock(this);
  }

  void __lineIndentRight(int lineNo, int tabSize) {
    string indent(tabSize, ' ');
    execCommand(Command::makeInsertSlice(lineNo, 0, indent));
  }

  void lineIndentLeft(int tabSize) {
    history.newBlock(this);

    if (hasActiveSelection()) {
      SelectionRange selection{selectionStart.value(), selectionEnd.value()};
      vector<LineSelection> lineSelections = selection.lineSelections();
      for (auto& sel : lineSelections) {
        int tabsRemoved = __lineIndentLeft(sel.lineNo, tabSize);

        if (sel.lineNo == currentRow()) setCol(currentCol() - tabsRemoved);

        if (sel.lineNo == selectionStart.value().row) {
          selectionStart = {selectionStart.value().row, selectionStart.value().col - tabsRemoved};
        }

        if (sel.lineNo == selectionEnd.value().row) {
          selectionEnd = {selectionEnd.value().row, selectionEnd.value().col - tabsRemoved};
        }
      }

    } else {
      int tabsRemoved = __lineIndentLeft(currentRow(), tabSize);
      setCol(currentCol() - tabsRemoved);
    }

    saveXMemory();

    history.closeBlock(this);
  }

  int __lineIndentLeft(int lineNo, int tabSize) {
    auto& line = lines[lineNo];
    auto it = find_if(line.begin(), line.end(), [](auto& c) { return !isspace(c); });
    int leadingTabLen = distance(line.begin(), it);
    int tabsRemoved = min(leadingTabLen, tabSize);

    if (tabsRemoved != 0) {
      string indent(tabsRemoved, ' ');
      execCommand(Command::makeDeleteSlice(lineNo, 0, indent));
    }

    return tabsRemoved;
  }

  // END INSERTIONS

  /***
   * FILE OPS
   */

  void reloadContent() {
    lines.clear();

    if (filePath.has_value()) {
      DLOG("Loading file: %s", filePath.value().c_str());

      ifstream f(filePath.value());

      if (!f.is_open()) {
        DLOG("File %s does not exists. Creating one.", filePath.value().c_str());
        f.open("a");
      } else {
        for (string line; getline(f, line);) {
          lines.emplace_back(line);
        }
      }

      f.close();

      isDirty = false;
    } else {
      DLOG("Cannot load file - config does not have any.");
    }

    reloadKeywordList();
    reloadSyntaxColoring();

    if (lines.empty()) lines.emplace_back("");

    cursor.set(0, 0);
  }

  void saveFile() {
    DLOG("Save file: %s", filePath.value().c_str());
    ofstream f(filePath.value(), ios::out | ios::trunc);

    for (int i = 0; i < (int)lines.size(); i++) {
      f << lines[i];

      if (i < (int)lines.size()) {
        f << endl;
      }
    }

    f.close();

    isDirty = false;

    fileWatcher.ignoreEventCycle();
  }

  void loadFile(string newFilePath) {
    filePath = optional<string>(newFilePath);
    fileWatcher.watch(newFilePath);

    reloadContent();
  }

  void closeFile() {
    filePath = nullopt;
    reloadContent();
  }

  optional<string> fileName() const {
    if (filePath.has_value()) {
      return filesystem::path(filePath.value()).filename();
    } else {
      return nullopt;
    }
  }

  // END FILE OPS

  /***
   * SELECTIONS
   */

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
    if (row == selectionStart.value().row && col < selectionStart.value().col) return false;
    if (row == selectionEnd.value().row && col > selectionEnd.value().col) return false;
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

  // END SELECTIONS

  void drawLine(string& out, int lineIdx, optional<string>& searchTerm) {
    string lineStr{};

    int lineNo = lineIdx + verticalScroll;

    if (size_t(lineNo) < lines.size()) {
      string& line = lines[lineNo];
      string decoratedLine = decorateLine(line, lineNo, searchTerm);

      char formatBuf[32];
      sprintf(formatBuf, "\x1b[33m%%%dd\x1b[0m ", leftMargin - 1);

      char marginBuf[32];
      sprintf(marginBuf, formatBuf, lineNo);

      lineStr.append(marginBuf);

      string finalLine{visibleSubstr(decoratedLine, horizontalScroll, textAreaCols())};
      if (horizontalScroll > 0 && visibleCharCount(finalLine) == 0) {
        lineStr.append("\x1b[90m<\x1b[0m");
      } else {
        lineStr.append(finalLine);
      }

      lineStr.append("\x1b[0m");
    } else {
      lineStr.push_back('~');
    }

    int paddingSize = cols - visibleCharCount(lineStr);
    if (paddingSize > 0) {
      string paddingSpaces(paddingSize, ' ');
      lineStr.append(paddingSpaces);
    } else if (paddingSize < 0) {
      DLOG("ERROR - line overflow. Cols: %d Line len: %d", cols, visibleCharCount(lineStr));
    }

    out.append(lineStr);
  }

  string decorateLine(string& line, int lineNo, optional<string>& searchTerm) {
    string out{};

    int offset{0};
    auto lineIt = line.begin();

    vector<SyntaxColorInfo> markers{};
    if (lineNo <= (int)syntaxColoring.size() - 1) {
      markers = syntaxColoring[lineNo];
    }

    auto selection = lineSelectionRange(lineNo);
    if (selection.has_value()) {
      markers.emplace_back(selection.value().first, BACKGROUND_REVERSE);
      markers.emplace_back(selection.value().second, RESET_REVERSE);
    }

    if (searchTerm.has_value()) {
      auto _searchTermMarkers = searchTermMarkers(line, searchTerm.value());
      copy(_searchTermMarkers.begin(), _searchTermMarkers.end(), back_inserter(markers));
    }

    sort(markers.begin(), markers.end(), [](SyntaxColorInfo& lhs, SyntaxColorInfo& rhs) { return lhs.pos < rhs.pos; });

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

  void updateDimensions(int newCols, int newRows) {
    cols = newCols;
    rows = newRows;

    leftMargin = max(1, (int)ceil(log10(lines.size()))) + 1;
  }
};
