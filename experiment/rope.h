#pragma once

#include <assert.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace std;

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

struct Rope;

struct RopeIntermediateNode {
  unique_ptr<Rope> lhs;
  unique_ptr<Rope> rhs;
};

struct RopeLeaf {
  string s{};
};

struct Rope {
  size_t start;
  size_t end;
  RopeNodeType type{RopeNodeType::Intermediate};

  union {
    RopeIntermediateNode intermediateNode;
    RopeLeaf leafNode;
  };

  Rope(string s) : start(0), type(RopeNodeType::Leaf), leafNode({s}) {
    end = start + s.size() - 1;
  }

  Rope(size_t start, string s)
      : start(start), type(RopeNodeType::Leaf), leafNode({s}) {
    end = start + s.size() - 1;
  }

  ~Rope() {
    if (type == RopeNodeType::Intermediate) {
      delete this->intermediateNode.lhs.release();
      delete this->intermediateNode.rhs.release();
    }
  }

  string to_string() {
    if (type == RopeNodeType::Intermediate) {
      return intermediateNode.lhs->to_string() +
             intermediateNode.rhs->to_string();
    } else {
      return leafNode.s;
    }
  }

  string debug_to_string() {
    if (type == RopeNodeType::Intermediate) {
      return intermediateNode.lhs->debug_to_string() +
             intermediateNode.rhs->debug_to_string();
    } else {
      return "[" + std::to_string(start) + ":" + std::to_string(end) + " " +
             leafNode.s + "]";
    }
  }

  size_t len() const { return end - start + 1; }

  RopeSplitResult split(size_t at) {
    if (type == RopeNodeType::Intermediate) {
      if (at <= intermediateNode.lhs->end) {
        return intermediateNode.lhs->split(at);
      } else {
        return intermediateNode.rhs->split(at);
      }
    } else {
      if (!in_range(at)) return RopeSplitResult::RangeError;

      if (at - start == 0 || 1 + end - at == 0)
        return RopeSplitResult::EmptySplitError;

      unique_ptr<Rope> lhs =
          make_unique<Rope>(start, leafNode.s.substr(0, at - start));
      unique_ptr<Rope> rhs =
          make_unique<Rope>(at, leafNode.s.substr(at - start));

      type = RopeNodeType::Intermediate;
      intermediateNode.lhs.release();
      intermediateNode.rhs.release();
      intermediateNode.lhs.reset(lhs.release());
      intermediateNode.rhs.reset(rhs.release());

      return RopeSplitResult::Success;
    }
  }

  bool insert(size_t at, char c) {
    if (!in_range(at)) return false;

    if (type == RopeNodeType::Intermediate) {
      end++;

      if (at <= intermediateNode.lhs->end) {
        intermediateNode.rhs->increase_start_and_end();
        return intermediateNode.lhs->insert(at, c);
      } else {
        return intermediateNode.rhs->insert(at, c);
      }
    } else {
      if (len() >= ROPE_UNIT_BREAK_THRESHOLD) {
        size_t mid = (end + start + 1) / 2;
        split(mid);

        return insert(at, c);
      } else {
        end++;

        size_t pos = at - start;
        leafNode.s.insert(pos, 1, c);

        return true;
      }
    }
  }

  void increase_start_and_end() {
    start++;
    end++;

    if (type == RopeNodeType::Intermediate) {
      intermediateNode.lhs->increase_start_and_end();
      intermediateNode.rhs->increase_start_and_end();
    }
  }

  // void remove(size_t at) {
  //   // if ()
  // }

  bool in_range(size_t at) const { return start <= at && at <= end + 1; }
};