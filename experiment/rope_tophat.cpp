#include <iostream>

#include "rope.h"

using namespace std;

int main() {
  Rope root = Rope("0123456789012345678901234567890123456789"s);
  cout << root.len() << endl;
  cout << root.debug_to_string() << endl;

  root.split(10);
  root.insert(20, 'x');
  cout << root.debug_to_string() << endl;

  root.split(20);
  cout << root.debug_to_string() << endl;

  root.split(5);
  cout << root.debug_to_string() << endl;

  root.split(30);
  cout << root.debug_to_string() << endl;
  cout << root.to_string() << endl;

  cout << root.len() << endl;

  root.remove(21);
  cout << root.debug_to_string() << endl;
  root.remove(20);
  cout << root.debug_to_string() << endl;

  return EXIT_SUCCESS;
}
