#include <string>
#include <fts.h>
#include "../DirTree.hh"

DirTree *getDirTree(std::string *dir, std::unordered_set<std::string> *ignore) {
  DirTree *tree = new DirTree();

  char *paths[2] {(char *)dir->c_str(), NULL};
  FTS *fts = fts_open(paths, FTS_NOCHDIR | FTS_PHYSICAL, NULL);
  FTSENT *node;

  while ((node = fts_read(fts)) != NULL) {
    if (ignore->count(std::string(node->fts_path)) > 0) {
      fts_set(fts, node, FTS_SKIP);
      continue;
    }

    tree->entries.insert(DirEntry(node->fts_path, node->fts_statp->st_mtime));
  }

  fts_close(fts);
  return tree;
}
