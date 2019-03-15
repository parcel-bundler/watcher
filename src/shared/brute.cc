#include <string>
#include <fstream>
#include "../DirTree.hh"
#include "../Event.hh"

DirTree *getDirTree(std::string *dir);

void writeSnapshotImpl(std::string *dir, std::string *snapshotPath) {
  auto tree = getDirTree(dir);
  std::ofstream ofs(*snapshotPath);
  tree->write(ofs);
}

EventList *getEventsSinceImpl(std::string *dir, std::string *snapshotPath) {
  std::ifstream ifs(*snapshotPath);
  if (ifs.fail()) {
    return new EventList();
  }

  auto snapshot = new DirTree(ifs);
  auto now = getDirTree(dir);
  return now->getChanges(snapshot);
}
