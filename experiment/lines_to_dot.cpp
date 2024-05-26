#include "lines.h"

int main(void) {
  Lines l{{"a", "b", "c", "d", "e", "f"}};

  l.split(2);
  l.split(4);

  LinesUtil::to_dot(l);

  return 0;
}
