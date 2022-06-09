#pragma once

#include <optional>
#include <string>

using namespace std;

struct Config {
  int tabSize{4};
  optional<string> fileName{nullopt};

  void setTabSize(int newTabSize) { tabSize = newTabSize; }

  void setFileName(string newFileName) {
    fileName = optional<string>(newFileName);
  }
};
