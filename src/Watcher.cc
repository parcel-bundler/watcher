#include "Watcher.hh"

// Watcher::Watcher() {

// }

struct EventData {
  std::function<void(EventList&)> mCallback;
  EventList mEvents;
};

void Watcher::wait() {
  std::unique_lock<std::mutex> lk(mMutex);
  mCond.wait(lk);
}

void Watcher::notify() {
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

void fireCallback(uv_async_t *handle) {
  EventData *data = (EventData *)handle->data;
  data->mCallback(data->mEvents);
  delete data;
}

void Watcher::watch(std::function<void(EventList&)> callback) {
  mWatching = true;
  mCallback = callback;
  uv_async_init(uv_default_loop(), &mAsync, fireCallback);
}

void Watcher::unwatch() {
  mWatching = false;
  uv_close(reinterpret_cast<uv_handle_t*>(&mAsync), nullptr);
}
