#pragma once

#include <assert.h>
#include <unistd.h>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <memory>
#include <new>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace std;

/**
 * Needs:
 * - navigation by line: up, down
 * - edit line: delete line
 * - insert/remove section (from..to)
 */

/**
 * - [done] line count (how many lines)
 * - [done] get Nth line
 * - [done] get Nth line length
 * - search for substring
 * - jump to next/pref substring match
 * - clear all
 */

#define LINES_UNIT_BREAK_THRESHOLD 8
#define LEFT true
#define RIGHT !LEFT
#define LINES_MASK_FROM_AT_START 0b1
#define LINES_MASK_TO_AT_END 0b10
#define LINES_IT_FWD 1
#define LINES_IT_BWD -1

#ifdef DEBUG
#define LOG_RETURN(val, msg)           \
  {                                    \
    printf("%d: %s\n", __LINE__, msg); \
    return val;                        \
  }
#else
#define LOG_RETURN(val, msg) return val;
#endif

enum class LinesNodeType {
  Intermediate,
  Leaf,
};

enum class LinesRemoveResult {
  Success,
  NeedMergeUp,
  RangeError,
};

struct Lines;

struct LinesIntermediateNode {
  unique_ptr<Lines> lhs;
  unique_ptr<Lines> rhs;

  LinesIntermediateNode(unique_ptr<Lines> &&lhs, unique_ptr<Lines> &&rhs)
      : lhs(std::forward<unique_ptr<Lines>>(lhs)), rhs(std::forward<unique_ptr<Lines>>(rhs)) {
  }

  LinesIntermediateNode(LinesIntermediateNode &other) = delete;
  LinesIntermediateNode(LinesIntermediateNode &&other) = delete;

  unique_ptr<Lines> &child(bool is_left) {
    return is_left ? lhs : rhs;
  }

  bool which_child(const Lines *child) const {
    if (lhs.get() == child) return LEFT;
    if (rhs.get() == child) return RIGHT;

    printf("ERROR: unknown child\n");
    exit(EXIT_FAILURE);
  }
};

struct LinesLeaf {
  vector<string> lines{};
  Lines *left{nullptr};
  Lines *right{nullptr};

  LinesLeaf() {
  }
  LinesLeaf(vector<string> &&lines) : lines(std::forward<vector<string>>(lines)) {
  }
  LinesLeaf(LinesLeaf &other) = delete;
  LinesLeaf(LinesLeaf &&other) = delete;

  bool is_one_empty_line() const {
    return lines.size() == 1 && lines[0].size() == 0;
  }

  void debug_dump() const {
    printf("Leaf: size=%lu left=%p right=%p", lines.size(), (void *)left, (void *)right);
    for (const auto &e : lines) printf(" %s", e.c_str());
    printf("\n");
  }
};

struct LinesConfig {
  size_t unit_break_threshold;

  LinesConfig(size_t unit_break_threshold) : unit_break_threshold(unit_break_threshold) {
  }
};

namespace LinesUtil {
bool has_new_line(string const &s) {
  auto it = find(s.begin(), s.end(), '\n');
  return it != s.end();
}

template <typename F>
void split_lines(const string &s, const F fn) {
  string current;
  for (const auto &c : s) {
    if (c == '\n') {
      fn(current);
      current.clear();
    } else {
      current.push_back(c);
    }
  }
  fn(current);
}
};  // namespace LinesUtil

struct Lines {
  size_t line_start;
  size_t line_count;
  LinesNodeType type{LinesNodeType::Intermediate};
  shared_ptr<LinesConfig> config;
  Lines *parent;

  union {
    LinesIntermediateNode intermediateNode;
    LinesLeaf leafNode;
  };

  Lines()
      : line_start(0),
        line_count(0),
        type(LinesNodeType::Leaf),
        config(std::make_shared<LinesConfig>(LINES_UNIT_BREAK_THRESHOLD)),
        parent(nullptr) {
    new (&leafNode) LinesLeaf{};
  }

  Lines(vector<string> &&lines)
      : line_start(0),
        line_count(lines.size()),
        type(LinesNodeType::Leaf),
        config(std::make_shared<LinesConfig>(LINES_UNIT_BREAK_THRESHOLD)),
        parent(nullptr) {
    new (&leafNode) LinesLeaf{std::forward<vector<string>>(lines)};
  }

  Lines(shared_ptr<LinesConfig> config, vector<string> &&lines)
      : line_start(0), line_count(lines.size()), type(LinesNodeType::Leaf), config(config), parent(nullptr) {
    new (&leafNode) LinesLeaf{std::forward<vector<string>>(lines)};
  }

  Lines(shared_ptr<LinesConfig> config, size_t start, Lines *parent, vector<string> &&lines)
      : line_start(start), line_count(lines.size()), type(LinesNodeType::Leaf), config(config), parent(parent) {
    new (&leafNode) LinesLeaf{std::forward<vector<string>>(lines)};
  }

  Lines(Lines &other) = delete;
  Lines(Lines &&other) = delete;

  ~Lines() {
    if (type == LinesNodeType::Intermediate) {
      intermediateNode.LinesIntermediateNode::~LinesIntermediateNode();
    } else {
      leafNode.LinesLeaf::~LinesLeaf();
    }
  }

  /**
   * OUTPUT
   */

  string to_string() const {
    if (type == LinesNodeType::Intermediate) {
      return intermediateNode.lhs->to_string() + intermediateNode.rhs->to_string();
    } else {
      stringstream ss;
      for (auto &line : leafNode.lines) {
        ss << line;
        ss << endl;
      }
      return ss.str();
    }
  }

  string debug_to_string() const {
    stringstream ss;

    if (type == LinesNodeType::Intermediate) {
      ss << "(" << intermediateNode.lhs->debug_to_string() << ")(" << intermediateNode.rhs->debug_to_string() << ")";
    } else {
      if (empty()) {
        ss << std::to_string(line_start) << "-";
      } else {
        ss << std::to_string(line_start) << ":" << std::to_string(line_end());
        for (auto &line : leafNode.lines) ss << "[" << line << "]";
      }
    }

    return ss.str();
  }

  string &operator[](size_t line_idx) const {
    Lines *node = node_at(line_idx);
    assert(node);

    return node->leafNode.lines[line_idx - node->line_start];
  }

  /**
   * OPERATIONS
   */

  void clear() {
    if (type == LinesNodeType::Intermediate) {
      intermediateNode.~LinesIntermediateNode();
      type = LinesNodeType::Leaf;
    } else {
      leafNode.~LinesLeaf();
    }

    new (&leafNode) LinesLeaf();
    line_count = 0;
  }

  bool split(size_t line_idx) {
    if (type == LinesNodeType::Intermediate) {
      if (intermediateNode.rhs->line_start <= line_idx) {
        return intermediateNode.rhs->split(line_idx);
      } else {
        return intermediateNode.lhs->split(line_idx);
      }
    } else {
      if (!in_range(line_idx)) LOG_RETURN(false, "ERR: split not in range");

      if (line_idx == line_start || line_end() + 1 == line_idx) return false;

      unique_ptr<Lines> lhs =
          make_unique<Lines>(config, line_start, this,
                             vector<string>{leafNode.lines.begin(), leafNode.lines.begin() + (line_idx - line_start)});
      unique_ptr<Lines> rhs =
          make_unique<Lines>(config, line_idx, this,
                             vector<string>{leafNode.lines.begin() + (line_idx - line_start), leafNode.lines.end()});

      // Set sibling pointers.
      Lines *old_left_sib = leafNode.left;
      Lines *old_right_sib = leafNode.right;
      lhs->leafNode.right = rhs.get();
      lhs->leafNode.left = old_left_sib;
      rhs->leafNode.left = lhs.get();
      rhs->leafNode.right = old_right_sib;
      if (old_left_sib) old_left_sib->leafNode.right = lhs.get();
      if (old_right_sib) old_right_sib->leafNode.left = rhs.get();

      leafNode.LinesLeaf::~LinesLeaf();
      type = LinesNodeType::Intermediate;
      new (&intermediateNode)
          LinesIntermediateNode(std::forward<unique_ptr<Lines>>(lhs), std::forward<unique_ptr<Lines>>(rhs));

      return true;
    }
  }

  // void emplace_back(string s) {
  //   Lines *node = rightmost();
  //   rightmost();
  // }

  bool insert(size_t line_idx, size_t pos, string &&snippet) {
    if (!in_range_lines(line_idx)) LOG_RETURN(false, "ERR: insert not in range");

    if (type == LinesNodeType::Intermediate) {
      if (intermediateNode.rhs->line_start <= line_idx) {
        return intermediateNode.rhs->insert(line_idx, pos, std::forward<string>(snippet));
      } else {
        return intermediateNode.lhs->insert(line_idx, pos, std::forward<string>(snippet));
      }
    } else {
      if (split_if_too_large()) return insert(line_idx, pos, std::forward<string>(snippet));

      size_t line_relative_idx = line_idx - line_start;

      // Line pos out of bounds.
      if (leafNode.lines[line_relative_idx].size() < pos) return false;

      leafNode.lines[line_relative_idx].insert(pos, snippet);

      // Handle new inserted new lines.
      if (LinesUtil::has_new_line(leafNode.lines[line_relative_idx])) {
        size_t old_line_count = leafNode.lines.size();

        string line_to_cut{leafNode.lines[line_relative_idx]};
        auto it = leafNode.lines.begin();
        advance(it, line_relative_idx);
        it = leafNode.lines.erase(it);

        LinesUtil::split_lines(line_to_cut, [&](const string &new_line) {
          it = leafNode.lines.insert(it, new_line);
          it++;
        });

        size_t line_count_diff = leafNode.lines.size() - old_line_count;
        adjust_line_count_and_line_start_up_and_right(line_count_diff, false);
      }

      return true;
    }
  }

  bool split_if_too_large() {
    assert(type == LinesNodeType::Leaf);
    if (leafNode.lines.size() <= config->unit_break_threshold) return false;

    size_t mid_line_idx = line_start + line_count / 2;
    split(mid_line_idx);

    return true;
  }

  void adjust_line_count_and_line_start_up_and_right(int diff, bool is_called_by_left_child) {
    line_count += diff;

    if (type == LinesNodeType::Intermediate && is_called_by_left_child) {
      intermediateNode.rhs->adjust_line_start_down(diff);
    }

    if (parent) {
      parent->adjust_line_count_and_line_start_up_and_right(diff, parent->intermediateNode.which_child(this) == LEFT);
    }
  }

  void adjust_line_start_down(int diff) {
    line_start += diff;

    if (type == LinesNodeType::Intermediate) {
      intermediateNode.lhs->adjust_line_start_down(diff);
      intermediateNode.rhs->adjust_line_start_down(diff);
    }
  }

  bool backspace(size_t line_idx, size_t pos) {
    if (!in_range_lines(line_idx)) LOG_RETURN(false, "ERR: backspace not in range");

    if (type == LinesNodeType::Intermediate) {
      Lines *leaf = node_at(line_idx);
      if (!leaf) LOG_RETURN(false, "ERR: backspace missing leaf");
      return leaf->backspace(line_idx, pos);
    }

    assert(type == LinesNodeType::Leaf);
    size_t relative_line_pos = line_idx - line_start;
    if (pos > leafNode.lines[relative_line_pos].size()) LOG_RETURN(false, "ERR: backspace pos out of range");

    if (pos > 0) {
      leafNode.lines[relative_line_pos].erase(pos - 1, 1);
      return true;
    } else {
      if (relative_line_pos > 0) {
        leafNode.lines[relative_line_pos - 1].append(leafNode.lines[relative_line_pos]);
        auto it = leafNode.lines.begin();
        advance(it, relative_line_pos);
        leafNode.lines.erase(it);
        adjust_line_count_and_line_start_up_and_right(-1, false);
        return true;
      } else {
        if (!leafNode.left) return false;

        leafNode.left->leafNode.lines.back().append(leafNode.lines.front());
        auto it = leafNode.lines.begin();
        leafNode.lines.erase(it);
        adjust_line_count_and_line_start_up_and_right(-1, false);

        if (line_count == 0 && parent) parent->merge_up(this);
        return true;
      }
    }
  }

  void remove_range_from_single_leaf(size_t from_line, size_t from_pos, size_t to_line, size_t to_pos) {
    size_t lhs_line_idx = from_line >= line_start ? (from_line - line_start) : 0;
    size_t lhs_from_pos = from_line >= line_start ? from_pos : 0;
    size_t rhs_line_idx = min(to_line - line_start, line_count - 1);
    size_t rhs_to_pos = to_line >= (line_start + line_count) ? leafNode.lines[rhs_line_idx].size() - 1 : to_pos;

    // Delete from only one line.
    if (lhs_line_idx == rhs_line_idx) {
      leafNode.lines[lhs_line_idx].erase(lhs_from_pos, rhs_to_pos - lhs_from_pos + 1);
    } else {  // Delete from multiple lines.
      // Erase left and right ends.
      leafNode.lines[lhs_line_idx].erase(lhs_from_pos);
      leafNode.lines[rhs_line_idx].erase(0, rhs_to_pos + 1);
      // Merge right end into left end.
      leafNode.lines[lhs_line_idx].append(leafNode.lines[rhs_line_idx]);
      // Erase mid section.
      int line_deletions = rhs_line_idx - lhs_line_idx;
      auto it_start = leafNode.lines.begin();
      advance(it_start, lhs_line_idx + 1);
      auto it_end = it_start + (line_deletions);
      leafNode.lines.erase(it_start, it_end);

      if (line_deletions > 0) adjust_line_count_and_line_start_up_and_right(-line_deletions, false);
    }
  }

  void merge_up(Lines *empty_child) {
    bool empty_node = intermediateNode.which_child(empty_child);

    if (intermediateNode.child(!empty_node)->type == LinesNodeType::Intermediate) {
      // Adjust sibling pointers.
      Lines *old_left = intermediateNode.child(empty_node)->leafNode.left;
      Lines *old_right = intermediateNode.child(empty_node)->leafNode.right;
      if (old_left) old_left->leafNode.right = old_right;
      if (old_right) old_right->leafNode.left = old_left;

      intermediateNode.child(empty_node).reset(nullptr);
      intermediateNode.child(empty_node).swap(intermediateNode.child(!empty_node)->intermediateNode.child(empty_node));

      auto grandchild = intermediateNode.child(!empty_node)->intermediateNode.child(!empty_node).release();
      intermediateNode.child(!empty_node).reset(grandchild);

      intermediateNode.lhs->parent = this;
      intermediateNode.rhs->parent = this;
    } else {
      assert(type == LinesNodeType::Intermediate);

      vector<string> old_lines = intermediateNode.child(!empty_node)->leafNode.lines;
      Lines *old_left_sib = intermediateNode.lhs->leafNode.left;
      Lines *old_right_sib = intermediateNode.rhs->leafNode.right;

      // Kill old self subtype.
      intermediateNode.LinesIntermediateNode::~LinesIntermediateNode();

      type = LinesNodeType::Leaf;
      new (&leafNode) LinesLeaf(std::forward<vector<string>>(old_lines));

      leafNode.left = old_left_sib;
      leafNode.right = old_right_sib;
      if (old_left_sib) old_left_sib->leafNode.right = this;
      if (old_right_sib) old_right_sib->leafNode.left = this;
    }
  }

  /**
   * BOUNDS
   */

  bool empty() const {
    return line_count == 0;
  }

  size_t line_end() const {
    // Must check emptiness before calling this.
    assert(!empty());
    return line_start + line_count - 1;
  }

  bool in_range(size_t at) const {
    return (empty() && at == line_start) || (line_start <= at && at <= line_end() + 1);
  }

  bool in_range_lines(size_t at) const {
    return !empty() && line_start <= at && at <= line_end();
  }

  /**
   * NAVIGATION
   */

  bool is_right_child() const {
    if (!parent) return false;
    return parent->intermediateNode.rhs.get() == this;
  }

  bool is_left_child() const {
    if (!parent) return false;
    return parent->intermediateNode.lhs.get() == this;
  }

  Lines *rightmost() const {
    if (type == LinesNodeType::Intermediate) {
      return intermediateNode.rhs->rightmost();
    } else {
      return (Lines *)this;
    }
  }

  Lines *leftmost() const {
    if (type == LinesNodeType::Intermediate) {
      return intermediateNode.lhs->leftmost();
    } else {
      return (Lines *)this;
    }
  }

  Lines *node_at(size_t at) const {
    if (!in_range_lines(at)) return nullptr;

    if (type == LinesNodeType::Intermediate) {
      if (intermediateNode.rhs->line_start <= at) {
        return intermediateNode.rhs->node_at(at);
      } else {
        return intermediateNode.lhs->node_at(at);
      }
    }

    return (Lines *)this;
  }

  /**
   * ITERATOR
   */

  struct LinesIter {
    using iterator_category = forward_iterator_tag;
    using difference_type = ptrdiff_t;
    using value_type = string;
    using pointer = string *;
    using reference = string &;

    int line_ptr;
    int direction;

    LinesIter(Lines *lines, int line_ptr, int direction) : line_ptr(line_ptr), direction(direction), lines(lines) {
    }

    reference operator*() const {
      return lines->leafNode.lines[line_ptr - lines->line_start];
    }

    pointer operator->() {
      return lines->leafNode.lines.data() + (line_ptr - lines->line_start);
    }

    LinesIter operator++() {
      if (lines) {
        line_ptr += direction;

        if (direction == LINES_IT_FWD) {
          if (line_ptr > lines->line_end()) lines = lines->leafNode.right;
        }
        if (direction == LINES_IT_BWD) {
          if (line_ptr < lines->line_start) lines = lines->leafNode.left;
        }
      }

      return *this;
    }

    LinesIter operator++(int) {
      LinesIter current = *this;
      ++(*this);
      return current;
    }

    friend bool operator==(const LinesIter &lhs, const LinesIter &rhs) {
      return lhs.line_ptr == rhs.line_ptr;
    }

    friend bool operator!=(const LinesIter &lhs, const LinesIter &rhs) {
      return lhs.line_ptr != rhs.line_ptr;
    }

   private:
    Lines *lines;
  };

  LinesIter begin() {
    return LinesIter(leftmost(), 0, LINES_IT_FWD);
  }
  LinesIter end() {
    return LinesIter(nullptr, line_count, 0);
  }
  LinesIter rbegin() {
    return LinesIter(rightmost(), line_count - 1, LINES_IT_BWD);
  }
  LinesIter rend() {
    return LinesIter(nullptr, -1, 0);
  }
};

namespace LinesUtil {
bool remove_range(Lines &root, size_t from_line, size_t from_pos, size_t to_line, size_t to_pos) {
  if (root.empty()) return false;

  assert(from_line <= to_line);
  if (from_line == to_line) assert(from_pos <= to_pos);

  // Right end.
  Lines *rhs_node = root.node_at(to_line);
  if (!rhs_node) LOG_RETURN(false, "ERR: remove range left node not found");
  assert(rhs_node->type == LinesNodeType::Leaf);
  assert(!rhs_node->leafNode.is_one_empty_line());

  rhs_node->remove_range_from_single_leaf(from_line, from_pos, to_line, to_pos);

  // Left end.
  Lines *lhs_node = root.node_at(from_line);
  // Only one node truncation.
  if (lhs_node == rhs_node) {
    return true;
  }

  if (!lhs_node) LOG_RETURN(false, "ERR: remove range left node not found");
  assert(lhs_node->type == LinesNodeType::Leaf);
  assert(!lhs_node->leafNode.is_one_empty_line());

  lhs_node->remove_range_from_single_leaf(from_line, from_pos, to_line, to_pos);

  // Merge ends.
  string right_line = rhs_node->leafNode.lines.front();
  rhs_node->leafNode.lines.erase(rhs_node->leafNode.lines.begin());
  rhs_node->adjust_line_count_and_line_start_up_and_right(-1, false);

  lhs_node->leafNode.lines.back().append(right_line);

  // Erase mid section.
  // BUG: rhs_node is not stable after merge-up.
  int del_line_count = rhs_node->line_start - lhs_node->line_end() - 1;
  size_t del_line_idx = lhs_node->line_end() + 1;

  // DO NOT USE `lhs_node` or `rhs_node` below this point!!!

  while (del_line_count > 0) {
    Lines *current_node = root.node_at(del_line_idx);
    assert(current_node);
    assert(current_node->type == LinesNodeType::Leaf);
    assert(current_node->parent);

    del_line_count -= current_node->leafNode.lines.size();

    current_node->adjust_line_count_and_line_start_up_and_right(-current_node->leafNode.lines.size(), false);
    current_node->parent->merge_up(current_node);
  }

  Lines *lhs_reloaded = root.node_at(del_line_idx - 1);
  assert(lhs_reloaded);
  Lines *rhs_reloaded = lhs_reloaded->leafNode.right;
  assert(rhs_reloaded);
  if (rhs_reloaded->empty()) rhs_reloaded->parent->merge_up(rhs_reloaded);

  return true;
}
};  // namespace LinesUtil