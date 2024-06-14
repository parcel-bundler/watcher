#ifndef FANOTIFY_H
#define FANOTIFY_H

#include <sys/fanotify.h>

#include "../shared/BruteForceBackend.hh"
#include "../DirTree.hh"

struct FAnotifySubscription {
  std::shared_ptr<DirTree> tree;
  std::string path;
  Watcher* watcher;
  int mountFd;
};

constexpr const uint64_t FANOTIFY_MASK = FAN_CREATE | FAN_DELETE | FAN_MODIFY | FAN_RENAME | FAN_EVENT_ON_CHILD | FAN_ATTRIB | FAN_DELETE_SELF | FAN_MOVED_FROM | FAN_MOVED_TO | FAN_MOVE_SELF | FAN_ONDIR;

class FAnotifyBackend : public BruteForceBackend {
protected:
  static std::string toHex(const char* data, size_t len) {
    static char abc[] = "0123456789abcdef";
    std::string result;

    for (size_t i = 0; i < len;++i) {
      result.append(1, abc[data[i] >> 8]);
      result.append(1, abc[data[i] & 0x0f]);
    }

    return result;
  }

  static std::string toString(fsid_t* fsid) {
    return toHex(reinterpret_cast<char*>(fsid), sizeof(fsid_t));
  }
};

#endif
