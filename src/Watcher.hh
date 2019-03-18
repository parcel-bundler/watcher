#ifndef WATCHER_H
#define WATCHER_H

#include <condition_variable>
#include <unordered_set>
#include <uv.h>
#include "Event.hh"

struct EventData {
  std::function<void(EventList&)> mCallback;
  EventList mEvents;
};

struct Watcher {
  std::string mDir;
  std::unordered_set<std::string> mIgnore;
  EventList mEvents;
  void *state;

  void wait() {
    std::unique_lock<std::mutex> lk(mMutex);
    mCond.wait(lk);
  }

  void notify() {
    mCond.notify_all();

    if (mWatching) {
      EventData *data = new EventData;
      data->mCallback = mCallback;
      data->mEvents = mEvents;
      mAsync.data = (void *)data;
      uv_async_send(&mAsync);
    }

    mEvents.clear();
  }

  void watch(std::function<void(EventList&)> callback) {
    mWatching = true;
    mCallback = callback;
    uv_async_init(uv_default_loop(), &mAsync, Watcher::fireCallback);
  }

  void unwatch() {
    mWatching = false;
    uv_close(reinterpret_cast<uv_handle_t*>(&mAsync), nullptr);
  }

private:
  std::mutex mMutex;
  std::condition_variable mCond;
  uv_async_t mAsync;
  bool mWatching;
  std::function<void(EventList&)> mCallback;

  static void fireCallback(uv_async_t *handle) {
    EventData *data = (EventData *)handle->data;
    data->mCallback(data->mEvents);
    delete data;
  }
};

#endif
