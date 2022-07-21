#pragma once

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "debug.h"

#define TYPED_CHAR_SIMPLE 0
#define TYPED_CHAR_ESCAPE 1

#define BLACK "30"
#define RED "31"
#define GREEN "32"
#define YELLOW "33"
#define BLUE "34"
#define MAGENTA "35"
#define CYAN "36"
#define LIGHTGRAY "37"
#define DARKGRAY "90"
#define LIGHTRED "91"
#define LIGHTGREEN "92"
#define LIGHTYELLOW "93"
#define LIGHTBLUE "94"
#define LIGHTMAGENTA "95"
#define LIGHTCYAN "96"
#define WHITE "97"
#define BLUE_BACKGROUND "44"
#define DEFAULT_FOREGROUND "39"
#define DEFAULT_BACKGROUND "49"
#define BACKGROUND_REVERSE "7"
#define RESET_REVERSE "27"
#define UNDERLINE "4"
#define RESET_UNDERLINE "24"

using namespace std;

// FIXME: Unused.
struct CodeComments {
  vector<string> oneLiners{"//"};
  vector<pair<string, string>> bounded{{"/*", "*/"}};
};

struct SyntaxHighlightConfig {
  const char *numberColor{MAGENTA};
  const char *stringColor{LIGHTYELLOW};
  const char *parenColor{CYAN};
  const char *keywordColor{LIGHTCYAN};
  const char *commentColor{DARKGRAY};
  unordered_set<string> *keywords;
  CodeComments comments{};

  SyntaxHighlightConfig(unordered_set<string> *keywords) : keywords(keywords) {}
};

struct SelectionEdge {
  int row;
  int col;

  SelectionEdge(int row, int col) : row(row), col(col) {}
};

struct LineSelection {
  int lineNo;
  int startCol{-1};
  int endCol{-1};

  LineSelection(int lineNo) : lineNo(lineNo) {}
  LineSelection(int lineNo, int startCol, int endCol)
      : lineNo(lineNo), startCol(startCol), endCol(endCol) {}

  bool isFullLine() { return startCol == -1 && endCol == -1; }
  bool isLeftBounded() { return startCol >= 0; }
  bool isRightBounded() { return endCol >= 0; }
};

/**
 * End column is not included.
 */
struct SelectionRange {
  int startRow;
  int startCol;
  int endRow;
  int endCol;

  SelectionRange(SelectionEdge s0, SelectionEdge s1) {
    if (SelectionRange::isSelectionRightFacing(s0, s1)) {
      startCol = s0.col;
      startRow = s0.row;
      endCol = s1.col;
      endRow = s1.row;
    } else {
      endCol = s0.col;
      endRow = s0.row;
      startCol = s1.col;
      startRow = s1.row;
    }
  }

  static bool isSelectionRightFacing(SelectionEdge s0, SelectionEdge s1) {
    if (s1.row < s0.row) return false;
    if (s1.row == s0.row && s1.col < s0.col) return false;
    return true;
  }

  bool isMultiline() { return startRow < endRow; }

  vector<LineSelection> lineSelections() {
    vector<LineSelection> out{};

    if (startRow == endRow) {
      out.emplace_back(startRow, startCol, endCol);
    } else {
      out.emplace_back(startRow, startCol, -1);
      for (int i = startRow + 1; i < endRow; i++) out.emplace_back(i, -1, -1);
      out.emplace_back(endRow, -1, endCol);
    }

    return out;
  }
};

struct Point {
  int x{-1};
  int y{-1};

  Point() {}
  Point(int x, int y) : x(x), y(y) {}

  void set(int newX, int newY) {
    x = newX;
    y = newY;
  }

  void set(Point newPoint) {
    x = newPoint.x;
    y = newPoint.y;
  }

  inline Point dx(int delta) { return Point{x + delta, y}; }
  inline Point dy(int delta) { return Point{x, y + delta}; }
};

struct ITextViewState {
  virtual Point getCursor() = 0;
  virtual optional<SelectionEdge> getSelectionStart() = 0;
  virtual optional<SelectionEdge> getSelectionEnd() = 0;
};

struct SyntaxColorInfo {
  int pos;
  const char *code;

  SyntaxColorInfo(int pos, const char *code) : pos(pos), code(code) {}

  // TODO: Make this more stable.
  inline bool isClosingTag() const {
    return strcmp(code, DEFAULT_FOREGROUND) == 0;
  }
};

enum class TextEditorAction {
  Type,
  Quit,
  SaveFile,
  SaveFileAs,
  OpenFile,
  MultiPurposeCommand,
  DeleteLine,
  Undo,
  Redo,
  Copy,
  Paste,
  SelectionToggle,
  JumpNextSearchHit,
  JumpPrevSearchHit,
  Backspace,
  WordBackspace,
  Enter,
  Tab,

  CursorDown,
  CursorUp,
  CursorLeft,
  CursorRight,
  CursorHome,
  CursorEnd,
  CursorPageUp,
  CursorPageDown,
  CursorWordJumpLeft,
  CursosWordJumpRight,
  SplitUnitToPrev,
  SplitUnitToNext,
  ScrollUp,
  ScrollDown,
  InsertDelete,
  LineIndentLeft,
  LineIndentRight,
  LineMoveBackward,
  LineMoveForward,
  NewTextView,
  ChangeActiveView0,
  ChangeActiveView1,
  ChangeActiveView2,
  ChangeActiveView3,
  ChangeActiveView4,
  ChangeActiveView5,
  ChangeActiveView6,
  ChangeActiveView7,
  ChangeActiveView8,
  ChangeActiveView9,
  NewSplitUnit,
  CloseTextView,
};

enum class InputStroke {
  Generic,

  CtrlQ,
  CtrlS,
  CtrlW,
  CtrlO,
  CtrlP,
  CtrlD,
  CtrlZ,
  CtrlR,
  CtrlC,
  CtrlV,
  CtrlX,
  CtrlN,
  CtrlB,

  Backspace,
  CtrlBackspace,
  Enter,
  Tab,

  Down,
  Up,
  Left,
  Right,
  Home,
  End,
  CtrlUp,
  CtrlDown,
  CtrlLeft,
  CtrlRight,
  CtrlAltLeft,
  CtrlAltRight,
  PageUp,
  PageDown,
  Delete,
  AltLT,
  AltGT,
  AltMinus,
  AltEqual,
  AltN,
  Alt0,
  Alt1,
  Alt2,
  Alt3,
  Alt4,
  Alt5,
  Alt6,
  Alt7,
  Alt8,
  Alt9,
  AltS,
  AltK,
};

enum class EscapeChar {
  Up,
  Down,
  Left,
  Right,
  CtrlUp,
  CtrlDown,
  CtrlLeft,
  CtrlRight,
  CtrlAltLeft,
  CtrlAltRight,
  Home,
  End,
  PageUp,
  PageDown,
  Delete,
  AltLT,
  AltGT,
  AltN,
  Alt0,
  Alt1,
  Alt2,
  Alt3,
  Alt4,
  Alt5,
  Alt6,
  Alt7,
  Alt8,
  Alt9,
  AltMinus,
  AltEqual,
  AltS,
  AltK,
};

struct TypedChar {
  union {
    char ch;
    EscapeChar escapeCh;
  } c;
  int type;

  TypedChar() {
    c.ch = '\0';
    type = TYPED_CHAR_SIMPLE;
  }

  TypedChar(char ch) {
    c.ch = ch;
    type = TYPED_CHAR_SIMPLE;
  }

  TypedChar(EscapeChar escapeChar) {
    c.escapeCh = escapeChar;
    type = TYPED_CHAR_ESCAPE;
  }

  bool is_failure() { return c.ch == '\0' && type == TYPED_CHAR_SIMPLE; }

  bool is_simple() { return type == TYPED_CHAR_SIMPLE; }

  bool is_escape() { return type == TYPED_CHAR_ESCAPE; }

  char simple() { return c.ch; }

  EscapeChar escape() { return c.escapeCh; }
};

#define TYPED_CHAR_FAILURE (TypedChar{'\0'})

void reportAndExit(const char *s) {
  perror(s);
  exit(EXIT_FAILURE);
}

enum class MultiLineCharIteratorState {
  OnCharacter,
  OnNewLine,
  OnEnd,
};

struct MultiLineCharIterator {
  vector<string> &lines;
  const char end{'\0'};
  const char newline{'\n'};
  Point idx{-1, -1};

  MultiLineCharIteratorState state{MultiLineCharIteratorState::OnNewLine};

  MultiLineCharIterator(vector<string> &lines) : lines(lines) { next(); }

  bool next() {
    if (state == MultiLineCharIteratorState::OnEnd) {
      return false;
    } else if (state == MultiLineCharIteratorState::OnNewLine) {
      idx.y++;
      idx.x = 0;
    } else if (state == MultiLineCharIteratorState::OnCharacter) {
      idx.x++;
    } else {
      reportAndExit("Unhandled MultiLineCharIteratorState");
    }

    if (idx.y >= (int)lines.size()) {
      state = MultiLineCharIteratorState::OnEnd;
      return true;
    }

    if (idx.x >= (int)lines[idx.y].size()) {
      state = MultiLineCharIteratorState::OnNewLine;
      return true;
    }

    state = MultiLineCharIteratorState::OnCharacter;

    return true;
  }

  const char current() const {
    switch (state) {
      case MultiLineCharIteratorState::OnNewLine:
        return newline;
      case MultiLineCharIteratorState::OnEnd:
        return end;
      case MultiLineCharIteratorState::OnCharacter:
        return lines[idx.y][idx.x];
    }

    return end;
  }

  inline string peek(int n) const { return lines[idx.y].substr(idx.x, n); }

  bool isPeekMatch(string &s) const {
    if (idx.x + s.size() > lines[idx.y].size()) return false;

    for (int i = 0; i < (int)s.size() && (i + idx.x) < (int)lines[idx.y].size();
         i++) {
      if (s[i] != lines[idx.y][idx.x + i]) return false;
    }

    return true;
  }

  const bool isEnded() const {
    return state == MultiLineCharIteratorState::OnEnd;
  }

  const bool isRealChar() const {
    return state == MultiLineCharIteratorState::OnCharacter;
  }

  const bool isNewLine() const {
    return state == MultiLineCharIteratorState::OnNewLine;
  }
};

inline bool isParen(char c) {
  return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}';
}
inline bool isWordStart(char c) { return isalpha(c) || c == '_'; }
inline bool isWord(char c) { return isalnum(c) || c == '_'; }
inline bool isNumber(char c) { return isdigit(c); }
inline bool isQuote(char c) { return c == '"' || c == '\''; }

enum class TokenState {
  Unknown,
  Skip,
  Word,
  Number,
  QuotedString,
  Paren,
  Comment,
};

struct TokenAnalyzer {
  SyntaxHighlightConfig config;

  TokenAnalyzer(SyntaxHighlightConfig config) : config(config) {}

  vector<vector<SyntaxColorInfo>> colorizeTokens(vector<string> &inputLines) {
    string current{};
    vector<vector<SyntaxColorInfo>> out{};

    for (int i = 0; i < (int)inputLines.size(); i++) out.emplace_back();

    for (MultiLineCharIterator it{inputLines}; !it.isEnded();) {
      current.clear();
      Point start = it.idx;
      Point end = it.idx;

      if (isWordStart(it.current())) {
        while (!it.isEnded() && isWord(it.current())) consume(current, it, end);
        registerColorMarks(current, start, it.idx.dx(-1), TokenState::Word,
                           out);
      } else if (isNumber(it.current())) {
        while (!it.isEnded() && isNumber(it.current()))
          consume(current, it, end);
        registerColorMarks(current, start, it.idx.dx(-1), TokenState::Number,
                           out);
      } else if (isQuote(it.current())) {
        char quoteType{it.current()};

        // First quote.
        consume(current, it, end);

        // Collect until closing quote or end.
        while (!it.isEnded() && it.current() != quoteType)
          consume(current, it, end);

        // Add closing quote (in case it wasn't overrunning the line).
        consume(current, it, end);

        registerColorMarks(current, start, end, TokenState::QuotedString, out);
      } else if (isParen(it.current())) {
        while (!it.isEnded() && isParen(it.current()))
          consume(current, it, end);
        registerColorMarks(current, start, it.idx.dx(-1), TokenState::Paren,
                           out);
      } else {
        bool hasMatch{false};

        for (auto &oneLinerComment : config.comments.oneLiners) {
          if (it.isPeekMatch(oneLinerComment)) {
            hasMatch = true;

            while (!it.isEnded() && !it.isNewLine()) consume(it, end);

            registerColorMarks(current, start, end, TokenState::Comment, out);
            break;
          }
        }
        if (hasMatch) continue;

        for (auto &bounded : config.comments.bounded) {
          if (it.isPeekMatch(bounded.first)) {
            hasMatch = true;

            // Consume first half of comment boundary.
            for (int i = 0; i < (int)bounded.first.size(); i++)
              consume(it, end);

            while (!it.isEnded() && !it.isPeekMatch(bounded.second))
              consume(it, end);

            if (it.isPeekMatch(bounded.second)) {
              // Consume first half of comment boundary.
              for (int i = 0; i < (int)bounded.second.size(); i++)
                consume(it, end);
            }

            registerColorMarks(current, start, end, TokenState::Comment, out);
            break;
          }
        }
        if (hasMatch) continue;

        it.next();
      }
    }

    return out;
  }

 private:
  void consume(string &current, MultiLineCharIterator &it, Point &end) {
    if (it.isEnded()) return;

    if (it.isRealChar()) {
      current.push_back(it.current());
      end.set(it.idx);
    }

    it.next();
  }

  void consume(MultiLineCharIterator &it, Point &end) {
    if (it.isEnded()) return;
    if (it.isRealChar()) end.set(it.idx);
    it.next();
  }

  const char *analyzeToken(TokenState state, string &token) {
    unordered_set<string>::iterator wordIt;

    switch (state) {
      case TokenState::Number:
        return config.numberColor;
      case TokenState::Word:
        if (!config.keywords) return nullptr;

        wordIt = find(config.keywords->begin(), config.keywords->end(), token);
        if (wordIt != config.keywords->end()) return config.keywordColor;

        return nullptr;
      case TokenState::QuotedString:
        return config.stringColor;
      case TokenState::Paren:
        return config.parenColor;
      case TokenState::Comment:
        return config.commentColor;
      default:
        return nullptr;
    }
  }

  void registerColorMarks(string &word, Point start, Point end,
                          TokenState state,
                          vector<vector<SyntaxColorInfo>> &out) {
    const char *colorResult = analyzeToken(state, word);
    if (colorResult) {
      out[start.y].emplace_back(start.x, colorResult);

      for (int i = start.y + 1; i <= end.y; i++) {
        out[i].emplace_back(0, colorResult);
      }

      out[end.y].emplace_back(end.x + 1, DEFAULT_FOREGROUND);
    }
  }
};

/**
 * @brief Returns the location in a character seqence of the next different
 * type character - alphabetic vs not-alphabetic. Eg if it's on a word, it
shows
 * the character after that word.
 * At the end of the string it returns the position after the last character.
 *
 * @param line
 * @param currentPos
 * @return int
 */
int nextWordJumpLocation(string &line, int currentPos) {
  if (currentPos >= (int)line.size()) return line.size();
  if (currentPos < 0) return 0;

  auto it = line.begin();
  advance(it, currentPos + 1);

  bool isOnAlpha = isalpha(*it) > 0;

  for (; it != line.end(); it++) {
    if (isOnAlpha ^ (isalpha(*it) > 0)) {
      return distance(line.begin(), it);
    }
  }

  return line.size();
}

/**
 * @brief Returns the location in a character seqence of the previous different
 * type character - alphabetic vs not-alphabetic. Eg if it's on a word, it shows
 * the character before that word.
 * At the beginning of the string it returns -1.
 *
 * @param line
 * @param currentPos
 * @return int
 */
int prevWordJumpLocation(string &line, int currentPos) {
  if (currentPos > (int)line.size()) return line.size();
  if (currentPos < 0) return -1;

  auto it = line.rbegin();
  advance(it, line.size() - currentPos);

  bool isOnAlpha = isalpha(*it) > 0;

  for (; it != line.rend(); it++) {
    if (isOnAlpha ^ (isalpha(*it) > 0)) {
      return line.size() - distance(line.rbegin(), it) - 1;
    }
  }

  return -1;
}

int prefixTabOrSpaceLength(string &line) {
  auto lineIt =
      find_if(line.begin(), line.end(), [](auto &c) { return !isspace(c); });
  return distance(line.begin(), lineIt);
}

/**
 * Count all non escaping characters.
 */
int visibleCharCount(string &s) {
  int n{0};
  bool isEscape{false};
  for (auto &c : s) {
    if (isEscape) {
      if (c == 'm') isEscape = false;
    } else {
      if (c == '\x1b') {
        isEscape = true;
      } else {
        n++;
      }
    }
  }
  return n;
}

int visibleStrRightCut(string &s, int len) {
  int n{0};      // Visible idx.
  int realN{0};  // Real idx.
  bool isEscape{false};
  for (auto &c : s) {
    realN++;
    if (isEscape) {
      if (c == 'm') isEscape = false;
    } else {
      if (c == '\x1b') {
        isEscape = true;
      } else {
        n++;

        if (n > len) return realN - 1;
      }
    }
  }

  return realN;
}

/**
 * @return pair<int, int> = <start, end>
 *  start = -1 -> not visible
 *  end   = -1 -> end is fully visible
 */
pair<int, int> visibleStrSlice(string &s, int offset, int len) {
  if (len <= 0) reportAndExit("Invalid argument: len");

  int start{-1};
  int end{(int)s.size() - 1};

  int n{0};      // Visible idx.
  int realN{0};  // Real idx.
  bool isEscape{false};

  for (auto &c : s) {
    if (start == -1 && n + 1 > offset) {
      start = realN;
    }

    realN++;
    if (isEscape) {
      if (c == 'm') isEscape = false;
    } else {
      if (c == '\x1b') {
        isEscape = true;
      } else {
        n++;

        if (n > offset + len) {
          end = realN - 2;
          break;
        }
      }
    }
  }

  return pair<int, int>{start, end};
}

vector<SyntaxColorInfo> searchTermMarkers(string &line, string &searchTerm) {
  vector<SyntaxColorInfo> out{};

  size_t from{0};

  for (;;) {
    size_t i = line.find(searchTerm, from);
    if (i == string::npos) break;

    out.emplace_back(i, BLUE_BACKGROUND);
    out.emplace_back(i + (int)searchTerm.size(), DEFAULT_BACKGROUND);

    from = i + searchTerm.size();
  }

  return out;
}

vector<string> directoryFiles(string path) {
  vector<string> out{};

  for (auto &p : filesystem::directory_iterator(path)) {
    if (p.path().filename().c_str()[0] == '.') continue;

    if (p.is_directory()) {
      auto subDirFiles = directoryFiles(p.path());
      copy(subDirFiles.begin(), subDirFiles.end(), back_insert_iterator(out));
    } else if (p.is_regular_file()) {
      out.push_back(p.path().c_str());
    }
  }

  return out;
}

vector<string> directoryFiles() {
  // This is massively ineffective:
  // - needs to be cached
  // - needs to be reloaded by watching directory changes
  return directoryFiles("./");
}

bool poormansFuzzyMatch(string term, string word) {
  if (term.empty()) return false;

  int termIdx{(int)term.size() - 1};

  for (auto wordIt = word.rbegin(); wordIt != word.rend(); wordIt++) {
    if (term[termIdx] == *wordIt) {
      termIdx--;

      if (termIdx < 0) return true;
    }
  }

  return false;
}

vector<string> poormansFuzzySearch(string term, vector<string> options,
                                   int maxResult) {
  vector<string> out{};

  for (auto &option : options) {
    if (poormansFuzzyMatch(term, option)) {
      out.push_back(option);
      if ((int)out.size() >= maxResult) break;
    }
  }

  return out;
}

// FIXME: This is a bit tied to prompt (due to coloring scheme).
string highlightPoormanFuzzyMatch(string &term, string &word) {
  string out{};

  int termIdx{(int)term.size() - 1};

  for (auto wordIt = word.rbegin(); wordIt != word.rend(); wordIt++) {
    if (termIdx >= 0 && term[termIdx] == *wordIt) {
      termIdx--;

      out.insert(0, "\x1b[27m\x1b[39m");
      out.insert(0, 1, *wordIt);
      out.insert(0, "\x1b[7m\x1b[93m");
    } else {
      out.insert(0, 1, *wordIt);
    }
  }

  return out;
}
