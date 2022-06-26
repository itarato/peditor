#include <iostream>
#include <vector>

#include "utility.h"

using namespace std;

#define ASSERT_EQ(v1, v2) assert_eq(v1, v2, __LINE__)

template <

typename T>
void assert_eq(T v1, T v2, int lineNo) {
  if (v1 == v2) {
    cout << ".";
  } else {
    cout << "\n\nERROR on line " << lineNo << ": expected " << v1
         << " to be equal with " << v2 << ".\n\n";
  }
}

void test_find_number_beginning() {
  string raw = "123   ";
  SyntaxHighlightConfig conf{};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(1, (int)result.size());
  ASSERT_EQ(0, (int)result[0].start);
  ASSERT_EQ(2, (int)result[0].end);
}

void test_find_number_middle() {
  string raw = "  123   ";
  SyntaxHighlightConfig conf{};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(1, (int)result.size());
  ASSERT_EQ(2, (int)result[0].start);
  ASSERT_EQ(4, (int)result[0].end);
}

void test_find_number_end() {
  string raw = "   123";
  SyntaxHighlightConfig conf{};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(1, (int)result.size());
  ASSERT_EQ(3, (int)result[0].start);
  ASSERT_EQ(5, (int)result[0].end);
}

void test_single_find_number_beginning() {
  string raw = "1   ";
  SyntaxHighlightConfig conf{};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(1, (int)result.size());
  ASSERT_EQ(0, (int)result[0].start);
  ASSERT_EQ(0, (int)result[0].end);
}

void test_single_find_number_middle() {
  string raw = "  1   ";
  SyntaxHighlightConfig conf{};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(1, (int)result.size());
  ASSERT_EQ(2, (int)result[0].start);
  ASSERT_EQ(2, (int)result[0].end);
}

void test_single_find_number_end() {
  string raw = "   1";
  SyntaxHighlightConfig conf{};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(1, (int)result.size());
  ASSERT_EQ(3, (int)result[0].start);
  ASSERT_EQ(3, (int)result[0].end);
}

void test_find_string() {
  string raw = "\"abc\"";
  SyntaxHighlightConfig conf{};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(1, (int)result.size());
  ASSERT_EQ(0, (int)result[0].start);
  ASSERT_EQ(4, (int)result[0].end);
}

void test_find_string_middle() {
  string raw = " \"abc\" ";
  SyntaxHighlightConfig conf{};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(1, (int)result.size());
  ASSERT_EQ(1, (int)result[0].start);
  ASSERT_EQ(5, (int)result[0].end);
}

void test_find_single_quoted_string() {
  string raw = "--'a'--";
  SyntaxHighlightConfig conf{};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(1, (int)result.size());
  ASSERT_EQ(2, (int)result[0].start);
  ASSERT_EQ(4, (int)result[0].end);
}

void test_find_word() {
  string raw = "for";
  SyntaxHighlightConfig conf{};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(1, (int)result.size());
  ASSERT_EQ(0, (int)result[0].start);
  ASSERT_EQ(2, (int)result[0].end);
}

void test_does_not_find_unknown_word() {
  string raw = "hello for ever";
  SyntaxHighlightConfig conf{};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(1, (int)result.size());
  ASSERT_EQ(6, (int)result[0].start);
  ASSERT_EQ(8, (int)result[0].end);
}

void test_find_complex_examples() {
  string raw = "for 123for x3 \"12'ab\"";
  SyntaxHighlightConfig conf{};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(5, (int)result.size());

  ASSERT_EQ(0, (int)result[0].start);
  ASSERT_EQ(2, (int)result[0].end);

  ASSERT_EQ(4, (int)result[1].start);
  ASSERT_EQ(6, (int)result[1].end);

  ASSERT_EQ(7, (int)result[2].start);
  ASSERT_EQ(9, (int)result[2].end);

  ASSERT_EQ(12, (int)result[3].start);
  ASSERT_EQ(12, (int)result[3].end);

  ASSERT_EQ(14, (int)result[4].start);
  ASSERT_EQ(20, (int)result[4].end);
}

void test_next_word_jump_location() {
  string s;

  s = "abc   ";
  ASSERT_EQ(3, nextWordJumpLocation(s, 0));
  ASSERT_EQ(3, nextWordJumpLocation(s, 1));
  ASSERT_EQ(6, nextWordJumpLocation(s, 2));
  ASSERT_EQ(6, nextWordJumpLocation(s, 5));
  ASSERT_EQ(6, nextWordJumpLocation(s, 8));

  s = " abc_ _GHI  ";
  ASSERT_EQ(7, nextWordJumpLocation(s, 3));
  ASSERT_EQ(0, nextWordJumpLocation(s, -1));

  s = "abc";
  ASSERT_EQ(3, nextWordJumpLocation(s, 0));
}

void test_prev_word_jump_location() {
  string s;

  s = "abc   ";
  ASSERT_EQ(2, prevWordJumpLocation(s, 5));
  ASSERT_EQ(2, prevWordJumpLocation(s, 4));
  ASSERT_EQ(-1, prevWordJumpLocation(s, 3));
  ASSERT_EQ(-1, prevWordJumpLocation(s, 2));
  ASSERT_EQ(-1, prevWordJumpLocation(s, 0));
  ASSERT_EQ(-1, prevWordJumpLocation(s, -1));

  s = " abc_ _GHI  ";
  ASSERT_EQ(3, prevWordJumpLocation(s, 7));

  s = "abc";
  ASSERT_EQ(-1, prevWordJumpLocation(s, 3));
}

int main() {
  cout << "Tests\n";

  test_find_number_beginning();
  test_find_number_middle();
  test_find_number_end();

  test_single_find_number_beginning();
  test_single_find_number_middle();
  test_single_find_number_end();

  test_find_string();
  test_find_string_middle();

  test_find_single_quoted_string();

  test_find_word();
  test_does_not_find_unknown_word();

  test_find_complex_examples();

  test_next_word_jump_location();
  test_prev_word_jump_location();

  cout << endl;
}