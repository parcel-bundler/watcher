#ifndef DIR_TREE_H
#define DIR_TREE_H

#include <string>
#include <unordered_map>
#include <ostream>
#include <istream>
#include <memory>
#include "const.hh"
#include "Event.hh"

#ifdef _WIN32
#define DIR_SEP "\\"
#else
#define DIR_SEP "/"
#endif

struct DirEntry {
  std::string path;
  ino_t ino;
  std::string fileId;
  uint64_t mtime;
  Kind kind;
  mutable void *state;

  DirEntry(std::string p, ino_t i, uint64_t t, Kind k, std::string fileId = FAKE_FILEID);
  DirEntry(std::istream &stream);
  void write(std::ostream &stream) const;
  bool operator==(const DirEntry &other) const {
    return path == other.path;
  }
};

class DirTree {
public:
  static std::shared_ptr<DirTree> getCached(std::string root, bool recursiveRemove = true);
  DirTree(std::string root, bool recursiveRemove = true) : root(root), isComplete(false), recursiveRemove(recursiveRemove) {}
  DirTree(std::string root, std::istream &stream, bool recursiveRemove = true);
  DirEntry *add(std::string path, ino_t ino, uint64_t mtime, Kind kind, std::string fileId = FAKE_FILEID);
  DirEntry *find(std::string path);
  DirEntry *update(std::string path, ino_t ino, uint64_t mtime, std::string fileId = FAKE_FILEID);
  void remove(std::string path);
  void write(std::ostream &stream);
  void getChanges(DirTree *snapshot, EventList &events);

  std::mutex mMutex;
  std::string root;
  bool isComplete;
  std::map<std::string, DirEntry> entries;

private:
  bool recursiveRemove;
  DirEntry *_find(std::string path);
  DirEntry *findByIno(ino_t ino);
  DirEntry *findByFileId(std::string fileId);
};

#endif
