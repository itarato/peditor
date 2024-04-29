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

  ASSERT_EQ(false, r1.insert(5, 'y'));

  r1.insert(0, 'x');
  ASSERT_EQ("xabcd"s, r1.to_string());

  r1.insert(5, 'y');
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

  // ASSERT_EQ(true, r.insert())
}

int main() {
  test_default();
  test_split();
  test_insert();
  test_insert_with_splits();

  return EXIT_SUCCESS;
}