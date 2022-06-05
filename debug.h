#pragma once

#include <cstdarg>
#include <cstdio>

const char* debugFileName{"./debug.txt"};

void dlog(const char* s, ...) {
  FILE* f = fopen(debugFileName, "a+");

  va_list args;
  va_start(args, s);

  fprintf(f, "[dbg] ");
  vfprintf(f, s, args);
  fprintf(f, "\n");

  va_end(args);

  fclose(f);
}
