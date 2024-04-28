#include <assert.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace std;

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

  string debug_to_string() {
    if (type == RopeNodeType::Intermediate) {
      string out{};
      out = out + intermediateNode.lhs->debug_to_string();
      out = out + intermediateNode.rhs->debug_to_string();
      return out;
    } else {
      return "[" + std::to_string(start) + ":" + std::to_string(end) + " " +
             leafNode.s + "]";
    }
  }

  void split(size_t at) {
    if (type == RopeNodeType::Intermediate) {
      if (at <= intermediateNode.lhs->end) {
        intermediateNode.lhs->split(at);
      } else {
        intermediateNode.rhs->split(at);
      }
    } else {
      assert(start <= at && at <= end);

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
};

int main() {
  Rope root = Rope(0, "hello world another line and another");
  cout << root.debug_to_string() << endl;

  root.split(10);
  cout << root.debug_to_string() << endl;

  root.split(20);
  cout << root.debug_to_string() << endl;

  root.split(5);
  cout << root.debug_to_string() << endl;

  root.split(30);
  cout << root.debug_to_string() << endl;

  return EXIT_SUCCESS;
}
