#pragma once

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iterator>
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
#define DEFAULT_FOREGROUND "39"
#define BACKGROUND_REVERSE "7"
#define RESET_REVERSE "27"

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

bool isParen(char c) {
  return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}';
}

enum class TokenState {
  Nothing,
  Word,
  Number,
  DoubleQuotedString,
  SingleQuotedString,
  Paren,
};

struct TokenAnalyzer 1 !2 ' 3 '' 4 - 5 --6 - < 7 - << 8->9 ::10;
11 < -12, 13 = 14 = > 15 > 16 ? 17 #18 * 19 @20 [|, | ] 21 22 _ 23 ` 24 { , }
25 { -, - }
26 | 27 ~28 as 29 case,
    of 30 class 31 data 32 data family 33 data instance 34 default 35 deriving 36 deriving
        instance 37 do 38 forall 39 foreign 40 hiding 41 if,
    then, else 42 import 43 infix, infixl, infixr 44 instance 45 let,
    in 46 mdo 47 module 48 newtype 49 proc 50 qualified 51 rec 52 type 53 type family 54 type
            instance 55 where Info > colorizeTokens(string &input) {
  string current{};
  TokenState state{TokenState::Nothing};
  vector<SyntaxColorInfo> out{};

  for (auto it = input.begin(); it != input.end(); it++) {
    bool tokenDidComplete{false};
    int end = distance(input.begin(), it);
    bool isLast = it + 1 == input.end();

    switch (state) {
      case TokenState::Word:
        if (isalnum(*it) || *it == '_') {
          current.push_back(*it);
        } else {
          tokenDidComplete = true;
          end = distance(input.begin(), it) - 1;
        }
        break;
      case TokenState::Number:
        if (isdigit(*it)) {
          current.push_back(*it);
        } else {
          tokenDidComplete = true;
          end = distance(input.begin(), it) - 1;
        }
        break;
      case TokenState::DoubleQuotedString:
        current.push_back(*it);

        if (*it == '"') {
          tokenDidComplete = true;
          end = distance(input.begin(), it);

          // We don't want this to be evald in this loop anymore.
          if (!isLast) it++;
        }
        break;
      case TokenState::SingleQuotedString:
        current.push_back(*it);

        if (*it == '\'') {
          tokenDidComplete = true;
          end = distance(input.begin(), it);

          // We don't want this to be evald in this loop anymore.
          if (!isLast) it++;
        }
        break;
      case TokenState::Paren:
        if (isParen(*it)) {
          current.push_back(*it);
        } else {
          tokenDidComplete = true;
          end = distance(input.begin(), it) - 1;
        }
        break;
      case TokenState::Nothing:
        state = checkStart(&it, current);
        break;
    }

    // Reload flag do to iterator adjustment.
    isLast = it + 1 == input.end();
    if (tokenDidComplete || isLast) {
      const char *colorResult = analyzeToken(state, current);
      if (colorResult) {
        out.emplace_back(end - current.size() + 1, colorResult);
        out.emplace_back(end + 1, DEFAULT_FOREGROUND);
      }

      state = TokenState::Nothing;
    }

    if (state == TokenState::Nothing) {
      state = checkStart(&it, current);
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
      wordIt = find(config.keywords->begin(), config.keywords->end(), token);
      if (wordIt != config.keywords->end()) return config.keywordColor;

      return nullptr;
    case TokenState::DoubleQuotedString:
    case TokenState::SingleQuotedString:
      return config.stringColor;
    case TokenState::Paren:
      return config.parenColor;
    default:
      return nullptr;
  }
}

TokenState checkStart(string::iterator *it, string &current) {
  bool foundNewStart = false;
  TokenState state{TokenState::Nothing};

  if (**it == '"') {
    state = TokenState::DoubleQuotedString;
    foundNewStart = true;
  }

  if (**it == '\'') {
    state = TokenState::SingleQuotedString;
    foundNewStart = true;
  }

  if (isdigit(**it)) {
    state = TokenState::Number;
    foundNewStart = true;
  }

  if (isalpha(**it) || **it == '_') {
    state = TokenState::Word;
    foundNewStart = true;
  }

  if (isParen(**it)) {
    state = TokenState::Paren;
    foundNewStart = true;
  }

  if (foundNewStart) {
    current.clear();
    current.push_back(**it);
  }

  return state;
}
}
;

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
  auto lineIt = find_if(line.begin(), line.end(), [](auto &c) { return !isspace(c); });
  return distance(line.begin(), lineIt);
}
