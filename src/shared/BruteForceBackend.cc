#include <string>
#include <fstream>
#include "../DirTree.hh"
#include "../Event.hh"
#include "./BruteForceBackend.hh"

DirTree *getDirTree(std::string *dir, std::unordered_set<std::string> *ignore);

void BruteForceBackend::writeSnapshot(Watcher &watcher, std::string *snapshotPath) {
  auto tree = getDirTree(&watcher.mDir, &watcher.mIgnore);
  std::ofstream ofs(*snapshotPath);
  tree->write(ofs);
}

void BruteForceBackend::getEventsSince(Watcher &watcher, std::string *snapshotPath) {
  std::ifstream ifs(*snapshotPath);
  if (ifs.fail()) {
    return;
  }

  auto snapshot = new DirTree(ifs);
  auto now = getDirTree(&watcher.mDir, &watcher.mIgnore);
  now->getChanges(snapshot, watcher.mEvents);
}
