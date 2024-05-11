#pragma once

#include <assert.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <optional>
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

#define ROPE_UNIT_BREAK_THRESHOLD 8
#define LEFT true
#define RIGHT !LEFT

enum class RopeNodeType {
  Intermediate,
  Leaf,
};

enum class RopeSplitResult {
  Success,
  RangeError,
  EmptySplitError,
};

enum class RopeRemoveResult {
  Success,
  NeedMergeUp,
  RangeError,
};

struct Rope;

struct RopeIntermediateNode {
  unique_ptr<Rope> lhs;
  unique_ptr<Rope> rhs;

  unique_ptr<Rope> &child(bool is_left) { return is_left ? lhs : rhs; }
};

struct RopeLeaf {
  string s;
  Rope *left{nullptr};
  Rope *right{nullptr};

  int next_char_after(size_t pos, char ch) const {
    while (pos < s.size()) {
      if (s[pos] == ch) return pos;
      pos++;
    }
    return -1;
  }

  int prev_char_before(size_t pos, char ch) const {
    while (pos >= 0) {
      if (s[pos] == ch) return pos;
      if (pos == 0) return -1;

      pos--;
    }

    return -1;
  }
};

struct RopeConfig {
  size_t unit_break_threshold;

  RopeConfig(size_t unit_break_threshold)
      : unit_break_threshold(unit_break_threshold) {}
};

namespace RopeUtil {
size_t count_new_lines(const string &s);
size_t count_new_lines(const string &s, size_t from, size_t to);
int nth_new_line_pos(const string &s, size_t nth);
};  // namespace RopeUtil

struct Rope {
  size_t start;
  size_t size;
  size_t new_line_count{0};
  RopeNodeType type{RopeNodeType::Intermediate};
  shared_ptr<RopeConfig> config;
  Rope *parent;

  union {
    RopeIntermediateNode intermediateNode;
    RopeLeaf leafNode;
  };

  Rope()
      : start(0),
        size(0),
        type(RopeNodeType::Leaf),
        config(std::make_shared<RopeConfig>(ROPE_UNIT_BREAK_THRESHOLD)),
        parent(nullptr),
        leafNode({std::forward<string>(""s)}) {
    new_line_count = 0;
  }

  Rope(string &&s)
      : start(0),
        size(s.size()),
        type(RopeNodeType::Leaf),
        config(std::make_shared<RopeConfig>(ROPE_UNIT_BREAK_THRESHOLD)),
        parent(nullptr),
        leafNode({std::forward<string>(s)}) {
    new_line_count = RopeUtil::count_new_lines(leafNode.s);
  }

  Rope(shared_ptr<RopeConfig> config, string &&s)
      : start(0),
        size(s.size()),
        type(RopeNodeType::Leaf),
        config(config),
        parent(nullptr),
        leafNode({std::forward<string>(s)}) {
    new_line_count = RopeUtil::count_new_lines(leafNode.s);
  }

  Rope(shared_ptr<RopeConfig> config, size_t start, Rope *parent, string &&s)
      : start(start),
        size(s.size()),
        type(RopeNodeType::Leaf),
        config(config),
        parent(parent),
        leafNode({std::forward<string>(s)}) {
    new_line_count = RopeUtil::count_new_lines(leafNode.s);
  }

  ~Rope() {
    if (type == RopeNodeType::Intermediate) {
      delete intermediateNode.lhs.release();
      delete intermediateNode.rhs.release();
    } else {
      leafNode.RopeLeaf::~RopeLeaf();
    }
  }

  /**
   * OUTPUT
   */

  string to_string() const {
    if (type == RopeNodeType::Intermediate) {
      return intermediateNode.lhs->to_string() +
             intermediateNode.rhs->to_string();
    } else {
      return leafNode.s;
    }
  }

  string debug_to_string() const {
    if (type == RopeNodeType::Intermediate) {
      return intermediateNode.lhs->debug_to_string() +
             intermediateNode.rhs->debug_to_string();
    } else {
      if (empty()) {
        return "[" + std::to_string(start) + ":-]";
      } else {
        return "[" + std::to_string(start) + ":" + std::to_string(end()) + " " +
               leafNode.s + "]";
      }
    }
  }

  string substr(size_t at, size_t len) const {
    if (!in_range(at)) return "";

    if (type == RopeNodeType::Intermediate) {
      Rope *start_node = node_at(at);
      if (!start_node) return "";
      return start_node->substr(at, len);
    } else {
      size_t pos = at - start;
      size_t in_node_len = min(len, size - pos);

      string out = leafNode.s.substr(pos, in_node_len);

      bool need_more = in_node_len < len;
      if (need_more) {
        Rope *next_node = next();
        if (next_node) {
          return out + next_node->substr(next_node->start, len - in_node_len);
        }
      }

      return out;
    }
  }

  /**
   * OPERATIONS
   */

  RopeSplitResult split(size_t at) {
    if (type == RopeNodeType::Intermediate) {
      if (intermediateNode.rhs->start <= at) {
        return intermediateNode.rhs->split(at);
      } else {
        return intermediateNode.lhs->split(at);
      }
    } else {
      if (!in_range(at)) return RopeSplitResult::RangeError;

      if (at == start || end() + 1 == at)
        return RopeSplitResult::EmptySplitError;

      unique_ptr<Rope> lhs = make_unique<Rope>(
          config, start, this, leafNode.s.substr(0, at - start));
      unique_ptr<Rope> rhs =
          make_unique<Rope>(config, at, this, leafNode.s.substr(at - start));

      // Set sibling pointers.
      Rope *old_left_sib = leafNode.left;
      Rope *old_right_sib = leafNode.right;
      lhs->leafNode.right = rhs.get();
      lhs->leafNode.left = old_left_sib;
      rhs->leafNode.left = lhs.get();
      rhs->leafNode.right = old_right_sib;
      if (old_left_sib) old_left_sib->leafNode.right = lhs.get();
      if (old_right_sib) old_right_sib->leafNode.left = rhs.get();

      leafNode.RopeLeaf::~RopeLeaf();

      type = RopeNodeType::Intermediate;
      intermediateNode.lhs.release();
      intermediateNode.rhs.release();
      intermediateNode.lhs.reset(lhs.release());
      intermediateNode.rhs.reset(rhs.release());

      return RopeSplitResult::Success;
    }
  }

  bool insert(size_t at, string &&snippet) {
    if (!in_range(at)) return false;

    if (type == RopeNodeType::Intermediate) {
      size += snippet.size();
      new_line_count += RopeUtil::count_new_lines(snippet);

      if (intermediateNode.rhs->start <= at) {
        return intermediateNode.rhs->insert(at, std::forward<string>(snippet));
      } else {
        intermediateNode.rhs->adjust_start(snippet.size());
        return intermediateNode.lhs->insert(at, std::forward<string>(snippet));
      }
    } else {
      if (size >= config->unit_break_threshold) {
        size_t mid = start + size / 2;
        split(mid);

        return insert(at, std::forward<string>(snippet));
      } else {
        size += snippet.size();
        new_line_count += RopeUtil::count_new_lines(snippet);

        size_t pos = at - start;
        leafNode.s.insert(pos, snippet);

        return true;
      }
    }
  }

  void adjust_start(int diff) {
    start += diff;

    if (type == RopeNodeType::Intermediate) {
      intermediateNode.lhs->adjust_start(diff);
      intermediateNode.rhs->adjust_start(diff);
    }
  }

  void adjust_new_line_count_to_all_parents(int diff) {
    if (!parent) return;

    parent->new_line_count += diff;
    parent->adjust_new_line_count_to_all_parents(diff);
  }

  RopeRemoveResult remove(size_t at) {
    if (!in_range_chars(at)) return RopeRemoveResult::RangeError;

    if (type == RopeNodeType::Intermediate) {
      size--;

      bool is_left_adjusted =
          !intermediateNode.lhs->empty() && intermediateNode.lhs->end() >= at;
      RopeRemoveResult result;

      if (is_left_adjusted) {
        intermediateNode.rhs->adjust_start(-1);
        result = intermediateNode.lhs->remove(at);
      } else {
        result = intermediateNode.rhs->remove(at);
      }

      if (result == RopeRemoveResult::NeedMergeUp) {
        merge_up(is_left_adjusted);
        return RopeRemoveResult::Success;
      } else {
        return result;
      }
    } else {
      size--;

      size_t pos = at - start;
      if (leafNode.s[pos] == '\n') {
        new_line_count--;
        adjust_new_line_count_to_all_parents(-1);
      }

      leafNode.s.erase(pos, 1);

      if (empty()) {
        return RopeRemoveResult::NeedMergeUp;
      } else {
        return RopeRemoveResult::Success;
      }
    }
  }

  RopeRemoveResult remove_range(size_t from, size_t to) {
    if (!in_range_chars(from) || !in_range_chars(to)) {
      return RopeRemoveResult::RangeError;
    }

    if (type == RopeNodeType::Intermediate) {
      size_t rhs_from = max(intermediateNode.rhs->start, from);
      size_t lhs_to = min(intermediateNode.lhs->end(), to);

      if (rhs_from <= to) {
        size -= to - rhs_from + 1;
        RopeRemoveResult result =
            intermediateNode.rhs->remove_range(rhs_from, to);
        if (result == RopeRemoveResult::NeedMergeUp) {
          merge_up(RIGHT);
        } else if (result == RopeRemoveResult::RangeError) {
          return result;
        }

        if (from <= lhs_to) {
          return remove_range(from, lhs_to);
        } else {
          return RopeRemoveResult::Success;
        }
      }

      if (from <= lhs_to) {
        size -= lhs_to - from + 1;
        intermediateNode.rhs->adjust_start(-(lhs_to - from + 1));

        RopeRemoveResult result =
            intermediateNode.lhs->remove_range(from, lhs_to);
        if (result == RopeRemoveResult::NeedMergeUp) {
          merge_up(LEFT);
        } else if (result == RopeRemoveResult::RangeError) {
          return result;
        }

        return RopeRemoveResult::Success;
      }

      return RopeRemoveResult::RangeError;
    } else {
      size -= to - from + 1;

      size_t pos = from - start;
      size_t removed_new_line_count =
          RopeUtil::count_new_lines(leafNode.s, pos, to - start);
      if (removed_new_line_count > 0) {
        new_line_count -= removed_new_line_count;
        adjust_new_line_count_to_all_parents(-removed_new_line_count);
      }

      leafNode.s.erase(pos, to - from + 1);

      if (empty()) {
        return RopeRemoveResult::NeedMergeUp;
      } else {
        return RopeRemoveResult::Success;
      }
    }
  }

  void merge_up(bool empty_node) {
    if (intermediateNode.child(!empty_node)->type ==
        RopeNodeType::Intermediate) {
      intermediateNode.child(empty_node).reset(nullptr);
      intermediateNode.child(empty_node)
          .swap(intermediateNode.child(!empty_node)
                    ->intermediateNode.child(empty_node));

      auto grandchild = intermediateNode.child(!empty_node)
                            ->intermediateNode.child(!empty_node)
                            .release();
      intermediateNode.child(!empty_node).reset(grandchild);
    } else {
      string s = intermediateNode.child(!empty_node)->leafNode.s;
      Rope *old_left_sib = intermediateNode.lhs->leafNode.left;
      Rope *old_right_sib = intermediateNode.rhs->leafNode.right;

      // Kill old self subtype.
      intermediateNode.RopeIntermediateNode::~RopeIntermediateNode();

      type = RopeNodeType::Leaf;
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

  bool empty() const { return size == 0; }

  size_t end() const {
    // Must check emptiness before calling this.
    assert(!empty());
    return start + size - 1;
  }

  bool in_range(size_t at) const {
    return (empty() && at == start) || (start <= at && at <= end() + 1);
  }

  bool in_range_chars(size_t at) const {
    return !empty() && start <= at && at <= end();
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

  // TODO: We could cache leaf-sibling pointers.
  Rope *prev() const {
    if (!parent) return nullptr;
    if (is_right_child()) return parent->intermediateNode.lhs->rightmost();
    return parent->prev();
  }

  Rope *rightmost() const {
    if (type == RopeNodeType::Intermediate) {
      return intermediateNode.rhs->rightmost();
    } else {
      return (Rope *)this;
    }
  }

  Rope *next() const {
    if (!parent) return nullptr;
    if (is_left_child()) return parent->intermediateNode.rhs->leftmost();
    return parent->next();
  }

  Rope *leftmost() const {
    if (type == RopeNodeType::Intermediate) {
      return intermediateNode.lhs->leftmost();
    } else {
      return (Rope *)this;
    }
  }

  Rope *node_at(size_t at) const {
    if (!in_range_chars(at)) return nullptr;

    if (type == RopeNodeType::Intermediate) {
      if (intermediateNode.rhs->start <= at) {
        return intermediateNode.rhs->node_at(at);
      } else {
        return intermediateNode.lhs->node_at(at);
      }
    }

    return (Rope *)this;
  }

  int next_line_at(size_t at) const {
    if (type == RopeNodeType::Intermediate) {
      auto node = node_at(at);
      if (!node) return -1;
      return node->next_line_at(at);
    }

    int result = leafNode.next_char_after(at - start, '\n');
    if (result >= 0) return result + start;

    auto next_node = next();
    if (!next_node) return -1;

    return next_node->next_line_at(next_node->start);
  }

  int prev_line_at(size_t at) const {
    if (type == RopeNodeType::Intermediate) {
      auto node = node_at(at);
      if (!node) return -1;
      return node->prev_line_at(at);
    }

    int result = leafNode.prev_char_before(at - start, '\n');
    if (result >= 0) return result + start;

    auto prev_node = prev();
    if (!prev_node) return -1;

    return prev_node->prev_line_at(prev_node->end());
  }

  int nth_new_line_at(size_t nth) const {
    if (new_line_count <= nth) return -1;

    if (type == RopeNodeType::Intermediate) {
      if (intermediateNode.lhs->new_line_count >= nth + 1) {
        return intermediateNode.lhs->nth_new_line_at(nth);
      } else {
        return intermediateNode.rhs->nth_new_line_at(
            nth - intermediateNode.lhs->new_line_count);
      }
    } else {
      int relative_pos = RopeUtil::nth_new_line_pos(leafNode.s, nth);
      assert(relative_pos >= 0);

      return start + relative_pos;
    }
  }
};

namespace RopeUtil {
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

string nth_line(Rope const &rope, size_t nth) {
  if (rope.empty()) return "";

  size_t start_pos = nth == 0 ? 0 : rope.nth_new_line_at(nth - 1);
  if (start_pos == -1) return "";

  size_t end_pos = rope.nth_new_line_at(nth);
  if (end_pos == -1) end_pos = rope.end();
  if (start_pos >= end_pos - 1) return "";

  return rope.substr(start_pos + 1, end_pos - start_pos - 1);
}
};  // namespace RopeUtil