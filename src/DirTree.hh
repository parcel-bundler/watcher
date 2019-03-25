#ifndef DIR_TREE_H
#define DIR_TREE_H

#include <string>
#include <unordered_map>
#include <ostream>
#include <istream>
#include "Event.hh"

struct DirEntry {
  std::string path;
  time_t mtime;
  bool isDir;
  mutable void *state;

  DirEntry(std::string p, time_t t, bool d) {
    path = p;
    mtime = t;
    isDir = d;
    state = NULL;
  }
  
  DirEntry(std::istream &stream) {
    size_t size;

    if (stream >> size) {
      path.resize(size);
      if (stream.read(&path[0], size)) {
        stream >> mtime;
      }
    }
  }

  bool operator==(const DirEntry &other) const {
    return path == other.path;
  }

  void write(std::ostream &stream) const {
    stream << path.size() << path << mtime << "\n";
  }
};

namespace std {
  template <>
  struct hash<DirEntry> {
    std::size_t operator()(const DirEntry& k) const {
      return hash<string>()(k.path);
    }
  };
}

struct DirTree {
  std::unordered_map<std::string, DirEntry> entries;
  DirTree() {}

  DirTree(std::istream &stream) {
    size_t size;
    if (stream >> size) {
      for (size_t i = 0; i < size; i++) {
        DirEntry entry(stream);
        entries.emplace(entry.path, entry);
      }
    }
  }

  DirEntry *add(std::string path, time_t mtime, bool isDir) {
    DirEntry entry(path, mtime, isDir);
    auto it = entries.emplace(entry.path, entry);
    return &it.first->second;
  }

  DirEntry *find(std::string path) {
    auto found = entries.find(path);
    if (found == entries.end()) {
      return NULL;
    }

    return &found->second;
  }

  DirEntry *update(std::string path, time_t mtime) {
    DirEntry *found = find(path);
    if (found) {
      found->mtime = mtime;
    }

    return found;
  }

  void remove(std::string path) {
    DirEntry *found = find(path);

    // Remove all sub-entries if this is a directory
    if (found && found->isDir) {
      std::string pathStart = path + "/";
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

  void write(std::ostream &stream) {
    stream << entries.size() << "\n";
    for (auto it = entries.begin(); it != entries.end(); it++) {
      it->second.write(stream);
    }
  }

  void getChanges(DirTree *snapshot, EventList &events) {
    for (auto it = entries.begin(); it != entries.end(); it++) {
      auto found = snapshot->entries.find(it->first);
      if (found == snapshot->entries.end()) {
        events.create(it->second.path);
      } else if (found->second.mtime != it->second.mtime) {
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
};

#endif
