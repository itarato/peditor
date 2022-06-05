#include <termios.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

using namespace std;

struct termios orig_termios;

inline char ctrlKey(char c) { return c & 0x1f; }

void reportAndExit(const char* s) {
  perror(s);
  exit(EXIT_FAILURE);
}

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

void runLoop() {
  char c;
  for (;;) {
    c = readKey();

    if (iscntrl(c)) {
      // Ignore for now.
      printf("CTRL(%d)", uint(c));
    } else {
      printf("%c", c);
    }

    if (c == ctrlKey('q')) {
      break;
    }

    fflush(STDIN_FILENO);
  }
}

int main() {
  preserveTermiosOriginalState();
  enableRawMode();

  runLoop();

  return EXIT_SUCCESS;
}
