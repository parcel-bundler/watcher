#include <string>
#include <fstream>
#include "../DirTree.hh"
#include "../Event.hh"
#include "../Backend.hh"

DirTree *getDirTree(std::string *dir, std::unordered_set<std::string> *ignore);

void BruteForceBackend::writeSnapshot(std::string *snapshotPath) {
  auto tree = getDirTree(&mDir, &mIgnore);
  std::ofstream ofs(*snapshotPath);
  tree->write(ofs);
}

EventList *BruteForceBackend::getEventsSince(std::string *snapshotPath) {
  std::ifstream ifs(*snapshotPath);
  if (ifs.fail()) {
    return new EventList();
  }

  auto snapshot = new DirTree(ifs);
  auto now = getDirTree(&mDir, &mIgnore);
  return now->getChanges(snapshot);
}
