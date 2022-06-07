#pragma once

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <utility>

#include "utility.h"

using namespace std;

struct termios orig_termios;

void enableRawMode() {
  struct termios raw = orig_termios;

  raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
  raw.c_oflag &= ~(OPOST);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_cflag |= (CS8);

  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
    reportAndExit("Failed saving tc data.");
  }
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1) {
    reportAndExit("Failed disabling raw mode.");
  }
}

void preserveTermiosOriginalState() {
  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) {
    reportAndExit("Failed fetching tc data.");
  }
  atexit(disableRawMode);
}

void resetCursorLocation() { write(STDOUT_FILENO, "\x1b[H", 3); }

void setCursorLocation(int row, int col) {
  char buf[32];

  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row + 1, col + 1);
  write(STDOUT_FILENO, buf, strlen(buf));
}

void clearScreen() { write(STDOUT_FILENO, "\x1b[2J", 4); }

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  u_int8_t i = 0;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }

  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

  return 0;
}

pair<int, int> getTerminalDimension() {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) {
      reportAndExit("Cannot detect window size.");
    }

    int rows;
    int cols;
    getCursorPosition(&rows, &cols);
    return pair<int, int>(rows, cols);
  }

  return pair<int, int>(ws.ws_row, ws.ws_col);
}

inline char ctrlKey(char c) { return c & 0x1f; }

TypedChar readKey() {
  char c;
  int result;

  while ((result = read(STDIN_FILENO, &c, 1)) != 1) {
    if (result == -1 && errno != EAGAIN) {
      reportAndExit("Failed reading input");
    }
  }

  if (c == '\x1b') {
    char c1, c2;
    if (read(STDIN_FILENO, &c1, 1) != 1 || c1 != '[') return c;
    if (read(STDIN_FILENO, &c2, 1) != 1) return c;

    if (c2 == 'A') return TypedChar(EscapeChar::Up);
    if (c2 == 'B') return TypedChar(EscapeChar::Down);
    if (c2 == 'C') return TypedChar(EscapeChar::Right);
    if (c2 == 'D') return TypedChar(EscapeChar::Left);

    dlog("Uncaught escape char: %c", c2);
  }

  return TypedChar{c};
}

void hideCursor() { write(STDOUT_FILENO, "\x1b[?25l", 6); }

void showCursor() { write(STDOUT_FILENO, "\x1b[?25h", 6); }
