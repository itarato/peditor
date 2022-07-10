#pragma once

#include <deque>
#include <optional>
#include <utility>
#include <vector>

#include "command.h"
#include "utility.h"

#define UNDO_LIMIT 64

using namespace std;

struct HistoryUnit {
  vector<Command> commands{};

  optional<SelectionEdge> beforeSelectionStart;
  optional<SelectionEdge> beforeSelectionEnd;
  Point beforeCursor;

  optional<SelectionEdge> afterSelectionStart;
  optional<SelectionEdge> afterSelectionEnd;
  Point afterCursor;

  // Debug flag protecting against nested blocks.
  bool final{false};

  HistoryUnit() {}
};

struct History {
  deque<HistoryUnit> undos;
  deque<HistoryUnit> redos;

  void newBlock(ITextViewState* textViewState) {
    if (!undos.empty() && !undos.back().final)
      reportAndExit("Nested history detected");

    redos.clear();

    undos.emplace_back();

    last().beforeSelectionStart = textViewState->getSelectionStart();
    last().beforeSelectionEnd = textViewState->getSelectionEnd();
    last().beforeCursor = textViewState->getCursor();

    while (undos.size() > UNDO_LIMIT) undos.pop_front();
  }

  void closeBlock(ITextViewState* textViewState) {
    last().afterSelectionStart = textViewState->getSelectionStart();
    last().afterSelectionEnd = textViewState->getSelectionEnd();
    last().afterCursor = textViewState->getCursor();

    last().final = true;
  }

  void record(Command&& cmd) {
    if (last().final) reportAndExit("Adding command to a final unit");

    last().commands.push_back(move(cmd));
  }

  HistoryUnit& useUndo() {
    if (undos.empty()) reportAndExit("Empty undo list, cannot undo");

    redos.push_back(undos.back());
    undos.pop_back();

    return redos.back();
  }

  HistoryUnit& useRedo() {
    if (redos.empty()) reportAndExit("Empty redo list, cannot undo");

    undos.push_back(redos.back());
    redos.pop_back();

    return undos.back();
  }

  HistoryUnit& last() {
    if (undos.empty()) reportAndExit("Empty history");

    return undos.back();
  }
};
