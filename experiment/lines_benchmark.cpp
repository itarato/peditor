#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

#include "lines.h"

using namespace std;

template <typename F>
void measure(string name, const F& fn) {
  clock_t t_start = clock();

  fn();

  clock_t t_end = clock();
  double t_elapsed = 1000.0 * (t_end - t_start) / CLOCKS_PER_SEC;

  printf("%s | T: %.2f ms\n", name.c_str(), t_elapsed);
}

void benchmark_lines_emplace_back(size_t unit_break_threshold) {
  Lines l{make_shared<LinesConfig>(unit_break_threshold)};

  string name = "Lines insert 1M times with threshold " + to_string(unit_break_threshold);
  ifstream fin("/home/itarato/CHECKOUT/p1brc/data/weather_1M.csv");
  measure(name, [&]() {
    for (string line; getline(fin, line);) {
      l.emplace_back(line);
    }
  });
}

void benchmark_vector_emplace_back() {
  ifstream fin("/home/itarato/CHECKOUT/p1brc/data/weather_1M.csv");
  vector<string> lines{};

  measure("Vector insert 1M times", [&]() {
    for (string line; getline(fin, line);) {
      lines.emplace_back(line);
    }
  });
}

void emplace_back_benchmark() {
  for (size_t i = 8; i <= 8192; i *= 2) {
    benchmark_lines_emplace_back(i);
  }

  benchmark_vector_emplace_back();
}

void heavy_run_emplace_back() {
  Lines l{make_shared<LinesConfig>((size_t)512)};
  ifstream fin("/home/itarato/CHECKOUT/p1brc/data/weather_1M.csv");

  for (string line; getline(fin, line);) {
    l.emplace_back(line);
  }
}

int main(void) {
  emplace_back_benchmark();
  // heavy_run_emplace_back();
  return EXIT_SUCCESS;
}
