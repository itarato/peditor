#include "lines.h"

#include <unistd.h>

#include <cstdlib>
#include <string>
#include <utility>

#define ASSERT_EQ(v1, v2) assert_eq(v1, v2, __LINE__)
#define ASSERT_NOT_NULLPTR(ptr) assert_not_nullptr(ptr, __LINE__)
#define ASSERT_NULLPTR(ptr) assert_nullptr(ptr, __LINE__)

template <typename T>
void assert_eq(T v1, T v2, int lineNo) {
  if (v1 == v2) {
    cout << ".";
  } else {
    cout << "\n\nFail!\nLine: " << lineNo << "\nExpected: <" << v1 << ">\n  Actual: <" << v2 << ">\n\n";
  }
}

template <typename T>
void assert_not_nullptr(T ptr, int lineNo) {
  if (ptr != nullptr) {
    cout << ".";
  } else {
    cout << "\n\nFail!\nLine: " << lineNo << "\nGot: NULLPTR\n\n";
  }
}

template <typename T>
void assert_nullptr(T ptr, int lineNo) {
  if (ptr == nullptr) {
    cout << ".";
  } else {
    cout << "\n\nFail!\nLine: " << lineNo << "\nGot: not a NULLPTR\n\n";
  }
}

void test_basic_empty() {
  Lines l{};
  ASSERT_EQ(""s, l.to_string());
  ASSERT_EQ("0[]"s, l.debug_to_string());
}

void test_basic_leaf() {
  Lines l{{"hello", "world", "!"}};
  ASSERT_EQ("hello\nworld\n!\n"s, l.to_string());
  ASSERT_EQ("0:2[hello][world][!]"s, l.debug_to_string());
}

void test_split() {
  Lines l{{"hello", "world", "!"}};

  l.split(1);
  ASSERT_EQ("(0:0[hello])(1:2[world][!])"s, l.debug_to_string());

  l.split(2);
  ASSERT_EQ("(0:0[hello])((1:1[world])(2:2[!]))"s, l.debug_to_string());

  ASSERT_EQ(false, l.split(0));
  ASSERT_EQ(false, l.split(1));
  ASSERT_EQ(false, l.split(2));
  ASSERT_EQ(false, l.split(3));
  ASSERT_EQ(false, l.split(10));

  ASSERT_EQ("hello\nworld\n!\n"s, l.to_string());
}

void test_split_deep() {
  Lines l{{"0", "1", "2", "3", "4", "5", "6", "7"}};

  ASSERT_EQ(true, l.split(4));
  ASSERT_EQ(true, l.split(6));
  ASSERT_EQ(true, l.split(2));
  ASSERT_EQ(true, l.split(1));
  ASSERT_EQ(true, l.split(5));

  ASSERT_EQ("(((0:0[0])(1:1[1]))(2:3[2][3]))(((4:4[4])(5:5[5]))(6:7[6][7]))"s, l.debug_to_string());
}

void test_insert() {
  Lines l{{"0", "1", "2", "3", "4", "5", "6", "7"}};

  l.insert(1, 0, "(pre-1)");
  ASSERT_EQ("0:7[0][(pre-1)1][2][3][4][5][6][7]"s, l.debug_to_string());

  l.insert(1, 7, "++");
  ASSERT_EQ("0:7[0][(pre-1)++1][2][3][4][5][6][7]"s, l.debug_to_string());

  l.insert(1, 10, "(post-1)");
  ASSERT_EQ("0:7[0][(pre-1)++1(post-1)][2][3][4][5][6][7]"s, l.debug_to_string());

  l.insert(7, 1, "(end)");
  ASSERT_EQ("0:7[0][(pre-1)++1(post-1)][2][3][4][5][6][7(end)]"s, l.debug_to_string());

  ASSERT_EQ(false, l.insert(0, 2, ""));
  ASSERT_EQ(false, l.insert(10, 0, ""));

  ASSERT_EQ("0:7[0][(pre-1)++1(post-1)][2][3][4][5][6][7(end)]"s, l.debug_to_string());
}

void test_insert_new_lines() {
  Lines l{{"helloworld"}};

  l.insert(0, 5, "\n");
  ASSERT_EQ("0:1[hello][world]"s, l.debug_to_string());

  l.insert(0, 0, "abc\ndef\n");
  ASSERT_EQ("0:3[abc][def][hello][world]"s, l.debug_to_string());

  l.insert(3, 5, "\nhi\n");
  ASSERT_EQ("0:5[abc][def][hello][world][hi][]"s, l.debug_to_string());
}

void test_insert_with_split() {
  Lines l{make_shared<LinesConfig>(2), {"0", "1", "2", "3", "4", "5", "6", "7"}};

  ASSERT_EQ("0:7[0][1][2][3][4][5][6][7]"s, l.debug_to_string());

  l.insert(2, 1, "x");
  ASSERT_EQ("((0:1[0][1])(2:3[2x][3]))(4:7[4][5][6][7])"s, l.debug_to_string());
}

void test_insert_empty_new_line() {
  Lines l{{"hello"}};

  l.insert(0, 5, "\n");
  ASSERT_EQ("0:1[hello][]"s, l.debug_to_string());
}

void test_backspace_basic() {
  Lines l{{"abcd", "efgh", "ijkl"}};

  ASSERT_EQ(true, l.backspace(0, 1));
  ASSERT_EQ(true, l.backspace(2, 4));
  ASSERT_EQ(true, l.backspace(1, 2));

  ASSERT_EQ("0:2[bcd][egh][ijk]"s, l.debug_to_string());

  ASSERT_EQ(false, l.backspace(20, 1));
  ASSERT_EQ(false, l.backspace(1, 20));
  ASSERT_EQ("0:2[bcd][egh][ijk]"s, l.debug_to_string());
}

void test_backspace_merge_in_node_lines() {
  Lines l{{"aa", "bb", "cc"}};

  ASSERT_EQ(true, l.backspace(1, 0));
  ASSERT_EQ("0:1[aabb][cc]"s, l.debug_to_string());

  ASSERT_EQ(true, l.backspace(1, 0));
  ASSERT_EQ("0:0[aabbcc]"s, l.debug_to_string());
}

void test_backspace_merge_between_subtrees() {
  Lines l{{"aa", "bb", "cc", "dd"}};

  l.split(2);
  l.split(1);
  l.split(3);
  ASSERT_EQ("((0:0[aa])(1:1[bb]))((2:2[cc])(3:3[dd]))"s, l.debug_to_string());

  ASSERT_EQ(true, l.backspace(2, 0));
  ASSERT_EQ("((0:0[aa])(1:1[bbcc]))(2:2[dd])"s, l.debug_to_string());

  ASSERT_EQ(true, l.backspace(2, 0));
  ASSERT_EQ("(0:0[aa])(1:1[bbccdd])"s, l.debug_to_string());

  ASSERT_EQ(true, l.backspace(1, 0));
  ASSERT_EQ("0:0[aabbccdd]"s, l.debug_to_string());
}

int main() {
  test_basic_empty();
  test_basic_leaf();

  test_split();
  test_split_deep();

  test_insert();
  test_insert_new_lines();
  test_insert_with_split();
  test_insert_empty_new_line();

  test_backspace_basic();
  test_backspace_merge_in_node_lines();
  test_backspace_merge_between_subtrees();

  printf("\nCompleted\n");

  return EXIT_SUCCESS;
}
