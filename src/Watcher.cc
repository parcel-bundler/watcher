#include "Watcher.hh"
#include <unordered_set>

using namespace Napi;

struct WatcherHash {
  std::size_t operator() (std::shared_ptr<Watcher> const &k) const {
    return std::hash<std::string>()(k->mDir);
  }
};

struct WatcherCompare {
  size_t operator() (std::shared_ptr<Watcher> const &a, std::shared_ptr<Watcher> const &b) const {
    return *a == *b;
  }
};

static std::unordered_set<std::shared_ptr<Watcher>, WatcherHash, WatcherCompare> sharedWatchers;

std::shared_ptr<Watcher> Watcher::getShared(std::string dir, std::unordered_set<std::string> ignore) {
  std::shared_ptr<Watcher> watcher = std::make_shared<Watcher>(dir, ignore);
  auto found = sharedWatchers.find(watcher);
  if (found != sharedWatchers.end()) {
    return *found;
  }

  sharedWatchers.insert(watcher);
  return watcher;
}

void removeShared(Watcher *watcher) {
  for (auto it = sharedWatchers.begin(); it != sharedWatchers.end(); it++) {
    if (it->get() == watcher) {
      sharedWatchers.erase(it);
      break;
    }
  }
}

struct EventData {
  std::vector<FunctionReference> *mCallbacks;
  EventList mEvents;
};

void Watcher::wait() {
  std::unique_lock<std::mutex> lk(mMutex);
  mCond.wait(lk);
}

void Watcher::notify() {
  std::unique_lock<std::mutex> lk(mMutex);
  mCond.notify_all();

  if (mWatching) {
    EventData *data = new EventData();
    data->mCallbacks = &mCallbacks;
    data->mEvents = mEvents;
    mAsync.data = (void *)data;
    uv_async_send(&mAsync);
  }

  mEvents.clear();
}

void fireCallbacks(uv_async_t *handle) {
  EventData *data = (EventData *)handle->data;
  auto end = data->mCallbacks->end();
  for (auto it = data->mCallbacks->begin(); it != end; it++) {
    HandleScope scope(it->Env());
    it->Call(std::initializer_list<napi_value>{data->mEvents.toJS(it->Env())});
  }

  delete data;
}

void Watcher::watch(Function callback) {
  std::unique_lock<std::mutex> lk(mMutex);
  mWatching = true;
  mCallbacks.push_back(Persistent(callback));
  uv_async_init(uv_default_loop(), &mAsync, fireCallbacks);
}

void Watcher::unwatch() {
  std::unique_lock<std::mutex> lk(mMutex);
  mWatching = false;
  uv_close(reinterpret_cast<uv_handle_t*>(&mAsync), nullptr);
  removeShared(this);
}
