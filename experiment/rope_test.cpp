#include "rope.h"

#include <unistd.h>

#include <cstdlib>
#include <string>

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

void test_default() {
  Rope r{"hello world"};
  ASSERT_EQ("hello world"s, r.to_string());
}

void test_split() {
  Rope r1{"abcd"};
  ASSERT_EQ("[0:3 abcd]"s, r1.debug_to_string());

  ASSERT_EQ((int)RopeSplitResult::Success, (int)r1.split(2));
  ASSERT_EQ("[0:1 ab][2:3 cd]"s, r1.debug_to_string());

  ASSERT_EQ((int)RopeSplitResult::RangeError, (int)r1.split(10));

  ASSERT_EQ((int)RopeSplitResult::EmptySplitError, (int)r1.split(0));
  ASSERT_EQ((int)RopeSplitResult::EmptySplitError, (int)r1.split(4));

  ASSERT_EQ((int)RopeSplitResult::Success, (int)r1.split(1));
  ASSERT_EQ("[0:0 a][1:1 b][2:3 cd]"s, r1.debug_to_string());
}

void test_insert() {
  Rope r1{"abcd"};

  ASSERT_EQ(false, r1.insert(5, "y"));

  r1.insert(0, "x");
  ASSERT_EQ("xabcd"s, r1.to_string());

  r1.insert(5, "y");
  ASSERT_EQ("xabcdy"s, r1.to_string());
}

void test_insert_with_splits() {
  Rope r{"abcde01234fghij56789"};

  ASSERT_EQ((int)RopeSplitResult::Success, (int)r.split(10));
  ASSERT_EQ((int)RopeSplitResult::Success, (int)r.split(5));
  ASSERT_EQ((int)RopeSplitResult::Success, (int)r.split(15));
  ASSERT_EQ((int)RopeSplitResult::Success, (int)r.split(8));

  ASSERT_EQ("[0:4 abcde][5:7 012][8:9 34][10:14 fghij][15:19 56789]"s,
            r.debug_to_string());

  ASSERT_EQ(true, r.insert(6, "x"));
  ASSERT_EQ("[0:4 abcde][5:8 0x12][9:10 34][11:15 fghij][16:20 56789]"s,
            r.debug_to_string());

  ASSERT_EQ(true, r.insert(5, "y"));
  ASSERT_EQ("[0:4 abcde][5:9 y0x12][10:11 34][12:16 fghij][17:21 56789]"s,
            r.debug_to_string());
}

void test_insert_with_auto_split() {
  Rope r{make_shared<RopeConfig>(4), "abcde01234fghij56789"};
  ASSERT_EQ(true, r.insert(2, "x"));

  ASSERT_EQ("[0:1 ab][2:5 xcde][6:10 01234][11:20 fghij56789]"s,
            r.debug_to_string());
}

void test_insert_string() {
  Rope r;
  r.insert(0, "herld");
  r.insert(2, "llo wo");

  ASSERT_EQ("hello world"s, r.to_string());
}

void test_remove() {
  Rope r{"abcd"};

  r.remove(0);
  ASSERT_EQ("[0:2 bcd]"s, r.debug_to_string());

  r.remove(2);
  ASSERT_EQ("[0:1 bc]"s, r.debug_to_string());
}

void test_remove_with_split() {
  Rope r{"abcde01234fghij56789"};

  ASSERT_EQ((int)RopeSplitResult::Success, (int)r.split(10));
  ASSERT_EQ((int)RopeSplitResult::Success, (int)r.split(5));
  ASSERT_EQ((int)RopeSplitResult::Success, (int)r.split(15));
  ASSERT_EQ((int)RopeSplitResult::Success, (int)r.split(8));

  ASSERT_EQ("[0:4 abcde][5:7 012][8:9 34][10:14 fghij][15:19 56789]"s,
            r.debug_to_string());

  ASSERT_EQ((int)RopeRemoveResult::Success, (int)r.remove(2));
  ASSERT_EQ("[0:3 abde][4:6 012][7:8 34][9:13 fghij][14:18 56789]"s,
            r.debug_to_string());
}

void test_remove_with_empty_node() {
  Rope r{"abcd"};
  r.split(1);

  ASSERT_EQ("[0:0 a][1:3 bcd]"s, r.debug_to_string());
  r.remove(0);
  ASSERT_EQ("[0:2 bcd]"s, r.debug_to_string());
}

void test_empty() {
  Rope r;
  ASSERT_EQ((size_t)0, r.size);
  ASSERT_EQ(""s, r.to_string());

  ASSERT_EQ((int)RopeRemoveResult::RangeError, (int)r.remove(0));
  ASSERT_EQ(true, r.insert(0, "x"));
  ASSERT_EQ(true, r.insert(0, "y"));

  ASSERT_EQ("[0:1 yx]"s, r.debug_to_string());
}

void test_empty_from_non_empty() {
  Rope r{"ab"};

  r.remove(1);
  ASSERT_EQ("[0:0 a]"s, r.debug_to_string());

  ASSERT_EQ((int)RopeRemoveResult::NeedMergeUp, (int)r.remove(0));
  ASSERT_EQ(""s, r.to_string());

  ASSERT_EQ(true, r.empty());
}

void test_parent() {
  Rope r{"abcdef"};
  r.split(2);
  r.split(4);
  ASSERT_EQ("[0:1 ab][2:3 cd][4:5 ef]"s, r.debug_to_string());

  Rope *part1 = r.intermediateNode.lhs.get();
  Rope *part2 = r.intermediateNode.rhs.get();

  ASSERT_EQ(true, part1->is_left_child());
  ASSERT_EQ(false, part1->is_right_child());
  ASSERT_EQ(false, part2->is_left_child());
  ASSERT_EQ(true, part2->is_right_child());

  ASSERT_EQ(false, r.is_left_child());
  ASSERT_EQ(false, r.is_right_child());
}

void test_adjacent() {
  Rope r{"abcd"};
  r.split(2);
  r.split(1);
  r.split(3);
  ASSERT_EQ("[0:0 a][1:1 b][2:2 c][3:3 d]"s, r.debug_to_string());

  Rope *c = r.intermediateNode.rhs->intermediateNode.lhs.get();
  ASSERT_EQ("c"s, c->to_string());

  Rope *prev_c = c->prev();
  ASSERT_EQ("b"s, prev_c->to_string());

  Rope *next_c = c->next();
  ASSERT_EQ("d"s, next_c->to_string());

  ASSERT_EQ((Rope *)nullptr, next_c->next());
}

int main() {
  test_default();
  test_split();

  test_insert();
  test_insert_with_splits();
  test_insert_with_auto_split();
  test_insert_string();

  test_remove();
  test_remove_with_split();
  test_remove_with_empty_node();

  test_empty();
  test_empty_from_non_empty();

  test_parent();

  test_adjacent();

  return EXIT_SUCCESS;
}
