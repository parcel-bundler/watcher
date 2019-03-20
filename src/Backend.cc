#ifdef FS_EVENTS
#include "macos/FSEvents.hh"
#endif
#ifdef WATCHMAN
#include "watchman/watchman.hh"
#endif
#include "shared/BruteForceBackend.hh"

#include "Backend.hh"
#include <unordered_map>

static std::unordered_map<std::string, std::shared_ptr<Backend>> sharedBackends;

std::shared_ptr<Backend> getBackend(std::string backend) {
  // Use FSEvents on macOS by default.
  // Use watchman by default if available on other platforms.
  // Fall back to brute force.
  #ifdef FS_EVENTS
    if (backend == "fs-events" || backend == "default") {
      return std::make_shared<FSEventsBackend>();
    }
  #endif
  #ifdef WATCHMAN
    if ((backend == "watchman" || backend == "default") && WatchmanBackend::check()) {
      return std::make_shared<WatchmanBackend>();
    }
  #endif
  if (backend == "brute-force" || backend == "default") {
    return std::make_shared<BruteForceBackend>();
  }

  return nullptr;
}

std::shared_ptr<Backend> Backend::getShared(std::string backend) {
  auto found = sharedBackends.find(backend);
  if (found != sharedBackends.end()) {
    return found->second;
  }

  auto result = getBackend(backend);
  if (!result) {
    return getShared("default");
  }

  sharedBackends.emplace(backend, result);
  return result;
}

void removeShared(Backend *backend) {
  for (auto it = sharedBackends.begin(); it != sharedBackends.end(); it++) {
    if (it->second.get() == backend) {
      sharedBackends.erase(it);
      break;
    }
  }
}

Backend::Backend() {
  mMutex.lock();
  mThread = std::thread([this] () {
    this->start();
  });
}

void Backend::start() {
  // Default implementation if not overridden needs to unlock the mutex.
  mMutex.unlock();
}

Backend::~Backend() {
  std::unique_lock<std::mutex> lock(mMutex);
  if (mThread.joinable()) {
    mThread.join();
  }
}

void Backend::watch(Watcher &watcher) {
  std::unique_lock<std::mutex> lock(mMutex);
  auto res = mSubscriptions.insert(&watcher);
  if (res.second) {
    this->subscribe(watcher);
  }
}

void Backend::unwatch(Watcher &watcher) {
  std::unique_lock<std::mutex> lock(mMutex);
  size_t deleted = mSubscriptions.erase(&watcher);
  if (deleted > 0) {
    this->unsubscribe(watcher);
    unref();
  }
}

void Backend::unref() {
  if (mSubscriptions.size() == 0) {
    removeShared(this);
  }
}
