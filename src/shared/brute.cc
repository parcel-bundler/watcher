#include <string>
#include <fstream>
#include "../DirTree.hh"
#include "../Event.hh"
#include "../Backend.hh"

DirTree *getDirTree(std::string *dir, std::unordered_set<std::string> *ignore);

void BruteForceBackend::writeSnapshot(std::string *dir, std::string *snapshotPath, std::unordered_set<std::string> *ignore) {
  auto tree = getDirTree(dir, ignore);
  std::ofstream ofs(*snapshotPath);
  tree->write(ofs);
}

EventList *BruteForceBackend::getEventsSince(std::string *dir, std::string *snapshotPath, std::unordered_set<std::string> *ignore) {
  std::ifstream ifs(*snapshotPath);
  if (ifs.fail()) {
    return new EventList();
  }

  auto snapshot = new DirTree(ifs);
  auto now = getDirTree(dir, ignore);
  return now->getChanges(snapshot);
}
