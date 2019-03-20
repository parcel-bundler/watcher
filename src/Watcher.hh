#ifndef WATCHER_H
#define WATCHER_H

#include <condition_variable>
#include <unordered_set>
#include <set>
#include <uv.h>
#include <node_api.h>
#include "Event.hh"

using namespace Napi;

struct Watcher {
  std::string mDir;
  std::unordered_set<std::string> mIgnore;
  EventList mEvents;
  void *state;
  bool mWatched;

  Watcher(std::string dir, std::unordered_set<std::string> ignore);

  bool operator==(const Watcher &other) const {
    return mDir == other.mDir;
  }

  void wait();
  void notify();
  bool watch(Function callback);
  bool unwatch(Function callback);
  void unref();

  static std::shared_ptr<Watcher> getShared(std::string dir, std::unordered_set<std::string> ignore);

private:
  std::mutex mMutex;
  std::condition_variable mCond;
  uv_async_t mAsync;
  std::set<FunctionReference> mCallbacks;
  std::set<FunctionReference>::iterator mCallbacksIterator;

  static void fireCallbacks(uv_async_t *handle);
};

#endif
