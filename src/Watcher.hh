#ifndef WATCHER_H
#define WATCHER_H

#include <condition_variable>
#include <unordered_set>
#include <uv.h>
#include "Event.hh"

struct Watcher {
  std::string mDir;
  std::unordered_set<std::string> mIgnore;
  EventList mEvents;
  void *state;

  void wait();
  void notify();
  void watch(std::function<void(EventList&)> callback);
  void unwatch();

private:
  std::mutex mMutex;
  std::condition_variable mCond;
  uv_async_t mAsync;
  bool mWatching;
  std::function<void(EventList&)> mCallback;
};

#endif
