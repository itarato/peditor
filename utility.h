#pragma once

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#define TYPED_CHAR_SIMPLE 0
#define TYPED_CHAR_ESCAPE 1

using namespace std;

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

  bool is_simple() { return type == TYPED_CHAR_SIMPLE; }

  bool is_escape() { return type == TYPED_CHAR_ESCAPE; }

  char simple() { return c.ch; }

  EscapeChar escape() { return c.escapeCh; }
};

void reportAndExit(const char *s) {
  perror(s);
  exit(EXIT_FAILURE);
}

enum class TokenState {
  Nothing,
  Word,
  Number,
  DoubleQuotedString,
};

struct TokenAnalyzer {
  vector<string> &tokenInfo;

  TokenAnalyzer(vector<string> &tokenInfo) : tokenInfo(tokenInfo) {}

  vector<SyntaxColorInfo> colorizeTokens(string &input) {
    string current{};
    TokenState state{TokenState::Nothing};
    vector<SyntaxColorInfo> out{};

    for (auto it = input.begin(); it != input.end(); it++) {
      bool tokenDidComplete{false};
      bool isLast = it + 1 == input.end();
      int end = distance(input.begin(), it);

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
          }
          break;
        case TokenState::Nothing:
          state = checkStart(&it, current);
          break;
      }

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
    switch (state) {
      case TokenState::Number:
        return "92";
      case TokenState::Word:
        return "93";
      case TokenState::DoubleQuotedString:
        return "96";
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

    if (isdigit(**it)) {
      state = TokenState::Number;
      foundNewStart = true;
    }

    if (isalpha(**it)) {
      state = TokenState::Word;
      foundNewStart = true;
    }

    if (foundNewStart) {
      current.clear();
      current.push_back(**it);
    }

    return state;
  }
};
