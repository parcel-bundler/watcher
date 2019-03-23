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
  Watcher *watcher;
  EventList events;
};

Watcher::Watcher(std::string dir, std::unordered_set<std::string> ignore) 
  : mDir(dir),
    mIgnore(ignore),
    mWatched(false),
    mTree(NULL),
    mCallingCallbacks(false) {
      mDebounce = Debounce::getShared();
      mDebounce->add([this] () {
        if (mCallbacks.size() > 0) {
          EventData *data = new EventData();
          data->watcher = this;
          data->events = mEvents;
          mAsync.data = (void *)data;
          uv_async_send(&mAsync);

          mEvents.clear();
        }
      });
    }

void Watcher::wait() {
  std::unique_lock<std::mutex> lk(mMutex);
  mCond.wait(lk);
}

void Watcher::notify() {
  std::unique_lock<std::mutex> lk(mMutex);
  mCond.notify_all();

  mDebounce->trigger();
  // if (mCallbacks.size() > 0) {
  //   EventData *data = new EventData();
  //   data->watcher = this;
  //   data->events = mEvents;
  //   mAsync.data = (void *)data;
  //   uv_async_send(&mAsync);

  //   mEvents.clear();
  // }
}

void Watcher::fireCallbacks(uv_async_t *handle) {
  EventData *data = (EventData *)handle->data;
  Watcher *watcher = data->watcher;
  watcher->mCallingCallbacks = true;

  watcher->mCallbacksIterator = watcher->mCallbacks.begin();
  while (watcher->mCallbacksIterator != watcher->mCallbacks.end()) {
    auto it = watcher->mCallbacksIterator;
    HandleScope scope(it->Env());
    it->Call(std::initializer_list<napi_value>{data->events.toJS(it->Env())});

    // If the iterator was changed, then the callback trigged an unwatch.
    // The iterator will have been set to the next valid callback.
    // If it is the same as before, increment it.
    if (watcher->mCallbacksIterator == it) {
      watcher->mCallbacksIterator++;
    }
  }

  watcher->mCallingCallbacks = false;
  if (watcher->mCallbacks.size() == 0) {
    watcher->unref();
  }

  delete data;
}

bool Watcher::watch(Function callback) {
  std::unique_lock<std::mutex> lk(mMutex);
  auto res = mCallbacks.insert(Persistent(callback));
  if (res.second && mCallbacks.size() == 1) {
    uv_async_init(uv_default_loop(), &mAsync, Watcher::fireCallbacks);
    mWatched = true;
    return true;
  }

  return false;
}

bool Watcher::unwatch(Function callback) {
  std::unique_lock<std::mutex> lk(mMutex);

  bool removed = false;
  for (auto it = mCallbacks.begin(); it != mCallbacks.end(); it++) {
    if (it->Value() == callback) {
      mCallbacksIterator = mCallbacks.erase(it);
      removed = true;
      break;
    }
  }
  
  if (removed && mCallbacks.size() == 0) {
    unref();
    return true;
  }

  return false;
}

void Watcher::unref() {
  if (mCallbacks.size() == 0 && !mCallingCallbacks) {
    if (mWatched) {
      uv_close(reinterpret_cast<uv_handle_t*>(&mAsync), nullptr);
    }

    removeShared(this);
  }
}
