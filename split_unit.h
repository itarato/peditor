#pragma once

#include <vector>

#include "text_view.h"

using namespace std;

struct SplitUnit {
  vector<TextView> textViews{};
  int activeTextViewIdx{0};

  SplitUnit(int cols, int rows) { textViews.emplace_back(cols, rows); }

  inline TextView* activeTextView() { return &(textViews[activeTextViewIdx]); }

  void newTextView(int cols, int rows) {
    textViews.emplace_back(cols, rows);
    activeTextViewIdx = textViews.size() - 1;
    activeTextView()->reloadContent();
  }

  void setActiveTextViewIdx(int newValue) {
    activeTextViewIdx = newValue % textViews.size();
  }

  inline bool hasMultipleTabs() const { return textViews.size() > 1; }

  void drawLine(string& out, int lineIdx, optional<string>& searchTerm) {
    activeTextView()->drawLine(out, lineIdx, searchTerm);
  }
};
