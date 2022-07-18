#pragma once

#include <string>
#include <vector>

#include "utility.h"

using namespace std;

enum class PromptCommand {
  Nothing,
  SaveFileAs,
  OpenFile,
  MultiPurpose,
  FileHasBeenModified,
};

struct Prompt {
  string prefix{};
  PromptCommand command{PromptCommand::Nothing};
  string rawMessage{};

  bool isAutoCompleteOn{false};
  vector<string> messageOptions{};

  void reset(string newPrefix, PromptCommand newCommand) {
    prefix = newPrefix;
    rawMessage.clear();
    command = newCommand;
    isAutoCompleteOn = false;
  }

  void reset(string newPrefix, PromptCommand newCommand,
             vector<string>&& newMessageOptions) {
    reset(newPrefix, newCommand);

    isAutoCompleteOn = true;
    messageOptions = move(newMessageOptions);
  }

  string message(bool withHighlights = false) {
    if (isAutoCompleteOn) {
      auto filePaths = directoryFiles();
      auto matches = poormansFuzzySearch(rawMessage, filePaths, 1);

      if (matches.empty()) {
        return rawMessage;
      } else if (withHighlights) {
        return highlightPoormanFuzzyMatch(rawMessage, matches[0]);
      } else {
        return matches[0];
      }
    } else {
      return rawMessage;
    }
  }

  inline int messageVisibleSize() {
    string s{message()};
    return visibleCharCount(s);
  }
};