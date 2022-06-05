#pragma once

#include <cstdio>
#include <cstdlib>

void reportAndExit(const char* s) {
  perror(s);
  exit(EXIT_FAILURE);
}
