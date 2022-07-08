#pragma once

using namespace std;

struct Config {
  int tabSize{2};
  void setTabSize(int newTabSize) { tabSize = newTabSize; }
};
