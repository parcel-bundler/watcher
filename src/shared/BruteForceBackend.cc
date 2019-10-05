#include <string>
#include <fstream>
#include "../DirTree.hh"
#include "../Event.hh"
#include "./BruteForceBackend.hh"

void BruteForceBackend::writeSnapshot(Watcher &watcher, std::string *snapshotPath) {
  std::unique_lock<std::mutex> lock(mMutex);
  auto tree = getTree(watcher);
  std::ofstream ofs(*snapshotPath);
  tree->write(ofs);
}

void BruteForceBackend::getEventsSince(Watcher &watcher, std::string *snapshotPath) {
  std::unique_lock<std::mutex> lock(mMutex);
  std::ifstream ifs(*snapshotPath);
  if (ifs.fail()) {
    return;
  }

  auto snapshot = DirTree(watcher.mDir, ifs);
  auto now = getTree(watcher);
  now->getChanges(&snapshot, watcher.mEvents);
}
