#ifndef BACKEND_H
#define BACKEND_H

#include "Event.hh"
#include "Watcher.hh"
#include <thread>

class Backend {
public:
  Backend();
  virtual ~Backend();

  virtual void start();
  virtual void writeSnapshot(Watcher &watcher, std::string *snapshotPath) = 0;
  virtual void getEventsSince(Watcher &watcher, std::string *snapshotPath) = 0;
  virtual void subscribe(Watcher &watcher) = 0;
  virtual void unsubscribe(Watcher &watcher) = 0;

  static std::shared_ptr<Backend> getShared(std::string backend);

  void watch(Watcher &watcher);
  void unwatch(Watcher &watcher);
  void unref();

  std::mutex mMutex;
private:
  std::thread mThread;
  std::unordered_set<Watcher *> mSubscriptions;
};

#endif
