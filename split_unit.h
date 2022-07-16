#pragma once

#include <vector>

#include "text_view.h"

using namespace std;

struct SplitUnit {
  vector<TextView> textViews{};
  int activeTextViewIdx{0};

  SplitUnit(int cols, int rows) { textViews.emplace_back(cols, rows); }

  inline TextView* activeTextView() { return &(textViews[activeTextViewIdx]); }
};
