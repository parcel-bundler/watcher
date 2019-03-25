#ifndef INOTIFY_H
#define INOTIFY_H

#include <unordered_map>
#include <sys/inotify.h>
#include "../shared/BruteForceBackend.hh"
#include "../DirTree.hh"
#include "../Signal.hh"

class InotifyBackend : public BruteForceBackend {
public:
  void start() override;
  ~InotifyBackend();
  void subscribe(Watcher &watcher) override;
  void unsubscribe(Watcher &watcher) override;
private:
  int mPipe[2];
  int mInotify;
  std::unordered_map<int, DirEntry *> mSubscriptions;
  bool mEnded;
  Signal mEndedSignal;

  void watchDir(Watcher &watcher, DirEntry *entry);
  void handleEvents();
  Watcher *handleEvent(struct inotify_event *event);
};

#endif
