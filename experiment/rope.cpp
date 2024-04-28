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

  void split(size_t at) {
    if (type == RopeNodeType::Intermediate) {
      if (at <= intermediateNode.lhs->end) {
        intermediateNode.lhs->split(at);
      } else {
        intermediateNode.rhs->split(at);
      }
    } else {
      assert_at(at);

      unique_ptr<Rope> lhs =
          make_unique<Rope>(start, leafNode.s.substr(0, at - start));
      unique_ptr<Rope> rhs =
          make_unique<Rope>(at, leafNode.s.substr(at - start));

      type = RopeNodeType::Intermediate;
      intermediateNode.lhs.release();
      intermediateNode.rhs.release();
      intermediateNode.lhs.reset(lhs.release());
      intermediateNode.rhs.reset(rhs.release());
    }
  }

  void insert(size_t at, char c) {
    if (type == RopeNodeType::Intermediate) {
      if (at <= intermediateNode.lhs->end) {
        intermediateNode.lhs->insert(at, c);
      } else {
        intermediateNode.rhs->insert(at, c);
      }
    } else {
      if (len() >= ROPE_UNIT_BREAK_THRESHOLD) {
        size_t mid = (end + start + 1) / 2;
        split(mid);
        insert(at, c);
      } else {
        assert_at(at);
        size_t pos = at - start;
        leafNode.s.insert(pos, 1, c);
      }
    }
  }

  void remove(size_t at) {}

  void assert_at(size_t at) const { assert(start <= at && at <= end); }
};

int main() {
  Rope root = Rope(0, "0123456789012345678901234567890123456789");
  cout << root.len() << endl;
  cout << root.debug_to_string() << endl;

  // root.split(10);
  root.insert(20, 'x');
  cout << root.debug_to_string() << endl;

  // root.split(20);
  // cout << root.debug_to_string() << endl;

  // root.split(5);
  // cout << root.debug_to_string() << endl;

  // root.split(30);
  // cout << root.debug_to_string() << endl;
  cout << root.to_string() << endl;

  cout << root.len() << endl;

  return EXIT_SUCCESS;
}
