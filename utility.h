#pragma once

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
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

struct SyntaxHighlightConfig {
  const char *numberColor{MAGENTA};
  const char *stringColor{LIGHTYELLOW};
  const char *parenColor{CYAN};
  const char *keywordColor{LIGHTCYAN};
  unordered_set<string> *keywords;

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
};

struct SyntaxColorInfo {
  int pos;
  const char *code;

  SyntaxColorInfo(int pos, const char *code) : pos(pos), code(code) {}
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
  // ShiftUp,
  // ShiftDown,
  // ShiftLeft,
  // ShiftRight,
  Home,
  End,
  PageUp,
  PageDown,
  Delete,
  AltLT,
  AltGT,
  AltShiftLT,
  AltShiftGT,
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

inline bool isParen(char c) {
  return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}';
}
inline bool isWordStart(char c) { return isalpha(c) || c == '_'; }
inline bool isWord(char c) { return isalnum(c) || c == '_'; }
inline bool isNumber(char c) { return isdigit(c); }

enum class TokenState {
  Unknown,
  Skip,
  Word,
  Number,
  QuotedString,
  Paren,
};

struct TokenAnalyzer {
  SyntaxHighlightConfig &config;

  TokenAnalyzer(SyntaxHighlightConfig &config) : config(config) {}

  vector<SyntaxColorInfo> colorizeTokens(string &input) {
    string current{};
    vector<SyntaxColorInfo> out{};

    for (auto it = input.begin(); it != input.end();) {
      current.clear();
      auto start = distance(input.begin(), it);

      if (isWordStart(*it)) {
        while (it != input.end() && isWord(*it)) current.push_back(*it++);
        registerColorMarks(current, start, TokenState::Word, &out);
      } else if (isNumber(*it)) {
        while (it != input.end() && isNumber(*it)) current.push_back(*it++);
        registerColorMarks(current, start, TokenState::Number, &out);
      } else if (*it == '\'') {
        current.push_back(*it++);

        while (it != input.end() && *it != '\'') current.push_back(*it++);
        if (it != input.end()) current.push_back(*it++);

        registerColorMarks(current, start, TokenState::QuotedString, &out);
      } else if (*it == '"') {
        current.push_back(*it++);

        while (it != input.end() && *it != '"') current.push_back(*it++);
        if (it != input.end()) current.push_back(*it++);

        registerColorMarks(current, start, TokenState::QuotedString, &out);
      } else if (isParen(*it)) {
        while (it != input.end() && isParen(*it)) current.push_back(*it++);
        registerColorMarks(current, start, TokenState::Paren, &out);
      } else {
        it++;
      }
    }

    return out;
  }

 private:
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
      default:
        return nullptr;
    }
  }

  void registerColorMarks(string &word, int start, TokenState state,
                          vector<SyntaxColorInfo> *out) {
    const char *colorResult = analyzeToken(state, word);
    if (colorResult) {
      out->emplace_back(start, colorResult);
      out->emplace_back(start + word.size(), DEFAULT_FOREGROUND);
    }
  }
};

/**
 * @brief Returns the location in a character seqence of the next different
 * type character - alphabetic vs not-alphabetic. Eg if it's on a word, it shows
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
