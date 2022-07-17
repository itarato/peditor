#pragma once

#include <vector>

#include "text_view.h"

using namespace std;

struct SplitUnit {
  vector<TextView> textViews{};
  int activeTextViewIdx{0};
  int topMargin{0};

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

  void updateMargins() {
    topMargin = hasMultipleTabs() ? 1 : 0;
    for (auto& textView : textViews) {
      // TODO: We can limit to only visible ones.
      textView.updateMargins();
    }
  }
};
