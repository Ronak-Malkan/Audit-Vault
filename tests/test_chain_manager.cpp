// test_chain_manager.cpp

#include "chain_manager.h"
#include <cassert>
#include <iostream>
#include <cstdio>    // for std::remove()

int main() {
  const char* testpath = "test_chain.json";

  // Ensure clean slate
  std::remove(testpath);

  // 1) Empty start
  ChainManager cm(testpath);
  assert(cm.getLastID() == 0);
  assert(cm.getLastHash().empty());
  assert(cm.getLastMerkleRoot().empty());
  assert(cm.getAll().empty());
  std::cout << "[Test] Empty start OK\n";

  // 2) Append one block
  BlockMeta m1{1, "h1", "", "mr1"};
  cm.append(m1);
  assert(cm.getLastID() == 1);
  assert(cm.getLastHash() == "h1");
  assert(cm.getLastMerkleRoot() == "mr1");
  auto all = cm.getAll();
  assert(all.size() == 1);
  assert(all[0].previous_hash.empty());
  std::cout << "[Test] Append one block OK\n";

  // 3) Append a second block
  BlockMeta m2{2, "h2", "h1", "mr2"};
  cm.append(m2);
  assert(cm.getLastID() == 2);
  assert(cm.getLastHash() == "h2");
  all = cm.getAll();
  assert(all.size() == 2);
  assert(all[1].previous_hash == "h1");
  std::cout << "[Test] Append second block OK\n";

  std::cout << "🎉 All ChainManager tests passed\n";
  return 0;
}
