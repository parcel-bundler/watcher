#ifndef WATCHER_H
#define WATCHER_H

#include <condition_variable>
#include <unordered_set>
#include <uv.h>
#include <node_api.h>
#include "Event.hh"

using namespace Napi;

struct Watcher {
  std::string mDir;
  std::unordered_set<std::string> mIgnore;
  EventList mEvents;
  void *state;

  Watcher() {}
  Watcher(std::string dir, std::unordered_set<std::string> ignore) : mDir(dir), mIgnore(ignore) {}

  bool operator==(const Watcher &other) const {
    return mDir == other.mDir;
  }

  void wait();
  void notify();
  void watch(Function callback);
  void unwatch();

  static std::shared_ptr<Watcher> getShared(std::string dir, std::unordered_set<std::string> ignore);

private:
  std::mutex mMutex;
  std::condition_variable mCond;
  uv_async_t mAsync;
  bool mWatching;
  std::vector<FunctionReference> mCallbacks;
};

#endif
