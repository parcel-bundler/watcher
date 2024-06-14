#ifndef FANOTIFY_FS_H
#define FANOTIFY_FS_H

#include <unordered_map>

#include "FAnotifyBackend.hh"
#include "../Signal.hh"

class FAnotifyFsBackend : public BruteForceBackend {
public:
  void start() override;
  ~FAnotifyFsBackend();
  void subscribe(Watcher &watcher) override;
  void unsubscribe(Watcher &watcher) override;
private:
};

#endif
