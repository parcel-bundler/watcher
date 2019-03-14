#include <string>
#include <ftw.h>
#include "../DirTree.hh"

static DirTree *tree;
int addEntry(const char *path, const struct stat *info, const int flags, struct FTW *ftw) {
  tree->entries.insert(DirEntry(path, info->st_mtime));
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
