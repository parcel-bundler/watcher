#include "DirTree.hh"

static std::mutex mDirCacheMutex;
static std::unordered_map<std::string, std::weak_ptr<DirTree>> dirTreeCache;

struct DirTreeDeleter {
  void operator()(DirTree *tree) {
    std::lock_guard<std::mutex> lock(mDirCacheMutex);
    dirTreeCache.erase(tree->root);
    delete tree;
  }
};

bool hasMore(std::istream &stream) {
  while (stream.peek() == ' ') stream.get();
  return stream.peek() != '\n';
}

std::shared_ptr<DirTree> DirTree::getCached(std::string root) {
  std::lock_guard<std::mutex> lock(mDirCacheMutex);

  auto found = dirTreeCache.find(root);
  std::shared_ptr<DirTree> tree;

  // Use cached tree, or create an empty one.
  if (found != dirTreeCache.end()) {
    tree = found->second.lock();
  } else {
    tree = std::shared_ptr<DirTree>(new DirTree(root), DirTreeDeleter());
    dirTreeCache.emplace(root, tree);
  }

  return tree;
}

DirTree::DirTree(std::string root, std::istream &stream) : root(root), isComplete(true) {
  size_t size;
  if (stream >> size) {
    for (size_t i = 0; i < size; i++) {
      DirEntry entry(stream);
      entries.emplace(entry.path, entry);
    }
  }
}

// Internal find method that has no lock
DirEntry *DirTree::_find(std::string path) {
  auto found = entries.find(path);
  if (found == entries.end()) {
    return NULL;
  }

  return &found->second;
}

DirEntry *DirTree::add(std::string path, ino_t ino, uint64_t mtime, Kind kind, std::string fileId) {
  std::lock_guard<std::mutex> lock(mMutex);

  DirEntry entry(path, ino, mtime, kind, fileId);
  auto it = entries.emplace(entry.path, entry);
  return &it.first->second;
}

DirEntry *DirTree::find(std::string path) {
  std::lock_guard<std::mutex> lock(mMutex);
  return _find(path);
}

DirEntry *DirTree::update(std::string path, ino_t ino, uint64_t mtime, std::string fileId) {
  std::lock_guard<std::mutex> lock(mMutex);

  DirEntry *found = _find(path);
  if (found) {
    found->mtime = mtime;

    if (ino != FAKE_INO) {
      found->ino = ino;
    }
    if (fileId != FAKE_FILEID) {
      found->fileId = fileId;
    }
  }

  return found;
}

void DirTree::remove(std::string path) {
  std::lock_guard<std::mutex> lock(mMutex);

  DirEntry *found = _find(path);

  // Remove all sub-entries if this is a directory
  if (found && found->kind == IS_DIR) {
    std::string pathStart = path + DIR_SEP;
    for (auto it = entries.begin(); it != entries.end();) {
      if (it->first.rfind(pathStart, 0) == 0) {
        it = entries.erase(it);
      } else {
        it++;
      }
    }
  }

  entries.erase(path);
}

void DirTree::write(std::ostream &stream) {
  std::lock_guard<std::mutex> lock(mMutex);

  stream << entries.size() << "\n";
  for (auto it = entries.begin(); it != entries.end(); it++) {
    it->second.write(stream);
  }
}

void DirTree::getChanges(DirTree *snapshot, EventList &events) {
  std::lock_guard<std::mutex> lock(mMutex);
  std::lock_guard<std::mutex> snapshotLock(snapshot->mMutex);

  for (auto it = snapshot->entries.begin(); it != snapshot->entries.end(); it++) {
    size_t count = entries.count(it->first);
    if (count == 0) {
      events.remove(it->second.path, it->second.kind, it->second.ino, it->second.fileId);
    }
  }

  for (auto it = entries.begin(); it != entries.end(); it++) {
    auto found = snapshot->entries.find(it->first);
    if (found == snapshot->entries.end()) {
      events.create(it->second.path, it->second.kind, it->second.ino, it->second.fileId);
    } else if (found->second.mtime != it->second.mtime && found->second.kind != IS_DIR && it->second.kind != IS_DIR) {
      events.update(it->second.path, it->second.ino, it->second.fileId);
    }
  }
}

DirEntry::DirEntry(std::string p, ino_t i, uint64_t t, Kind k, std::string f) {
  path = p;
  ino = i;
  mtime = t;
  kind = k;
  state = NULL;
  fileId = f;
}

DirEntry::DirEntry(std::istream &stream) {
  size_t size;

  if (stream >> size) {
    path.resize(size);
    if (stream.read(&path[0], size)) {
      stream >> mtime;
      stream >> kind;

      // XXX: works because the default ino is '0' and thus will never be an
      // empty char.
      if (hasMore(stream)) stream >> ino;
      if (hasMore(stream)) stream >> fileId;
    }
  }
}

void DirEntry::write(std::ostream &stream) const {
  stream << path.size() << path << mtime << " " << kind << " " << ino << " " << fileId << " " << "\n";
}
