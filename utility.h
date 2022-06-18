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

using namespace std;

struct SyntaxHighlightConfig {
  const char *numberColor{"92"};
  const char *stringColor{"93"};
  const char *parenColor{"91"};
  const char *lvl1Color{"96"};
  const char *lvl2Color{"35"};

  unordered_set<const char *> lvl1Words{
      "struct", "class",     "namespace", "include", "return",  "default",
      "public", "protected", "private",   "using",   "typedef", "enum"};

  unordered_set<const char *> lvl2Words{"if",     "for",  "while",
                                        "switch", "case", "const"};
};

struct SyntaxColorInfo {
  int start;
  int end;
  const char *colorCode;

  SyntaxColorInfo(int start, int end, const char *colorCode)
      : start(start), end(end), colorCode(colorCode) {}
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
  Home,
  End,
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

struct TokenAnalyzer {
  SyntaxHighlightConfig &config;

  TokenAnalyzer(SyntaxHighlightConfig &config) : config(config) {}

  vector<SyntaxColorInfo> colorizeTokens(string &input) {
    string current{};
    TokenState state{TokenState::Nothing};
    vector<SyntaxColorInfo> out{};

    for (auto it = input.begin(); it != input.end(); it++) {
      bool tokenDidComplete{false};
      int end = distance(input.begin(), it);
      bool isLast = it + 1 == input.end();

      switch (state) {
        case TokenState::Word:
          if (isalpha(*it)) {
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
          out.emplace_back(end - current.size() + 1, end, colorResult);
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
    unordered_set<const char *>::iterator wordIt;

    switch (state) {
      case TokenState::Number:
        return config.numberColor;
      case TokenState::Word:
        wordIt = find(config.lvl1Words.begin(), config.lvl1Words.end(), token);
        if (wordIt != config.lvl1Words.end()) {
          return config.lvl1Color;
        }
        wordIt = find(config.lvl2Words.begin(), config.lvl2Words.end(), token);
        if (wordIt != config.lvl2Words.end()) {
          return config.lvl2Color;
        }
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

    if (isalpha(**it)) {
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
