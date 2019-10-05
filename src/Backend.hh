#ifndef BACKEND_H
#define BACKEND_H

#include "Event.hh"
#include "Watcher.hh"
#include "Signal.hh"
#include "DirTree.hh"
#include <thread>

class Backend {
public:
  virtual ~Backend();
  void run();
  void notifyStarted();

  virtual void start();
  virtual void writeSnapshot(Watcher &watcher, std::string *snapshotPath) = 0;
  virtual void getEventsSince(Watcher &watcher, std::string *snapshotPath) = 0;
  virtual void subscribe(Watcher &watcher) = 0;
  virtual void unsubscribe(Watcher &watcher) = 0;
  virtual void readTree(Watcher &watcher, std::shared_ptr<DirTree> tree);

  static std::shared_ptr<Backend> getShared(std::string backend);

  void watch(Watcher &watcher);
  void unwatch(Watcher &watcher);
  void unref();
  void handleWatcherError(WatcherError &err);
  std::shared_ptr<DirTree> getTree(Watcher &watcher, bool shouldRead = true);

  std::mutex mMutex;
  std::thread mThread;
private:
  std::unordered_set<Watcher *> mSubscriptions;
  Signal mStartedSignal;

  void handleError(std::exception &err);
};

#endif
