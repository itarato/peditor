#pragma once

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <utility>

#include "debug.h"
#include "utility.h"

using namespace std;

struct termios orig_termios;

#define BACKSPACE char(127)
#define ESCAPE char(27)
#define ENTER char(13)
#define TAB char(9)
#define CTRL_BACKSPACE char(8)

vector<pair<string, EscapeChar>> escapeCharMap{
    {"[A", EscapeChar::Up},
    {"[B", EscapeChar::Down},
    {"[C", EscapeChar::Right},
    {"[D", EscapeChar::Left},
    {"[H", EscapeChar::Home},
    {"[F", EscapeChar::End},
    {"[1;5A", EscapeChar::CtrlUp},
    {"[1;5B", EscapeChar::CtrlDown},
    {"[1;5C", EscapeChar::CtrlRight},
    {"[1;5D", EscapeChar::CtrlLeft},
    // Shift + arrow is not a reliable combo, eg Konsole swallows it.
    // {"[1;2A", EscapeChar::ShiftUp},
    // {"[1;2B", EscapeChar::ShiftDown},
    // {"[1;2C", EscapeChar::ShiftRight},
    // {"[1;2D", EscapeChar::ShiftLeft},
    {"[5~", EscapeChar::PageUp},
    {"[6~", EscapeChar::PageDown},
    {"[3~", EscapeChar::Delete}};

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
void resetCursorLocation(string &out) { out.append("\x1b[H"); }

void setCursorLocation(string &out, int row, int col) {
  char buf[32];

  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row + 1, col + 1);
  out.append(buf, strlen(buf));
}

void clearScreen() { write(STDOUT_FILENO, "\x1b[2J", 4); }
void clearScreen(string &out) { out.append("\x1b[2J"); }

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

inline constexpr char ctrlKey(char c) { return c & 0x1f; }

TypedChar readKey() {
  char c;
  int result;

  while ((result = read(STDIN_FILENO, &c, 1)) != 1) {
    if (result == -1 && errno != EAGAIN && errno != EINTR) {
      reportAndExit("Failed reading input");
    }
  }

  if (c != '\x1b') return TypedChar{c};

  string combo{};

  for (int i = 0;; i++) {
    if (read(STDIN_FILENO, &c, 1) != 1) {
      DLOG("Cannot read follow up combo char");
      return i == 0 ? TypedChar{'\x1b'} : TYPED_CHAR_FAILURE;
    }

    combo.push_back(c);
    bool hasMatch{false};

    for (auto &[escapeCombo, escapeEnum] : escapeCharMap) {
      if (escapeCombo.starts_with(combo)) {
        if (escapeCombo == combo) return TypedChar{escapeEnum};

        hasMatch = true;
        break;
      }
    }

    if (!hasMatch) {
      DLOG("Failed detecting key combo. Prefix %s", combo.c_str());
      return TYPED_CHAR_FAILURE;
    }
  }
}

void hideCursor() { write(STDOUT_FILENO, "\x1b[?25l", 6); }

void showCursor(string &out) { out.append("\x1b[?25h"); }
