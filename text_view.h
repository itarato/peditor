#pragma once

#include <algorithm>
#include <deque>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "command.h"
#include "debug.h"
#include "utility.h"

using namespace std;

#define UNDO_LIMIT 64

static unordered_map<const char*, const char*> fileTypeAssociationMap{
    {".c++", "c++"}, {".cpp", "c++"}, {".hpp", "c++"},   {".h", "c++"},
    {".c", "c++"},   {".rb", "ruby"}, {".hs", "haskell"}};

struct TextView {
  Point cursor{0, 0};

  int verticalScroll{0};
  int horizontalScroll{0};
  int xMemory{0};

  optional<string> fileName{nullopt};
  unordered_set<string> keywords{};

  vector<string> lines{};

  optional<SelectionEdge> selectionStart{nullopt};
  optional<SelectionEdge> selectionEnd{nullopt};

  deque<Command> undos{};
  deque<Command> redos{};

  int cols;
  int rows;

  bool isDirty{false};

  void saveFile() {
    DLOG("Save file: %s", fileName.value().c_str());
    ofstream f(fileName.value(), ios::out | ios::trunc);

    for (int i = 0; i < (int)lines.size(); i++) {
      f << lines[i];

      if (i < (int)lines.size()) {
        f << endl;
      }
    }

    f.close();

    isDirty = false;
  }

  void setFileName(string newFileName) {
    fileName = optional<string>(newFileName);
  }

  void reloadKeywordList() {
    keywords.clear();

    if (!fileName.has_value()) {
      DLOG("No file, cannot load keyword list.");
      return;
    }

    auto ext = filesystem::path(fileName.value()).extension();
    auto it =
        find_if(fileTypeAssociationMap.begin(), fileTypeAssociationMap.end(),
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
  }

  bool onLineRow() {
    return currentRow() >= 0 && currentRow() < (int)lines.size();
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
      if (onLineRow()) cursor.x = 0;
    }

    fixCursorPos();
    saveXMemory();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();
  }

  void cursorPageDown() {
    cursor.y += rows;
    restoreXMemory();

    if (hasActiveSelection()) endSelectionUpdatePositionToCurrent();

    fixCursorPos();
  }

  void cursorPageUp() {
    cursor.y -= rows;
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
    } else if (cursor.y >= rows) {
      verticalScroll = currentRow() - rows + 1;
      cursor.y = rows - 1;
    }

    // Fix horizontal scroll.
    if (cursor.x < 0) {
      horizontalScroll = currentCol();
      cursor.x = 0;
    } else if (cursor.x >= cols) {
      horizontalScroll = currentCol() - cols + 1;
      cursor.x = cols - 1;
    }
  }

  inline int currentRow() { return verticalScroll + cursor.y; }
  inline int previousRow() { return verticalScroll + cursor.y - 1; }
  inline int nextRow() { return verticalScroll + cursor.y + 1; }

  // Text area related -> TODO: rename
  inline int currentCol() { return horizontalScroll + cursor.x; }

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
    } else if (horizontalScroll + cols < newCol) {
      horizontalScroll = newCol - cols + 1;
    }

    cursor.x = newCol - horizontalScroll;
  }

  inline string& currentLine() { return lines[currentRow()]; }
  inline string& previousLine() { return lines[previousRow()]; }
  inline string& nextLine() { return lines[nextRow()]; }
  inline int currentLineSize() { return currentLine().size(); }

  inline void restoreXMemory() { cursor.x = xMemory; }
  inline void saveXMemory() { xMemory = cursor.x; }

  inline bool isEndOfCurrentLine() { return currentCol() >= currentLineSize(); }
  inline bool isBeginningOfCurrentLine() { return currentCol() <= 0; }

  void execCommand(Command&& cmd) {
    undos.push_back(cmd);
    DLOG("EXEC cmd: %d", cmd.type);

    TextManipulator::execute(&cmd, &lines);
    isDirty = true;

    redos.clear();
    while (undos.size() > UNDO_LIMIT) undos.pop_front();
  }

  /***
   * INSERTIONS
   */

  void insertCharacter(char c) {
    if (hasActiveSelection()) insertBackspace();

    if (currentRow() < (int)lines.size() && currentCol() <= currentLineSize()) {
      execCommand(Command::makeInsertChar(currentRow(), currentCol(), c));

      cursorRight();
    }
  }

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

  void insertTab(int tabSize) {
    if (currentCol() <= currentLineSize()) {
      int spacesToFill = tabSize - (currentCol() % tabSize);

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

  void __lineIndentRight(int lineNo, int tabSize) {
    string indent(tabSize, ' ');
    execCommand(Command::makeInsertSlice(lineNo, 0, indent));
  }

  void lineMoveForward() {
    if (hasActiveSelection()) {
      if (selectionEnd.value().row >= (int)lines.size() - 1) return;

      int selectionLen =
          selectionEnd.value().row - selectionStart.value().row + 1;

      lineMoveForward(selectionStart.value().row, selectionLen);

      selectionStart = {selectionStart.value().row + 1,
                        selectionStart.value().col};
      selectionEnd = {selectionEnd.value().row + 1, selectionEnd.value().col};
    } else {
      if (currentRow() >= (int)lines.size() - 1) return;

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
      if (selectionStart.value().row <= 0) return;

      int selectionLen =
          selectionEnd.value().row - selectionStart.value().row + 1;

      lineMoveBackward(selectionStart.value().row, selectionLen);

      selectionStart = {selectionStart.value().row - 1,
                        selectionStart.value().col};
      selectionEnd = {selectionEnd.value().row - 1, selectionEnd.value().col};
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

  void lineIndentRight(int tabSize) {
    if (hasActiveSelection()) {
      SelectionRange selection{selectionStart.value(), selectionEnd.value()};
      vector<LineSelection> lineSelections = selection.lineSelections();
      for (auto& sel : lineSelections) {
        __lineIndentRight(sel.lineNo, tabSize);
      }

      selectionStart = {selectionStart.value().row,
                        selectionStart.value().col + tabSize};
      selectionEnd = {selectionEnd.value().row,
                      selectionEnd.value().col + tabSize};
    } else {
      __lineIndentRight(currentRow(), tabSize);
    }

    setCol(currentCol() + tabSize);
    saveXMemory();
  }

  void lineIndentLeft(int tabSize) {
    if (hasActiveSelection()) {
      SelectionRange selection{selectionStart.value(), selectionEnd.value()};
      vector<LineSelection> lineSelections = selection.lineSelections();
      for (auto& sel : lineSelections) {
        int tabsRemoved = __lineIndentLeft(sel.lineNo, tabSize);

        if (sel.lineNo == currentRow()) setCol(currentCol() - tabsRemoved);

        if (sel.lineNo == selectionStart.value().row) {
          selectionStart = {selectionStart.value().row,
                            selectionStart.value().col - tabsRemoved};
        }

        if (sel.lineNo == selectionEnd.value().row) {
          selectionEnd = {selectionEnd.value().row,
                          selectionEnd.value().col - tabsRemoved};
        }
      }

    } else {
      int tabsRemoved = __lineIndentLeft(currentRow(), tabSize);
      setCol(currentCol() - tabsRemoved);
    }

    saveXMemory();
  }

  int __lineIndentLeft(int lineNo, int tabSize) {
    auto& line = lines[lineNo];
    auto it =
        find_if(line.begin(), line.end(), [](auto& c) { return !isspace(c); });
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

  // END SELECTIONS
};
