#pragma once

#include <vector>

#include "text_view.h"

using namespace std;

struct SplitUnit {
  vector<TextView> textViews{};
  int activeTextViewIdx{0};
  int topMargin{0};
  bool hasMultipleSplitUnits{false};

  int cols{0};
  int rows{0};

  SplitUnit() {
    textViews.emplace_back();
  }

  inline TextView* activeTextView() {
    return &(textViews[activeTextViewIdx]);
  }

  void newTextView() {
    textViews.emplace_back(textViewCols(), textViewRows());
    setActiveTextViewIdx(textViews.size() - 1);
    updateInternalDimensions();
  }

  void closeTextView() {
    auto viewIt = textViews.begin();
    advance(viewIt, activeTextViewIdx);
    textViews.erase(viewIt);

    updateInternalDimensions();
    setActiveTextViewIdx(activeTextViewIdx - 1);
  }

  void setActiveTextViewIdx(int newValue) {
    activeTextViewIdx = (newValue + textViews.size()) % textViews.size();
  }

  inline bool hasMultipleTabs() const {
    return textViews.size() > 1;
  }

  void drawLine(string& out, int lineIdx, optional<string>& searchTerm) {
    if (lineIdx == 0 && needTabBar()) {
      generateTextViewsTabsLine(out);
      return;
    }

    int textViewLineIdx = needTabBar() ? lineIdx - 1 : lineIdx;
    activeTextView()->drawLine(out, textViewLineIdx, searchTerm);
  }

  inline bool needTabBar() const {
    return hasMultipleTabs() || hasMultipleSplitUnits;
  }

  void generateTextViewsTabsLine(string& out) {
    string tabsLine{};

    int maxTitleSize = cols / textViews.size();

    tabsLine.append("\x1b[7m\x1b[90m");

    if (maxTitleSize < 5) {
      tabsLine.append("\x1b[39m ");
      tabsLine.append(activeTextView()->fileName().value_or("<no file>").substr(0, cols));
    } else {
      for (int i = 0; i < (int)textViews.size(); i++) {
        if (i == activeTextViewIdx) {
          tabsLine.append("\x1b[39m ");
        } else {
          tabsLine.append("\x1b[90m ");
        }

        if (textViews[i].isDirty) tabsLine.append("\x1b[1m\x1b[41m*\x1b[49m\x1b[21m");

        tabsLine.append(textViews[i].fileName().value_or("<no file>").substr(0, maxTitleSize - 3));

        if (i < (int)textViews.size() - 1) {
          tabsLine.append(" \x1b[90m:");
        } else {
          tabsLine.append(" \x1b[90m");
        }
      }
    }

    int paddingSpacesLen = cols - visibleCharCount(tabsLine);
    if (paddingSpacesLen > 0) {
      string suffix(paddingSpacesLen, ' ');
      tabsLine.append(suffix);
    }
    tabsLine.append("\x1b[0m");

    out.append(tabsLine);
  }

  inline int textViewCols() const {
    return cols;
  }
  inline int textViewRows() const {
    return rows - topMargin;
  }

  void updateInternalDimensions() {
    updateTopMargin();

    for (auto& textView : textViews) {
      textView.updateDimensions(textViewCols(), textViewRows());
    }
  }

  void updateDimensions(int newCols, int newRows, bool newHasMultipleSplitUnits) {
    cols = newCols;
    rows = newRows;
    hasMultipleSplitUnits = newHasMultipleSplitUnits;

    updateInternalDimensions();
  }

  inline void updateTopMargin() {
    topMargin = needTabBar() ? 1 : 0;
  }
};
