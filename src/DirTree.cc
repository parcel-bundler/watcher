#include "DirTree.hh"
#include <cstdio>

static std::unordered_map<std::string, std::weak_ptr<DirTree>> dirTreeCache;

struct DirTreeDeleter {
  void operator()(DirTree *tree) {
    dirTreeCache.erase(tree->root);
    delete tree;
  }
};

std::shared_ptr<DirTree> DirTree::getCached(std::string root) {
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

DirEntry *DirTree::add(std::string path, uint64_t mtime, bool isDir, uint64_t size) {
  DirEntry entry(path, mtime, isDir, size);
  auto it = entries.emplace(entry.path, entry);
  return &it.first->second;
}

DirEntry *DirTree::find(std::string path) {
  auto found = entries.find(path);
  if (found == entries.end()) {
    return NULL;
  }

  return &found->second;
}

DirEntry *DirTree::update(std::string path, uint64_t mtime) {
  DirEntry *found = find(path);
  if (found) {
    found->mtime = mtime;
  }

  return found;
}

void DirTree::remove(std::string path) {
  DirEntry *found = find(path);

  // Remove all sub-entries if this is a directory
  if (found && found->isDir) {
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
  stream << entries.size() << "\n";
  for (auto it = entries.begin(); it != entries.end(); it++) {
    it->second.write(stream);
  }
}

void DirTree::getChanges(DirTree *snapshot, EventList &events) {
  for (auto it = entries.begin(); it != entries.end(); it++) {
    auto found = snapshot->entries.find(it->first);
    if (found == snapshot->entries.end()) {
      events.create(it->second.path);
    } else if (!found->second.isDir && !it->second.isDir && !found->second.compare(it->second)) {
      events.update(it->second.path);
    }
  }

  for (auto it = snapshot->entries.begin(); it != snapshot->entries.end(); it++) {
    size_t count = entries.count(it->first);
    if (count == 0) {
      events.remove(it->second.path);
    }
  }
}

DirEntry::DirEntry(std::string p, uint64_t t, bool d, uint64_t s) {
  path = p;
  mtime = t;
  isDir = d;
  size = s;
  hash = 0;
  state = NULL;
}

DirEntry::DirEntry(std::istream &stream) {
  size_t numEntries;

  if (stream >> numEntries) {
    path.resize(numEntries);
    if (stream.read(&path[0], numEntries)) {
      stream >> mtime;
      stream >> isDir;
      stream >> size;
      stream >> hash;
    }
  }
}

void DirEntry::write(std::ostream &stream) {
  stream << path.size() << path << mtime << " " << isDir << " " << size << " " << getHash() << "\n";
}

bool DirEntry::compare(DirEntry &other) {
  if (size != other.size) {
    return false;
  }

  if (mtime == other.mtime) {
    other.hash = hash;
    return true;
  }

  return getHash() == other.getHash();
}

#define BUFLEN 16384

inline XXH64_hash_t readHash(std::string path) {
  XXH64_state_t* const state = XXH64_createState();
  char buffer[BUFLEN];

  XXH64_hash_t const seed = 0;
  XXH64_reset(state, seed);

  FILE* file = fopen(path.c_str(), "rb");
  if (file == NULL) {
    return 0;
  }
  
  int len;
  while ((len = fread(buffer, 1, BUFLEN, file)) > 0) {
    XXH64_update(state, buffer, len);
  }

  fclose(file);

  XXH64_hash_t const hash = XXH64_digest(state);
  XXH64_freeState(state);

  return hash;
}

XXH64_hash_t DirEntry::getHash() {
  if (isDir) {
    return 0;
  }

  if (hash == 0) {
    hash = readHash(path);
  }

  return hash;
}
