#pragma once

#include <assert.h>
#include <unistd.h>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <memory>
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

enum class LinesNodeType {
  Intermediate,
  Leaf,
};

enum class LinesSplitResult {
  Success,
  RangeError,
  EmptySplitError,
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

  unique_ptr<Lines> &child(bool is_left) { return is_left ? lhs : rhs; }
};

struct LinesLeaf {
  vector<string> lines{};
  Lines *left{nullptr};
  Lines *right{nullptr};
};

struct LinesConfig {
  size_t unit_break_threshold;

  LinesConfig(size_t unit_break_threshold)
      : unit_break_threshold(unit_break_threshold) {}
};

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
        parent(nullptr),
        leafNode({}) {}

  Lines(vector<string> &&lines)
      : line_start(0),
        line_count(lines.size()),
        type(LinesNodeType::Leaf),
        config(std::make_shared<LinesConfig>(LINES_UNIT_BREAK_THRESHOLD)),
        parent(nullptr),
        leafNode({std::forward<vector<string>>(lines)}) {}

  Lines(shared_ptr<LinesConfig> config, vector<string> &&lines)
      : line_start(0),
        line_count(lines.size()),
        type(LinesNodeType::Leaf),
        config(config),
        parent(nullptr),
        leafNode({std::forward<vector<string>>(lines)}) {}

  Lines(shared_ptr<LinesConfig> config, size_t start, Lines *parent,
        vector<string> &&lines)
      : line_start(start),
        line_count(lines.size()),
        type(LinesNodeType::Leaf),
        config(config),
        parent(parent),
        leafNode({std::forward<vector<string>>(lines)}) {}

  ~Lines() {
    if (type == LinesNodeType::Intermediate) {
      delete intermediateNode.lhs.release();
      delete intermediateNode.rhs.release();
    } else {
      leafNode.LinesLeaf::~LinesLeaf();
    }
  }

  /**
   * OUTPUT
   */

  string to_string() const {
    if (type == LinesNodeType::Intermediate) {
      return intermediateNode.lhs->to_string() +
             intermediateNode.rhs->to_string();
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
      ss << "(" << intermediateNode.lhs->debug_to_string() << ")("
         << intermediateNode.rhs->debug_to_string() << ")";
    } else {
      if (empty()) {
        ss << std::to_string(line_start) << "[]";
      } else {
        ss << std::to_string(line_start) << ":" << std::to_string(line_end());
        for (auto &line : leafNode.lines) ss << "[" << line << "]";
      }
    }

    return ss.str();
  }

  /**
   * OPERATIONS
   */

  LinesSplitResult split(size_t line_idx) {
    if (type == LinesNodeType::Intermediate) {
      if (intermediateNode.rhs->line_start <= line_idx) {
        return intermediateNode.rhs->split(line_idx);
      } else {
        return intermediateNode.lhs->split(line_idx);
      }
    } else {
      if (!in_range(line_idx)) return LinesSplitResult::RangeError;

      if (line_idx == line_start || line_end() + 1 == line_idx)
        return LinesSplitResult::EmptySplitError;

      unique_ptr<Lines> lhs = make_unique<Lines>(
          config, line_start, this,
          vector<string>{leafNode.lines.begin(),
                         leafNode.lines.begin() + (line_idx - line_start)});
      unique_ptr<Lines> rhs = make_unique<Lines>(
          config, line_idx, this,
          vector<string>{leafNode.lines.begin() + (line_idx - line_start),
                         leafNode.lines.end()});

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
      intermediateNode.lhs.release();
      intermediateNode.rhs.release();
      intermediateNode.lhs.reset(lhs.release());
      intermediateNode.rhs.reset(rhs.release());

      return LinesSplitResult::Success;
    }
  }

  bool insert(size_t line_idx, size_t pos, string &&snippet) {
    if (!in_range(line_idx)) return false;

    if (type == LinesNodeType::Intermediate) {
      if (intermediateNode.rhs->line_start <= line_idx) {
        return intermediateNode.rhs->insert(line_idx, pos,
                                            std::forward<string>(snippet));
      } else {
        return intermediateNode.lhs->insert(line_idx, pos,
                                            std::forward<string>(snippet));
      }
    } else {
      size_t line_relative_idx = line_idx - line_start;
      // TODO: Handle new line chars in `snippet`.
      leafNode.lines[line_relative_idx].insert(pos, snippet);

      return true;
    }
  }

  void adjust_start(int diff) {
    line_start += diff;

    if (type == LinesNodeType::Intermediate) {
      intermediateNode.lhs->adjust_start(diff);
      intermediateNode.rhs->adjust_start(diff);
    }
  }

  LinesRemoveResult remove_char(size_t line_idx, size_t pos) {
    if (!in_range_chars(line_idx)) return LinesRemoveResult::RangeError;

    if (type == LinesNodeType::Intermediate) {
      size--;

      bool is_left_adjusted = !intermediateNode.lhs->empty() &&
                              intermediateNode.lhs->endpos() >= at;
      LinesRemoveResult result;

      if (is_left_adjusted) {
        intermediateNode.rhs->adjust_start(-1);
        result = intermediateNode.lhs->remove(at);
      } else {
        result = intermediateNode.rhs->remove(at);
      }

      if (result == LinesRemoveResult::NeedMergeUp) {
        merge_up(is_left_adjusted);
        return LinesRemoveResult::Success;
      } else {
        return result;
      }
    } else {
      size--;

      size_t pos = at - start;
      leafNode.s.erase(pos, 1);

      if (empty()) {
        return LinesRemoveResult::NeedMergeUp;
      } else {
        return LinesRemoveResult::Success;
      }
    }
  }

  LinesRemoveResult remove_range(size_t from, size_t to) {
    if (!in_range_chars(from) || !in_range_chars(to)) {
      return LinesRemoveResult::RangeError;
    }

    if (type == LinesNodeType::Intermediate) {
      size_t rhs_from = max(intermediateNode.rhs->start, from);
      size_t lhs_to = min(intermediateNode.lhs->endpos(), to);

      if (rhs_from <= to) {
        size -= to - rhs_from + 1;
        LinesRemoveResult result =
            intermediateNode.rhs->remove_range(rhs_from, to);
        if (result == LinesRemoveResult::NeedMergeUp) {
          merge_up(RIGHT);
        } else if (result == LinesRemoveResult::RangeError) {
          return result;
        }

        if (from <= lhs_to) {
          return remove_range(from, lhs_to);
        } else {
          return LinesRemoveResult::Success;
        }
      }

      if (from <= lhs_to) {
        size -= lhs_to - from + 1;
        intermediateNode.rhs->adjust_start(-(lhs_to - from + 1));

        LinesRemoveResult result =
            intermediateNode.lhs->remove_range(from, lhs_to);
        if (result == LinesRemoveResult::NeedMergeUp) {
          merge_up(LEFT);
        } else if (result == LinesRemoveResult::RangeError) {
          return result;
        }

        return LinesRemoveResult::Success;
      }

      return LinesRemoveResult::RangeError;
    } else {
      size -= to - from + 1;

      size_t pos = from - start;
      leafNode.s.erase(pos, to - from + 1);

      if (empty()) {
        return LinesRemoveResult::NeedMergeUp;
      } else {
        return LinesRemoveResult::Success;
      }
    }
  }

  void merge_up(bool empty_node) {
    if (intermediateNode.child(!empty_node)->type ==
        LinesNodeType::Intermediate) {
      // Adjust sibling pointers.
      Lines *old_left = intermediateNode.child(empty_node)->leafNode.left;
      Lines *old_right = intermediateNode.child(empty_node)->leafNode.right;
      if (old_left) old_left->leafNode.right = old_right;
      if (old_right) old_right->leafNode.left = old_left;

      intermediateNode.child(empty_node).reset(nullptr);
      intermediateNode.child(empty_node)
          .swap(intermediateNode.child(!empty_node)
                    ->intermediateNode.child(empty_node));

      auto grandchild = intermediateNode.child(!empty_node)
                            ->intermediateNode.child(!empty_node)
                            .release();
      intermediateNode.child(!empty_node).reset(grandchild);
    } else {
      assert(type == LinesNodeType::Intermediate);

      string s = intermediateNode.child(!empty_node)->leafNode.s;
      Lines *old_left_sib = intermediateNode.lhs->leafNode.left;
      Lines *old_right_sib = intermediateNode.rhs->leafNode.right;

      // Kill old self subtype.
      intermediateNode.LinesIntermediateNode::~LinesIntermediateNode();

      type = LinesNodeType::Leaf;
      leafNode.s.swap(s);
      leafNode.left = old_left_sib;
      leafNode.right = old_right_sib;
      if (old_left_sib) old_left_sib->leafNode.right = this;
      if (old_right_sib) old_right_sib->leafNode.left = this;
    }
  }

  /**
   * BOUNDS
   */

  bool empty() const { return line_count == 0; }

  size_t line_end() const {
    // Must check emptiness before calling this.
    assert(!empty());
    return line_start + line_count - 1;
  }

  bool in_range(size_t at) const {
    return (empty() && at == line_start) ||
           (line_start <= at && at <= line_end() + 1);
  }

  bool in_range_chars(size_t at) const {
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
    if (!in_range_chars(at)) return nullptr;

    if (type == LinesNodeType::Intermediate) {
      if (intermediateNode.rhs->start <= at) {
        return intermediateNode.rhs->node_at(at);
      } else {
        return intermediateNode.lhs->node_at(at);
      }
    }

    return (Lines *)this;
  }

  int next_line_at(size_t at) const {
    if (type == LinesNodeType::Intermediate) {
      auto node = node_at(at);
      if (!node) return -1;
      return node->next_line_at(at);
    }

    int result = leafNode.next_char_after(at - start, '\n');
    if (result >= 0) return result + start;

    auto next_node = leafNode.right;
    if (!next_node) return -1;

    return next_node->next_line_at(next_node->start);
  }

  int prev_line_at(size_t at) const {
    if (type == LinesNodeType::Intermediate) {
      auto node = node_at(at);
      if (!node) return -1;
      return node->prev_line_at(at);
    }

    int result = leafNode.prev_char_before(at - start, '\n');
    if (result >= 0) return result + start;

    auto prev_node = leafNode.left;
    if (!prev_node) return -1;

    return prev_node->prev_line_at(prev_node->endpos());
  }

  int nth_new_line_at(size_t nth) const {
    if (type == LinesNodeType::Intermediate) {
      return leftmost()->nth_new_line_at(nth);
    } else {
      for (size_t i = 0; i < leafNode.s.size(); i++) {
        if (leafNode.s[i] == '\n') {
          if (nth == 0) return start + i;
          nth--;
        }
      }

      if (leafNode.right) {
        return leafNode.right->nth_new_line_at(nth);
      } else {
        return -1;
      }
    }
  }

  /**
   * ITERATOR
   */

  struct LinesIter {
    using iterator_category = forward_iterator_tag;
    using difference_type = ptrdiff_t;
    using value_type = char;
    using pointer = char *;
    using reference = char &;

    size_t at_ptr;

    LinesIter(Lines *rope, size_t at_ptr) : at_ptr(at_ptr), rope(rope) {}

    reference operator*() const {
      return rope->leafNode.s[at_ptr - rope->start];
    }

    pointer operator->() {
      return rope->leafNode.s.data() + (at_ptr - rope->start);
    }

    LinesIter operator++() {
      if (rope) {
        at_ptr++;
        if (at_ptr > rope->endpos()) rope = rope->leafNode.right;
      }

      return *this;
    }

    LinesIter operator++(int) {
      LinesIter current = *this;
      ++(*this);
      return current;
    }

    friend bool operator==(const LinesIter &lhs, const LinesIter &rhs) {
      return lhs.at_ptr == rhs.at_ptr;
    }

    friend bool operator!=(const LinesIter &lhs, const LinesIter &rhs) {
      return lhs.at_ptr != rhs.at_ptr;
    }

   private:
    Lines *rope;
  };

  LinesIter begin() { return LinesIter(leftmost(), 0); }
  LinesIter end() { return LinesIter(nullptr, size); }
};

namespace LinesUtil {
size_t count_new_lines(const string &s) {
  size_t out{0};
  for (auto &c : s) {
    if (c == '\n') out++;
  }
  return out;
}
size_t count_new_lines(const string &s, size_t from, size_t to) {
  size_t out{0};
  for (size_t i = from; i <= to; i++) {
    if (s[i] == '\n') out++;
  }
  return out;
}

int nth_new_line_pos(const string &s, size_t nth) {
  for (int i = 0; i < s.size(); i++) {
    if (s[i] == '\n') {
      if (nth == 0) return i;
      nth--;
    }
  }
  return -1;
}

string nth_line(Lines const &rope, size_t nth) {
  if (rope.empty()) return "";

  int start_pos = nth == 0 ? 0 : rope.nth_new_line_at(nth - 1);
  if (start_pos == -1) return "";

  int end_pos = rope.nth_new_line_at(nth);
  if (end_pos == -1) end_pos = rope.endpos();
  if (start_pos >= end_pos - 1) return "";

  return rope.substr(start_pos + 1, end_pos - start_pos - 1);
}

size_t new_line_count(Lines const &rope) {
  Lines *r = rope.leftmost();
  size_t out{0};
  while (r) {
    for (auto &c : r->leafNode.s) {
      if (c == '\n') out++;
    }
    r = r->leafNode.right;
  }
  return out;
}

int find_str(Lines &rope, string const &pattern, size_t pos) {
  Lines::LinesIter it = Lines::LinesIter(rope.node_at(pos), pos);
  Lines::LinesIter it_end = rope.end();

  while (it != it_end) {
    if (*it == pattern[0]) {
      auto it_current = it;
      bool has_found{true};
      for (int i = 0; i < pattern.size() && it_current != it_end; i++) {
        if (*it_current != pattern[i]) {
          has_found = false;
          break;
        }
      }

      if (has_found) {
        return it.at_ptr;
      }
    }
  }

  return -1;
}
};  // namespace LinesUtil
