#include <sys/stat.h>
#include <string>
#include <stdlib.h>
#include <fts.h>
#include "../DirTree.hh"
#include "../shared/BruteForceBackend.hh"

DirTree *BruteForceBackend::readTree(Watcher &watcher) {
  DirTree *tree = new DirTree();

  char *paths[2] {(char *)watcher.mDir.c_str(), NULL};
  FTS *fts = fts_open(paths, FTS_NOCHDIR | FTS_PHYSICAL, NULL);
  FTSENT *node;

  while ((node = fts_read(fts)) != NULL) {
    if (watcher.mIgnore.count(std::string(node->fts_path)) > 0) {
      fts_set(fts, node, FTS_SKIP);
      continue;
    }

    tree->add(node->fts_path, node->fts_statp->st_mtime, (node->fts_info & FTS_D) == FTS_D);
  }

  fts_close(fts);
  return tree;
}
