#ifndef DIR_TREE_H
#define DIR_TREE_H

#include <string>
#include <unordered_set>
#include <ostream>
#include <istream>
#include "Event.hh"

struct DirEntry {
  std::string path;
  time_t mtime;

  DirEntry(std::string p, time_t t) {
    path = p;
    mtime = t;
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
  std::unordered_set<DirEntry> entries;
  DirTree() {}

  DirTree(std::istream &stream) {
    size_t size;
    if (stream >> size) {
      for (size_t i = 0; i < size; i++) {
        entries.insert(DirEntry(stream));
      }
    }
  }

  void write(std::ostream &stream) {
    stream << entries.size() << "\n";
    for (auto it = entries.begin(); it != entries.end(); it++) {
      it->write(stream);
    }
  }

  EventList *getChanges(DirTree *snapshot) {
    EventList *events = new EventList();
    for (auto it = entries.begin(); it != entries.end(); it++) {
      auto found = snapshot->entries.find(*it);
      if (found == snapshot->entries.end()) {
        events->push(it->path, "create");
      } else if (found->mtime != it->mtime) {
        events->push(it->path, "update");
      }
    }

    for (auto it = snapshot->entries.begin(); it != snapshot->entries.end(); it++) {
      size_t count = entries.count(*it);
      if (count == 0) {
        events->push(it->path, "delete");
      }
    }

    return events;
  }
};

#endif
