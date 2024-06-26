#include "rope.h"

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
    cout << "\n\nFail!\nLine: " << lineNo << "\nExpected: <" << v1
         << ">\n  Actual: <" << v2 << ">\n\n";
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

/*
      /--\
     +    +
   /-\    /-\
   +  gh ij +
  / \      / \
  ab +     + op
    / \   / \
   cd ef kl mn
*/
unique_ptr<Rope> make_medium_branched() {
  unique_ptr<Rope> rope = make_unique<Rope>("abcdefghijklmnop");

  rope->split(8);
  rope->split(6);
  rope->split(10);
  rope->split(2);
  rope->split(14);
  rope->split(4);
  rope->split(12);

  ASSERT_EQ(
      "[0:1 ab][2:3 cd][4:5 ef][6:7 gh][8:9 ij][10:11 kl][12:13 mn][14:15 op]"s,
      rope->debug_to_string());

  return rope;
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

void test_node_at() {
  // [0:1 ab][2:3 cd][4:5 ef][6:7 gh][8:9 ij][10:11 kl][12:13 mn][14:15 op]
  auto rope = make_medium_branched();

  ASSERT_NOT_NULLPTR(rope->node_at(4));
  ASSERT_EQ("ef"s, rope->node_at(4)->to_string());
  ASSERT_EQ("ef"s, rope->node_at(5)->to_string());

  ASSERT_EQ("ij"s, rope->node_at(8)->to_string());
  ASSERT_EQ("ij"s, rope->node_at(9)->to_string());

  ASSERT_EQ("ab"s, rope->node_at(0)->to_string());
  ASSERT_EQ("op"s, rope->node_at(15)->to_string());

  ASSERT_NULLPTR(rope->node_at(16));
}

void test_next_new_line_at() {
  Rope r{"\n0123\nabcd\n0123\nabcd\n"};
  r.split(3);
  r.split(18);
  ASSERT_EQ("[0:2 \n01][3:17 23\nabcd\n0123\nab][18:20 cd\n]"s,
            r.debug_to_string());

  ASSERT_EQ(0, r.next_line_at(0));

  ASSERT_EQ(5, r.next_line_at(1));
  ASSERT_EQ(5, r.next_line_at(2));
  ASSERT_EQ(5, r.next_line_at(3));
  ASSERT_EQ(5, r.next_line_at(4));
  ASSERT_EQ(5, r.next_line_at(5));

  ASSERT_EQ(10, r.next_line_at(6));
  ASSERT_EQ(20, r.next_line_at(20));
}

void test_next_new_line_at_many_passes() {
  Rope r{"abcdefgh01234567\n"};
  r.split(4);
  r.split(2);
  r.split(6);
  r.split(14);
  r.split(12);
  r.split(10);
  ASSERT_EQ(
      "[0:1 ab][2:3 cd][4:5 ef][6:9 gh01][10:11 23][12:13 45][14:16 67\n]"s,
      r.debug_to_string());

  ASSERT_EQ(16, r.next_line_at(0));
}

void test_prev_new_line_at() {
  Rope r{"\n0123\nabcd\n0123\nabcd\n"};
  r.split(3);
  r.split(18);
  ASSERT_EQ("[0:2 \n01][3:17 23\nabcd\n0123\nab][18:20 cd\n]"s,
            r.debug_to_string());

  ASSERT_EQ(20, r.prev_line_at(20));

  ASSERT_EQ(15, r.prev_line_at(19));
  ASSERT_EQ(15, r.prev_line_at(17));
  ASSERT_EQ(15, r.prev_line_at(15));
}

void test_prev_new_line_at_many_passes() {
  Rope r{"\nabcdefgh01234567"};
  r.split(4);
  r.split(2);
  r.split(6);
  r.split(14);
  r.split(12);
  r.split(10);
  ASSERT_EQ(
      "[0:1 \na][2:3 bc][4:5 de][6:9 fgh0][10:11 12][12:13 34][14:16 567]"s,
      r.debug_to_string());

  ASSERT_EQ(0, r.prev_line_at(16));
}

void test_prev_and_next_new_line_at_without_match() {
  Rope r{"abcdefgh01234567"};
  r.split(4);
  r.split(2);
  r.split(6);
  r.split(14);
  r.split(12);
  r.split(10);
  ASSERT_EQ("[0:1 ab][2:3 cd][4:5 ef][6:9 gh01][10:11 23][12:13 45][14:15 67]"s,
            r.debug_to_string());

  ASSERT_EQ(-1, r.next_line_at(0));
  ASSERT_EQ(-1, r.prev_line_at(16));
}

void test_merge_up_subtree_left() {
  Rope r{"xabcdef"};
  r.split(1);
  r.split(2);
  r.split(5);
  ASSERT_EQ("[0:0 x][1:1 a][2:4 bcd][5:6 ef]"s, r.debug_to_string());

  r.remove(1);
  ASSERT_EQ("[0:0 x][1:3 bcd][4:5 ef]"s, r.debug_to_string());

  r.remove(0);
  ASSERT_EQ("[0:2 bcd][3:4 ef]"s, r.debug_to_string());
}

void test_merge_up_subtree_right() {
  Rope r{"abcdefg"};
  r.split(6);
  r.split(5);
  r.split(2);
  ASSERT_EQ("[0:1 ab][2:4 cde][5:5 f][6:6 g]"s, r.debug_to_string());

  r.remove(5);
  ASSERT_EQ("[0:1 ab][2:4 cde][5:5 g]"s, r.debug_to_string());

  r.remove(5);
  ASSERT_EQ("[0:1 ab][2:4 cde]"s, r.debug_to_string());
}

void test_remove_range() {
  Rope r{"abcdef"};
  r.remove_range(2, 3);
  ASSERT_EQ("[0:3 abef]"s, r.debug_to_string());

  ASSERT_EQ((int)RopeRemoveResult::RangeError, (int)r.remove_range(0, 4));

  r.remove_range(0, 3);
  ASSERT_EQ("[0:-]"s, r.debug_to_string());
}

void test_remove_range_across_nodes() {
  // [0:1 ab][2:3 cd][4:5 ef][6:7 gh][8:9 ij][10:11 kl][12:13 mn][14:15 op]
  unique_ptr<Rope> r{make_medium_branched()};

  r->remove_range(3, 12);
  ASSERT_EQ("[0:1 ab][2:2 c][3:3 n][4:5 op]"s, r->debug_to_string());
}

void test_new_line_count() {
  Rope r{"\nhello\nbello\nfrom\nanother\nworld\n"};
  r.split(16);
  r.split(8);
  r.split(24);
  r.split(28);

  ASSERT_EQ(
      "[0:7 \nhello\nb][8:15 ello\nfro][16:23 m\nanothe][24:27 r\nwo][28:31 rld\n]"s,
      r.debug_to_string());

  ASSERT_EQ((size_t)6, RopeUtil::new_line_count(r));

  r.remove(25);
  ASSERT_EQ((size_t)5, RopeUtil::new_line_count(r));

  ASSERT_EQ(
      "[0:7 \nhello\nb][8:15 ello\nfro][16:23 m\nanothe][24:26 rwo][27:30 rld\n]"s,
      r.debug_to_string());
  r.remove_range(24, 26);
  ASSERT_EQ("[0:7 \nhello\nb][8:15 ello\nfro][16:23 m\nanothe][24:27 rld\n]"s,
            r.debug_to_string());
  ASSERT_EQ((size_t)5, RopeUtil::new_line_count(r));

  r.insert(20, "\n");
  ASSERT_EQ((size_t)6, RopeUtil::new_line_count(r));
}

void test_new_line_at() {
  Rope r{"\nhello\nbello\nfrom\nanother\nworld\n"};
  r.split(16);
  r.split(8);
  r.split(24);
  r.split(28);

  ASSERT_EQ(0, r.nth_new_line_at(0));
  ASSERT_EQ(6, r.nth_new_line_at(1));
  ASSERT_EQ(12, r.nth_new_line_at(2));
  ASSERT_EQ(17, r.nth_new_line_at(3));
  ASSERT_EQ(25, r.nth_new_line_at(4));
  ASSERT_EQ(31, r.nth_new_line_at(5));
}

void test_substr() {
  Rope r{"abcdef"};

  ASSERT_EQ("ab"s, r.substr(0, 2));
  ASSERT_EQ("cd"s, r.substr(2, 2));
  ASSERT_EQ("ef"s, r.substr(4, 2));
  ASSERT_EQ("abcdef"s, r.substr(0, 6));
  ASSERT_EQ("def"s, r.substr(3, 10));
}

void test_substr_multinode() {
  // [0:1 ab][2:3 cd][4:5 ef][6:7 gh][8:9 ij][10:11 kl][12:13 mn][14:15 op]
  auto r = make_medium_branched();

  ASSERT_EQ("abcdefghijklmnop"s, r->substr(0, 16));
  ASSERT_EQ("abcdefghijklmnop"s, r->substr(0, 20));
  ASSERT_EQ("bcdefghijklmno"s, r->substr(1, 14));
}

void test_nth_line() {
  Rope r{"\nhello\nbello\nfrom\n\nanother\nworld\n"};
  r.split(16);
  r.split(8);
  r.split(24);
  r.split(28);

  ASSERT_EQ(""s, RopeUtil::nth_line(r, 0));
  ASSERT_EQ("hello"s, RopeUtil::nth_line(r, 1));
  ASSERT_EQ("bello"s, RopeUtil::nth_line(r, 2));
  ASSERT_EQ("from"s, RopeUtil::nth_line(r, 3));
  ASSERT_EQ(""s, RopeUtil::nth_line(r, 4));
  ASSERT_EQ("another"s, RopeUtil::nth_line(r, 5));
  ASSERT_EQ("world"s, RopeUtil::nth_line(r, 6));
  ASSERT_EQ(""s, RopeUtil::nth_line(r, 7));
  ASSERT_EQ(""s, RopeUtil::nth_line(r, 10));
}

void test_siblings() {
  // [0:1 ab][2:3 cd][4:5 ef][6:7 gh][8:9 ij][10:11 kl][12:13 mn][14:15 op]
  auto r = make_medium_branched();

  r->remove_range(10, 11);
  r->insert(3, "hello bello strange person");
  r->insert(4, "lots of text");

  r->insert(10, "another bit of text haha");
  r->insert(11, "and even more data coming");

  r->remove_range(4, 20);

  ASSERT_EQ(
      "[0:1 ab][2:3 ch][4:20 ore data comingno][21:27 ther bi][28:42 t of text hahaf][43:52  textello ][53:59 bello s][60:73 trange persond][74:75 ef][76:77 gh][78:79 ij][80:81 mn][82:83 op]"s,
      r->debug_to_string());

  Rope *node = r->node_at(0);
  ASSERT_EQ("ab"s, node->to_string());
  ASSERT_NULLPTR(node->leafNode.left);
  ASSERT_NOT_NULLPTR(node->leafNode.right);

  node = node->leafNode.right;
  ASSERT_EQ("ch"s, node->to_string());
  ASSERT_EQ("ab"s, node->leafNode.left->to_string());

  for (int i = 0; i < 11; i++) node = node->leafNode.right;
  ASSERT_EQ("op"s, node->to_string());
  ASSERT_NULLPTR(node->leafNode.right);

  for (int i = 0; i < 12; i++) node = node->leafNode.left;
  ASSERT_EQ("ab"s, node->to_string());
}

void test_iterator() {
  auto rope = make_medium_branched();
  string expected{"abcdefghijklmnop"};

  int i = 0;
  for (auto c : *rope) ASSERT_EQ(expected[i++], c);
}

void test_find_str() {
  // [0:1 ab][2:3 cd][4:5 ef][6:7 gh][8:9 ij][10:11 kl][12:13 mn][14:15 op]
  auto rope = make_medium_branched();

  ASSERT_EQ(0, RopeUtil::find_str(*(rope.get()), "a", 0));
  ASSERT_EQ(0, RopeUtil::find_str(*(rope.get()), "ab", 0));
  ASSERT_EQ(0, RopeUtil::find_str(*(rope.get()), "abc", 0));
  ASSERT_EQ(0, RopeUtil::find_str(*(rope.get()), "abcdedg", 0));

  ASSERT_EQ(5, RopeUtil::find_str(*(rope.get()), "fghi", 0));
}

// void test_find_str_from_mid() { Rope r{"abc012abc345abc678"};
//   r.split(); }

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

  test_node_at();

  test_next_new_line_at();
  test_next_new_line_at_many_passes();
  test_prev_new_line_at();
  test_prev_new_line_at_many_passes();
  test_prev_and_next_new_line_at_without_match();

  test_merge_up_subtree_left();
  test_merge_up_subtree_right();

  test_remove_range();
  test_remove_range_across_nodes();

  test_new_line_count();
  test_new_line_at();
  test_nth_line();

  test_substr();
  test_substr_multinode();

  test_siblings();

  test_iterator();

  printf("\nCompleted\n");

  return EXIT_SUCCESS;
}
