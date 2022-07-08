#pragma once

#include <algorithm>
#include <deque>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "command.h"
#include "debug.h"
#include "utility.h"

using namespace std;

static unordered_map<const char*, const char*> fileTypeAssociationMap{
    {".c++", "c++"}, {".cpp", "c++"}, {".hpp", "c++"},   {".h", "c++"},
    {".c", "c++"},   {".rb", "ruby"}, {".hs", "haskell"}};

struct TextView {
  int __cursorX{0};
  int __cursorY{0};
  int verticalScroll{0};
  int horizontalScroll{0};
  int xMemory{0};

  optional<string> fileName{nullopt};
  unordered_set<string> keywords{};

  vector<string> lines{};

  optional<SelectionEdge> selectionStart{nullopt};
  optional<SelectionEdge> selectionEnd{nullopt};

  deque<Command> undos{};
  deque<Command> redos{};

  bool isDirty{false};

  void saveFile() {
    DLOG("Save file: %s", fileName.value().c_str());
    ofstream f(fileName.value(), ios::out | ios::trunc);

    for (int i = 0; i < (int)lines.size(); i++) {
      f << lines[i];

      if (i < (int)lines.size()) {
        f << endl;
      }
    }

    f.close();

    isDirty = false;
  }

  void setFileName(string newFileName) {
    fileName = optional<string>(newFileName);
  }

  void reloadKeywordList() {
    keywords.clear();

    if (!fileName.has_value()) {
      DLOG("No file, cannot load keyword list.");
      return;
    }

    auto ext = filesystem::path(fileName.value()).extension();
    auto it =
        find_if(fileTypeAssociationMap.begin(), fileTypeAssociationMap.end(),
                [&](auto& kv) { return kv.first == ext; });

    if (it == fileTypeAssociationMap.end()) {
      DLOG("Cannot find keyword file for extension: %s", ext.c_str());
      return;
    }

    filesystem::path keywordFilePath{"./config/keywords/"};
    keywordFilePath /= it->second;

    if (!filesystem::exists(keywordFilePath)) {
      DLOG("Missing keyword file %s", keywordFilePath.c_str());
    }

    ifstream f(keywordFilePath);

    if (!f.is_open()) reportAndExit("Failed opening file");

    for (string line; getline(f, line);) {
      keywords.insert(line);
    }
    f.close();
  }
};
