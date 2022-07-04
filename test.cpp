#include <iostream>
#include <unordered_set>
#include <vector>

#include "utility.h"

using namespace std;

#define ASSERT_EQ(v1, v2) assert_eq(v1, v2, __LINE__)

template <typename T>
void assert_eq(T v1, T v2, int lineNo) {
  if (v1 == v2) {
    cout << ".";
  } else {
    cout << "\n\nFail!\nLine: " << lineNo << "\nExpected: <" << v1
         << ">\n  Actual: <" << v2 << ">\n\n";
  }
}

void test_find_number_beginning() {
  string raw = "123   ";
  SyntaxHighlightConfig conf{nullptr};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(2, (int)result.size());
  ASSERT_EQ(0, (int)result[0].pos);
  ASSERT_EQ(3, (int)result[1].pos);
}

void test_find_number_middle() {
  string raw = "  123   ";
  SyntaxHighlightConfig conf{nullptr};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(2, (int)result.size());
  ASSERT_EQ(2, (int)result[0].pos);
  ASSERT_EQ(5, (int)result[1].pos);
}

void test_find_number_end() {
  string raw = "   123";
  SyntaxHighlightConfig conf{nullptr};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(2, (int)result.size());
  ASSERT_EQ(3, (int)result[0].pos);
  ASSERT_EQ(6, (int)result[1].pos);
}

void test_single_find_number_beginning() {
  string raw = "1   ";
  SyntaxHighlightConfig conf{nullptr};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(2, (int)result.size());
  ASSERT_EQ(0, (int)result[0].pos);
  ASSERT_EQ(1, (int)result[1].pos);
}

void test_single_find_number_middle() {
  string raw = "  1   ";
  SyntaxHighlightConfig conf{nullptr};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(2, (int)result.size());
  ASSERT_EQ(2, (int)result[0].pos);
  ASSERT_EQ(3, (int)result[1].pos);
}

void test_single_find_number_end() {
  string raw = "   1";
  SyntaxHighlightConfig conf{nullptr};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(2, (int)result.size());
  ASSERT_EQ(3, (int)result[0].pos);
  ASSERT_EQ(4, (int)result[1].pos);
}

void test_find_string() {
  string raw = "\"abc\"";
  SyntaxHighlightConfig conf{nullptr};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(2, (int)result.size());
  ASSERT_EQ(0, (int)result[0].pos);
  ASSERT_EQ(5, (int)result[1].pos);
}

void test_find_string_middle() {
  string raw = " \"abc\" ";
  SyntaxHighlightConfig conf{nullptr};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(2, (int)result.size());
  ASSERT_EQ(1, (int)result[0].pos);
  ASSERT_EQ(6, (int)result[1].pos);
}

void test_find_single_quoted_string() {
  string raw = "--'a'--";
  SyntaxHighlightConfig conf{nullptr};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(2, (int)result.size());
  ASSERT_EQ(2, (int)result[0].pos);
  ASSERT_EQ(5, (int)result[1].pos);
}

void test_find_word() {
  string raw = "for";

  unordered_set<string> keywords{
      "for",
  };
  SyntaxHighlightConfig conf{&keywords};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(2, (int)result.size());
  ASSERT_EQ(0, (int)result[0].pos);
  ASSERT_EQ(3, (int)result[1].pos);
}

void test_does_not_find_unknown_word() {
  string raw = "hello for ever";

  unordered_set<string> keywords{
      "for",
  };
  SyntaxHighlightConfig conf{&keywords};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(2, (int)result.size());
  ASSERT_EQ(6, (int)result[0].pos);
  ASSERT_EQ(9, (int)result[1].pos);
}

void test_find_complex_examples() {
  string raw = "for 123for x3 \"12'ab\"";

  unordered_set<string> keywords{
      "for",
  };
  SyntaxHighlightConfig conf{&keywords};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(8, (int)result.size());

  ASSERT_EQ(0, (int)result[0].pos);
  ASSERT_EQ(3, (int)result[1].pos);

  ASSERT_EQ(4, (int)result[2].pos);
  ASSERT_EQ(7, (int)result[3].pos);

  ASSERT_EQ(7, (int)result[4].pos);
  ASSERT_EQ(10, (int)result[5].pos);

  ASSERT_EQ(14, (int)result[6].pos);
  ASSERT_EQ(21, (int)result[7].pos);
}

void test_parens() {
  string raw = "abc(";
  SyntaxHighlightConfig conf{nullptr};

  TokenAnalyzer ta{conf};
  auto result = ta.colorizeTokens(raw);

  ASSERT_EQ(2, (int)result.size());
  ASSERT_EQ(3, (int)result[0].pos);
  ASSERT_EQ(4, (int)result[1].pos);
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

void test_visibleCharCount() {
  string s{"abc\x1b[1mdef\x1b[21m123"};
  ASSERT_EQ(9, visibleCharCount(s));
}

void test_visibleStrRightCut() {
  string s{"abc\x1b[1mdef\x1b[21m"};
  ASSERT_EQ(2, visibleStrRightCut(s, 2));
  ASSERT_EQ(15, visibleStrRightCut(s, 100));
  ASSERT_EQ(7, visibleStrRightCut(s, 3));
  ASSERT_EQ(8, visibleStrRightCut(s, 4));
  ASSERT_EQ(9, visibleStrRightCut(s, 5));
  ASSERT_EQ(15, visibleStrRightCut(s, 6));
}

void test_visibleStrSlice() {
  string s;

  s = {"abc\x1b[1mdef\x1b[21m"};

  pair<int, int> res;

  res = visibleStrSlice(s, 0, 2);
  ASSERT_EQ(0, res.first);
  ASSERT_EQ(1, res.second);

  res = visibleStrSlice(s, 0, 3);
  ASSERT_EQ(0, res.first);
  ASSERT_EQ(6, res.second);

  res = visibleStrSlice(s, 0, 5);
  ASSERT_EQ(0, res.first);
  ASSERT_EQ(8, res.second);

  res = visibleStrSlice(s, 0, 6);
  ASSERT_EQ(0, res.first);
  ASSERT_EQ(14, res.second);

  res = visibleStrSlice(s, 1, 1);
  ASSERT_EQ(1, res.first);
  ASSERT_EQ(1, res.second);

  res = visibleStrSlice(s, 1, 2);
  ASSERT_EQ(1, res.first);
  ASSERT_EQ(6, res.second);

  res = visibleStrSlice(s, 0, 4);
  ASSERT_EQ(0, res.first);
  ASSERT_EQ(7, res.second);

  res = visibleStrSlice(s, 2, 1);
  ASSERT_EQ(2, res.first);
  ASSERT_EQ(6, res.second);

  res = visibleStrSlice(s, 3, 2);
  ASSERT_EQ(3, res.first);
  ASSERT_EQ(8, res.second);

  res = visibleStrSlice(s, 3, 3);
  ASSERT_EQ(3, res.first);
  ASSERT_EQ(14, res.second);

  s = {"\x1b[1mdef\x1b[21m"};

  res = visibleStrSlice(s, 0, 2);
  ASSERT_EQ(0, res.first);
  ASSERT_EQ(5, res.second);
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

  test_parens();

  test_next_word_jump_location();
  test_prev_word_jump_location();

  test_visibleCharCount();
  test_visibleStrRightCut();

  test_visibleStrSlice();

  cout << endl;
}