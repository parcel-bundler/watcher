#ifndef FANOTIFY_FS_H
#define FANOTIFY_FS_H

#include <unordered_map>

#include "FAnotifyBackend.hh"
#include "../Signal.hh"

class FAnotifyFsBackend : public BruteForceBackend {
public:
  void start() override;
  ~FAnotifyFsBackend() override;
  void subscribe(WatcherRef watcher) override;
  void unsubscribe(WatcherRef watcher) override;
private:
};

#endif
