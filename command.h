#pragma once

#include <string>

using namespace std;

enum class CommandType {
  // Insert a single character at position.
  // No memory
  InsertChar,

  // Remove a character at a position.
  // Memory: removed char
  RemoveChar,

  // Remove a whole slice from a line.
  // Memory: removed slice
  RemoveSlice,

  // Remove a whole line
  // Memory: line content
  DeleteLine,

  // Divide a line to 2 lines
  // No memory
  SplitLine,

  // Merge 2 lines to 1
  // Memory: split point
  MergeLine,
};

struct Command {
  CommandType type;

  int row{-1};
  int col{-1};

  string memoryStr{};
  char memoryChr{'\0'};
  bool isMemoryChar{true};

  Command(CommandType type, int row, int col)
      : type(type), row(row), col(col) {}

  Command(CommandType type, int row, int col, string memoryStr)
      : type(type),
        row(row),
        col(col),
        memoryStr(memoryStr),
        isMemoryChar(false) {}

  Command(CommandType type, int row, int col, char memoryChr)
      : type(type),
        row(row),
        col(col),
        memoryChr(memoryChr),
        isMemoryChar(true) {}

  static Command makeInsertChar(int row, int col, char c) {
    return Command(CommandType::InsertChar, row, col, c);
  }
};
