#pragma once

#include <cstdarg>
#include <cstdio>

const char* debugFileName{"./debug.txt"};

#ifdef VERBOSE
#define DLOG(...) dlog(__FILE__, __LINE__, __VA_ARGS__)
#else
#define DLOG(...) \
  {}
#endif

void dlog(const char* fileName, int lineNo, const char* s, ...) {
  FILE* f = fopen(debugFileName, "a+");

  va_list args;
  va_start(args, s);

  fprintf(f, "\x1b[93m%16s\x1b[39m:\x1b[96m%-4d\x1b[0m \x1b[94m", fileName,
          lineNo);
  vfprintf(f, s, args);
  fprintf(f, "\x1b[0m\n");

  va_end(args);

  fclose(f);
}
