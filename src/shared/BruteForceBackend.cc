#include <string>
#include <fstream>
#include "../DirTree.hh"
#include "../Event.hh"
#include "./BruteForceBackend.hh"

DirTree *BruteForceBackend::getTree(Watcher &watcher) {
  if (!watcher.mTree) {
    watcher.mTree = readTree(watcher);
  }

  return watcher.mTree;
}

void BruteForceBackend::writeSnapshot(Watcher &watcher, std::string *snapshotPath) {
  auto tree = getTree(watcher);
  std::ofstream ofs(*snapshotPath);
  tree->write(ofs);
}

void BruteForceBackend::getEventsSince(Watcher &watcher, std::string *snapshotPath) {
  std::ifstream ifs(*snapshotPath);
  if (ifs.fail()) {
    return;
  }

  auto snapshot = new DirTree(ifs);
  auto now = getTree(watcher);
  now->getChanges(snapshot, watcher.mEvents);
}
