#pragma once

#include <string>

using namespace std;

enum class CommandType {
  // Insert a single character at position.
  // No memory
  InsertChar,

  // Insert a string.
  // Memory: snippet
  InsertSlice,

  // Remove a character at a position.
  // Memory: removed char
  DeleteChar,

  // Remove a whole slice from a line.
  // Memory: removed slice
  DeleteSlice,

  // Remove a whole line
  // Memory: line content
  DeleteLine,

  // Divide a line to 2 lines
  // No memory
  SplitLine,

  // Merge 2 lines to 1
  // Memory: split point
  MergeLine,

  // Swap 2 lines
  // Memory: indices
  SwapLine,
};

struct Command {
  CommandType type;

  int row{-1};
  int col{-1};

  string memoryStr{};
  char memoryChr{'\0'};

  Command(CommandType type, int row) : type(type), row(row) {}

  Command(CommandType type, int row, int col)
      : type(type), row(row), col(col) {}

  Command(CommandType type, int row, int col, string memoryStr)
      : type(type), row(row), col(col), memoryStr(memoryStr) {}

  Command(CommandType type, int row, string memoryStr)
      : type(type), row(row), memoryStr(memoryStr) {}

  Command(CommandType type, int row, int col, char memoryChr)
      : type(type), row(row), col(col), memoryChr(memoryChr) {}

  static inline Command makeInsertChar(int row, int col, char c) {
    return Command(CommandType::InsertChar, row, col, c);
  }

  static inline Command makeDeleteChar(int row, int col, char c) {
    return Command(CommandType::DeleteChar, row, col, c);
  }

  static inline Command makeMergeLine(int row, int col) {
    return Command(CommandType::MergeLine, row, col);
  }

  static inline Command makeDeleteLine(int row, string memory) {
    return Command(CommandType::DeleteLine, row, memory);
  }

  static inline Command makeDeleteSlice(int row, int col, string memory) {
    return Command(CommandType::DeleteSlice, row, col, memory);
  }

  static inline Command makeSplitLine(int row, int col) {
    return Command(CommandType::SplitLine, row, col);
  }

  static inline Command makeInsertSlice(int row, int col, string memory) {
    return Command(CommandType::InsertSlice, row, col, memory);
  }

  static inline Command makeSwapLine(int row) {
    return Command(CommandType::SwapLine, row);
  }
};
