#ifndef BACKEND_H
#define BACKEND_H

#include "Event.hh"
#include "Watcher.hh"
#include <thread>

class Backend {
public:
  Backend() {
    mMutex.lock();
    mThread = std::thread([this] () {
      this->start();
    });
  }

  virtual ~Backend() {
    if (mThread.joinable()) {
      mThread.join();
    }
  }

  virtual void start() {}
  virtual void writeSnapshot(Watcher &watcher, std::string *snapshotPath) = 0;
  virtual void getEventsSince(Watcher &watcher, std::string *snapshotPath) = 0;
  virtual void subscribe(Watcher &watcher) = 0;
  virtual void unsubscribe(Watcher &watcher) = 0;

  static std::shared_ptr<Backend> getShared(std::string backend);

  std::mutex mMutex;
private:
  std::thread mThread;
};

#ifdef WATCHMAN
struct WatchmanBackend : public Backend {
  static bool check();
  void writeSnapshot(Watcher &watcher, std::string *snapshotPath) override;
  void getEventsSince(Watcher &watcher, std::string *snapshotPath) override;
};
#endif

// struct BruteForceBackend : public Backend {
//   void writeSnapshot(Watcher &watcher, std::string *snapshotPath) override;
//   void getEventsSince(Watcher &watcher, std::string *snapshotPath) override;
// };

#endif
