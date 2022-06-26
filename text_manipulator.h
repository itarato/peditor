#pragma once

#include <string>
#include <vector>

#include "command.h"
#include "utility.h"

using namespace std;

namespace TextManipulator {

void execute(Command *cmd, vector<string> *lines) {
  if (cmd->type == CommandType::InsertChar) {
    lines->at(cmd->row).insert(cmd->col, 1, cmd->memoryChr);
  } else if (cmd->type == CommandType::DeleteChar) {
    lines->at(cmd->row).erase(cmd->col, 1);
  } else if (cmd->type == CommandType::MergeLine) {
    lines->at(cmd->row).append(lines->at(cmd->row + 1));

    auto lineIt = lines->begin();
    advance(lineIt, cmd->row + 1);
    lines->erase(lineIt);
  } else {
    reportAndExit("Unknown command.");
  }
}

void reverse(Command *cmd, vector<string> *lines) {
  if (cmd->type == CommandType::InsertChar) {
    lines->at(cmd->row).erase(cmd->col, 1);
  } else if (cmd->type == CommandType::DeleteChar) {
    lines->at(cmd->row).insert(cmd->col, 1, cmd->memoryChr);
  } else if (cmd->type == CommandType::MergeLine) {
    auto rowIt = lines->at(cmd->row).begin();
    advance(rowIt, cmd->col);
    string newLine(rowIt, lines->at(cmd->row).end());

    auto rowIt2 = lines->at(cmd->row).begin();
    advance(rowIt2, cmd->col);
    lines->at(cmd->row).erase(rowIt2, lines->at(cmd->row).end());

    auto lineIt = lines->begin();
    advance(lineIt, cmd->row + 1);
    lines->insert(lineIt, newLine);
  } else {
    reportAndExit("Unknown command.");
  }
}

}  // namespace TextManipulator
