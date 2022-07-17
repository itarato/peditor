#pragma once

#include <vector>

#include "text_view.h"

using namespace std;

struct SplitUnit {
  vector<TextView> textViews{};
  int activeTextViewIdx{0};
  int topMargin{0};

  int cols{0};
  int rows{0};

  SplitUnit() { textViews.emplace_back(); }

  inline TextView* activeTextView() { return &(textViews[activeTextViewIdx]); }

  void newTextView() {
    textViews.emplace_back(textViewCols(), textViewRows());
    activeTextViewIdx = textViews.size() - 1;
  }

  void setActiveTextViewIdx(int newValue) {
    activeTextViewIdx = newValue % textViews.size();
  }

  inline bool hasMultipleTabs() const { return textViews.size() > 1; }

  void drawLine(string& out, int lineIdx, optional<string>& searchTerm) {
    if (lineIdx == 0 && hasMultipleTabs()) {
      generateTextViewsTabsLine(out);
      return;
    }

    int textViewLineIdx = hasMultipleTabs() ? lineIdx - 1 : lineIdx;
    activeTextView()->drawLine(out, textViewLineIdx, searchTerm);
  }

  void generateTextViewsTabsLine(string& out) {
    int maxTitleSize = cols / textViews.size();

    out.append("\x1b[7m\x1b[90m");

    if (maxTitleSize < 5) {
      out.append("\x1b[39m ");
      out.append(
          activeTextView()->fileName().value_or("<no file>").substr(0, cols));
    } else {
      for (int i = 0; i < (int)textViews.size(); i++) {
        if (i == activeTextViewIdx) {
          out.append("\x1b[39m ");
        } else {
          out.append("\x1b[90m ");
        }
        // TODO only use filename part
        out.append(textViews[i]
                       .fileName()
                       .value_or("<no file>")
                       .substr(0, maxTitleSize - 3));

        if (i < (int)textViews.size() - 1) {
          out.append(" \x1b[90m:");
        } else {
          out.append(" \x1b[90m");
        }
      }
    }

    string suffix(cols - visibleCharCount(out), ' ');
    out.append(suffix);
    out.append("\x1b[0m");
  }

  inline int textViewCols() const { return cols; }
  inline int textViewRows() const { return rows - topMargin; }

  void updateDimensions(int newCols, int newRows) {
    cols = newCols;
    rows = newRows;

    topMargin = hasMultipleTabs() ? 1 : 0;

    for (auto& textView : textViews) {
      textView.updateDimensions(textViewCols(), textViewRows());
    }
  }
};
