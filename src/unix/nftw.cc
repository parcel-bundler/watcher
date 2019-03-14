#include <string>
#include <sstream>
#include <ftw.h>
#include "../DirTree.hh"
#include "../Event.hh"

static DirTree *tree;
int addEntry(const char *path, const struct stat *info, const int flags, struct FTW *ftw) {
  tree->entries.insert(DirEntry(path, info));
  return 0;
}

DirTree *getDirTree(std::string *dir) {
  tree = new DirTree();
  int result = nftw(dir->c_str(), &addEntry, 15, FTW_PHYS);
  if (result < 0) {
    printf("error\n");
  }

  return tree;
}

std::string getCurrentTokenImpl(std::string *dir) {
  auto tree = getDirTree(dir);
  std::ostringstream os;
  tree->write(os);
  return os.str();
}

EventList *getEventsSinceImpl(std::string *dir, std::string *token) {
  std::istringstream is(*token);
  auto snapshot = new DirTree(is);
  auto now = getDirTree(dir);

  return now->getChanges(snapshot);
}
