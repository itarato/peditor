#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

#include "rope.h"

using namespace std;

template <typename F>
void measure(string name, const F& fn) {
  clock_t t_start = clock();

  fn();

  clock_t t_end = clock();
  double t_elapsed = 1000.0 * (t_end - t_start) / CLOCKS_PER_SEC;

  printf("%s | T: %.2f ms\n", name.c_str(), t_elapsed);
}

string load_medium_string() {
  ifstream fin("/home/itarato/CHECKOUT/p1brc/data/weather_1M.csv");
  stringstream buf;
  buf << fin.rdbuf();
  return buf.str();
}

void benchmark_insert_rope(size_t rope_unit_break_threshold) {
  Rope r{make_shared<RopeConfig>(rope_unit_break_threshold),
         load_medium_string()};

  string name = "Rope insert 1000 times + break threshold " +
                to_string(rope_unit_break_threshold);
  measure(name, [&]() {
    for (int i = 0; i < 10000; i += 10) {
      r.insert(i, "ok");
    }
  });
}

void benchmark_insert_string() {
  string s{load_medium_string()};

  measure("String insert 1000 times", [&]() {
    for (int i = 0; i < 10000; i += 10) {
      s.insert(i, "ok");
    }
  });
}

/*

Rope insert 1000 times + break threshold 8 | T: 19.55 ms
Rope insert 1000 times + break threshold 16 | T: 4.75 ms
Rope insert 1000 times + break threshold 32 | T: 4.17 ms
Rope insert 1000 times + break threshold 64 | T: 3.96 ms
Rope insert 1000 times + break threshold 128 | T: 3.83 ms
Rope insert 1000 times + break threshold 256 | T: 3.75 ms
Rope insert 1000 times + break threshold 512 | T: 3.61 ms
Rope insert 1000 times + break threshold 1024 | T: 3.53 ms
Rope insert 1000 times + break threshold 2048 | T: 3.82 ms
Rope insert 1000 times + break threshold 4096 | T: 3.45 ms
String insert 1000 times | T: 1024.83 ms

*/

int main(void) {
  for (size_t i = 8; i <= 4096; i *= 2) {
    benchmark_insert_rope(i);
  }

  benchmark_insert_string();

  return EXIT_SUCCESS;
}