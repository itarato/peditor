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
    lines[cmd->row].swap(lines[cmd->row + 1]);
  } else {
    reportAndExit("Unknown command.");
  }
}

void reverse(Command *cmd, Lines &lines) {
  if (cmd->type == CommandType::InsertChar) {
    lines[cmd->row].erase(cmd->col, 1);
  } else if (cmd->type == CommandType::DeleteChar) {
    lines[cmd->row].insert(cmd->col, 1, cmd->memoryChr);
  } else if (cmd->type == CommandType::MergeLine) {
    lines.insert(cmd->row, cmd->col, "\n");
  } else if (cmd->type == CommandType::DeleteLine) {
    lines.insert_line(cmd->row, cmd->memoryStr);
  } else if (cmd->type == CommandType::DeleteSlice) {
    lines[cmd->row].insert(cmd->col, cmd->memoryStr);
  } else if (cmd->type == CommandType::SplitLine) {
    lines.backspace(cmd->row + 1, 0);
  } else if (cmd->type == CommandType::InsertSlice) {
    lines[cmd->row].erase(cmd->col, cmd->memoryStr.size());
  } else if (cmd->type == CommandType::SwapLine) {
    lines[cmd->row].swap(lines[cmd->row + 1]);
  } else {
    reportAndExit("Unknown revert command.");
  }
}

}  // namespace TextManipulator
