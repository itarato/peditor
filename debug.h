#pragma once

#include <cstdarg>
#include <cstdio>

const char* debugFileName{"./debug.txt"};

#define DLOG(...) dlog(__FILE__, __LINE__, __VA_ARGS__)

void dlog(const char* fileName, int lineNo, const char* s, ...) {
  FILE* f = fopen(debugFileName, "a+");

  va_list args;
  va_start(args, s);

  fprintf(f, "[%s:%d] ", fileName, lineNo);
  vfprintf(f, s, args);
  fprintf(f, "\n");

  va_end(args);

  fclose(f);
}
