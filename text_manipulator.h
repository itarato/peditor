#pragma once

#include <string>
#include <vector>

#include "command.h"
#include "experiment/lines.h"
#include "utility.h"

using namespace std;

namespace TextManipulator {

void execute(Command *cmd, Lines &lines) {
  if (cmd->type == CommandType::InsertChar) {
    lines[cmd->row].insert(cmd->col, 1, cmd->memoryChr);
  } else if (cmd->type == CommandType::DeleteChar) {
    lines[cmd->row].erase(cmd->col, 1);
  } else if (cmd->type == CommandType::MergeLine) {
    lines.backspace(cmd->row + 1, 0);
  } else if (cmd->type == CommandType::DeleteLine) {
    lines.remove_line(cmd->row);
  } else if (cmd->type == CommandType::DeleteSlice) {
    lines[cmd->row].erase(cmd->col, cmd->memoryStr.size());
  } else if (cmd->type == CommandType::SplitLine) {
    lines.insert(cmd->row, cmd->col, "\n");
  } else if (cmd->type == CommandType::InsertSlice) {
    lines.insert(cmd->row, cmd->col, cmd->memoryStr);
  } else if (cmd->type == CommandType::SwapLine) {
    auto lineIt = lines->begin();
    advance(lineIt, cmd->row);
    iter_swap(lineIt, lineIt + 1);
  } else {
    reportAndExit("Unknown command.");
  }
}

void reverse(Command *cmd, Lines *lines) {
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
  } else if (cmd->type == CommandType::DeleteLine) {
    auto lineIt = lines->begin();
    advance(lineIt, cmd->row);
    lines->insert(lineIt, cmd->memoryStr);
  } else if (cmd->type == CommandType::DeleteSlice) {
    lines->at(cmd->row).insert(cmd->col, cmd->memoryStr);
  } else if (cmd->type == CommandType::SplitLine) {
    lines->at(cmd->row).append(lines->at(cmd->row + 1));

    auto lineIt = lines->begin();
    advance(lineIt, cmd->row + 1);
    lines->erase(lineIt);
  } else if (cmd->type == CommandType::InsertSlice) {
    lines->at(cmd->row).erase(cmd->col, cmd->memoryStr.size());
  } else if (cmd->type == CommandType::SwapLine) {
    auto lineIt = lines->begin();
    advance(lineIt, cmd->row);
    iter_swap(lineIt, lineIt + 1);
  } else {
    reportAndExit("Unknown revert command.");
  }
}

}  // namespace TextManipulator
