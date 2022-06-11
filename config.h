#pragma once

#include <optional>
#include <string>
#include <vector>

using namespace std;

struct Config {
  int tabSize{4};
  optional<string> fileName{nullopt};
  vector<string> keywordList{"if", "while", "void"};

  void setTabSize(int newTabSize) { tabSize = newTabSize; }

  void setFileName(string newFileName) {
    fileName = optional<string>(newFileName);
  }
};
