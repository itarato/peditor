#pragma once

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cstdio>
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

void clearScreen() {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  resetCursorLocation();
}

pair<int, int> terminalDimension() {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    reportAndExit("Cannot detect window size.");
  }

  return pair<int, int>(ws.ws_col, ws.ws_row);
}

inline char ctrlKey(char c) { return c & 0x1f; }

char readKey() {
  char c;
  int result;

  while ((result = read(STDIN_FILENO, &c, 1)) != 1) {
    if (result == -1 && errno != EAGAIN) {
      reportAndExit("Failed reading input");
    }
  }

  return c;
}
