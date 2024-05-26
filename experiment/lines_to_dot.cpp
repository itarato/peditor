#include <string>
#include <utility>
#include <vector>

#include "lines.h"

int main(void) {
  Lines l{make_shared<LinesConfig>((size_t)3), {}};
  vector<string> input{"a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m",
                       "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z"};
  for (const auto &e : input) l.emplace_back(e);

  LinesUtil::to_dot(l);

  return 0;
}
