#pragma once

#include <cstdio>
#include <cstdlib>

#define TYPED_CHAR_SIMPLE 0
#define TYPED_CHAR_ESCAPE 1

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

void reportAndExit(const char* s) {
  perror(s);
  exit(EXIT_FAILURE);
}
