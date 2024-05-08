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

#define ROPE_UNIT_BREAK_THRESHOLD 8

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
};

struct RopeLeaf {
  string s;

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

struct Rope {
  size_t start;
  size_t size;
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
        leafNode({std::forward<string>(""s)}) {}

  Rope(string &&s)
      : start(0),
        size(s.size()),
        type(RopeNodeType::Leaf),
        config(std::make_shared<RopeConfig>(ROPE_UNIT_BREAK_THRESHOLD)),
        parent(nullptr),
        leafNode({std::forward<string>(s)}) {}

  Rope(shared_ptr<RopeConfig> config, string &&s)
      : start(0),
        size(s.size()),
        type(RopeNodeType::Leaf),
        config(config),
        parent(nullptr),
        leafNode({std::forward<string>(s)}) {}

  Rope(shared_ptr<RopeConfig> config, size_t start, Rope *parent, string &&s)
      : start(start),
        size(s.size()),
        type(RopeNodeType::Leaf),
        config(config),
        parent(parent),
        leafNode({std::forward<string>(s)}) {}

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

      if (intermediateNode.rhs->start <= at) {
        return intermediateNode.rhs->insert(at, std::forward<string>(snippet));
      } else {
        intermediateNode.rhs->adjust_start(1);
        return intermediateNode.lhs->insert(at, std::forward<string>(snippet));
      }
    } else {
      if (size >= config->unit_break_threshold) {
        size_t mid = start + size / 2;
        split(mid);

        return insert(at, std::forward<string>(snippet));
      } else {
        size += snippet.size();

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
      printf("Range error start=%ld size=%ld from=%ld to=%ld\n", start, size,
             from, to);
      return RopeRemoveResult::RangeError;
    }

    if (type == RopeNodeType::Intermediate) {
      size_t rhs_from = max(intermediateNode.rhs->start, from);
      size_t lhs_to = min(intermediateNode.lhs->end(), to);

      printf("INT %ld-%ld .. %ld-%ld\n", from, lhs_to, rhs_from, to);

      if (rhs_from <= to) {
        printf("INT remove right %ld-%ld Adjust %ld\n", rhs_from, to,
               -(to - rhs_from + 1));
        size -= to - rhs_from + 1;
        RopeRemoveResult result =
            intermediateNode.rhs->remove_range(rhs_from, to);
        if (result == RopeRemoveResult::NeedMergeUp) {
          merge_up(false);
        } else if (result == RopeRemoveResult::RangeError) {
          return result;
        }

        if (from <= lhs_to) {
          printf("INT right - also remove left\n");
          return remove_range(from, lhs_to);
        } else {
          return RopeRemoveResult::Success;
        }
      }

      if (from <= lhs_to) {
        printf("INT remove left %ld-%ld Adjust: %ld\n", from, lhs_to,
               -(lhs_to - from + 1));

        size -= lhs_to - from + 1;
        intermediateNode.rhs->adjust_start(-(lhs_to - from + 1));

        RopeRemoveResult result =
            intermediateNode.lhs->remove_range(from, lhs_to);
        if (result == RopeRemoveResult::NeedMergeUp) {
          merge_up(true);
        } else if (result == RopeRemoveResult::RangeError) {
          return result;
        }

        return RopeRemoveResult::Success;
      }

      return RopeRemoveResult::RangeError;
    } else {
      printf("LEAF %ld-%ld\n", from, to);
      size -= to - from + 1;

      size_t pos = from - start;
      leafNode.s.erase(pos, to - from + 1);

      if (empty()) {
        return RopeRemoveResult::NeedMergeUp;
      } else {
        return RopeRemoveResult::Success;
      }
    }
  }

  void merge_up(bool is_left) {
    if (is_left) {
      if (intermediateNode.rhs->type == RopeNodeType::Intermediate) {
        intermediateNode.lhs.reset(nullptr);
        intermediateNode.lhs.swap(intermediateNode.rhs->intermediateNode.lhs);

        auto right_right_child =
            intermediateNode.rhs->intermediateNode.rhs.release();
        intermediateNode.rhs.reset(right_right_child);
      } else {
        string s = intermediateNode.rhs->leafNode.s;
        intermediateNode.RopeIntermediateNode::~RopeIntermediateNode();
        type = RopeNodeType::Leaf;
        leafNode.s.swap(s);
      }
    } else {
      if (intermediateNode.lhs->type == RopeNodeType::Intermediate) {
        intermediateNode.rhs.reset(nullptr);
        intermediateNode.rhs.swap(intermediateNode.lhs->intermediateNode.rhs);

        auto left_left_child =
            intermediateNode.lhs->intermediateNode.lhs.release();
        intermediateNode.lhs.reset(left_left_child);
      } else {
        string s = intermediateNode.lhs->leafNode.s;
        intermediateNode.RopeIntermediateNode::~RopeIntermediateNode();
        type = RopeNodeType::Leaf;
        leafNode.s.swap(s);
      }
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
};
