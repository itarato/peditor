#pragma once

#include <string>
#include <vector>

#include "command.h"
#include "utility.h"

using namespace std;

namespace TextManipulator {

void execute(Command *cmd, vector<string> *lines) {
  switch (cmd->type) {
    case CommandType::InsertChar:
      lines->at(cmd->row).insert(cmd->col, 1, cmd->memoryChr);
      break;
    default:
      reportAndExit("Unknown command.");
  }
}

}  // namespace TextManipulator
