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
  int colorCode;

  SyntaxColorInfo(int start, int end, int colorCode)
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

int analyzeToken(TokenState state, string &token) { return -1; }

vector<SyntaxColorInfo> colorizeTokens(string &input, vector<string> &tokens) {
  string current{};
  TokenState state;
  vector<SyntaxColorInfo> out{};
  bool foundNewStart{false};

  for (auto it = input.begin(); it != input.end(); it++) {
    bool tokenDidComplete{false};

    switch (state) {
      case TokenState::Word:
        if (isalpha(*it)) {
          current.push_back(*it);
        } else {
          tokenDidComplete = true;
        }
        break;
      case TokenState::Number:
        if (isdigit(*it)) {
          current.push_back(*it);
        } else {
          tokenDidComplete = true;
        }
        break;
      case TokenState::DoubleQuotedString:
        current.push_back(*it);

        if (*it == '"') tokenDidComplete = true;
        break;
      case TokenState::Nothing:
        // Noop.
        break;
    }

    if (tokenDidComplete) {
      int colorResult = analyzeToken(state, current);
      if (colorResult >= 0) {
        int end = distance(input.begin(), it);
        out.emplace_back(end - current.size(), end, colorResult);
      }

      state = TokenState::Nothing;
    }

    if (state == TokenState::Nothing) {
      foundNewStart = false;

      if (*it == '"') {
        state = TokenState::DoubleQuotedString;
        foundNewStart = true;
      }

      if (isdigit(*it)) {
        state = TokenState::Number;
        foundNewStart = true;
      }

      if (isalpha(*it)) {
        state = TokenState::Word;
        foundNewStart = true;
      }

      if (foundNewStart) {
        current.clear();
        current.push_back(*it);
      }
    }
  }

  return out;
}
