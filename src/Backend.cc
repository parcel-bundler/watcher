#ifdef FS_EVENTS
#include "macos/FSEvents.hh"
#endif
#ifdef WATCHMAN
#include "watchman/watchman.hh"
#endif
#ifdef WINDOWS
#include "windows/WindowsBackend.hh"
#endif
#ifdef INOTIFY
#include "linux/InotifyBackend.hh"
#endif
#include "shared/BruteForceBackend.hh"

#include "Backend.hh"
#include <unordered_map>

static std::unordered_map<std::string, std::shared_ptr<Backend>> sharedBackends;

std::shared_ptr<Backend> getBackend(std::string backend) {
  printf("getting backend\n");
  fflush(stdout);
  // Use FSEvents on macOS by default.
  // Use watchman by default if available on other platforms.
  // Fall back to brute force.
  #ifdef FS_EVENTS
    if (backend == "fs-events" || backend == "default") {
      return std::make_shared<FSEventsBackend>();
    }
  #endif
  #ifdef WATCHMAN
    printf("checking watchman\n");
    if ((backend == "watchman" || backend == "default") && WatchmanBackend::checkAvailable()) {
      printf("got watchman\n");
      return std::make_shared<WatchmanBackend>();
    }
  #endif
  #ifdef WINDOWS
    if (backend == "windows" || backend == "default") {
      return std::make_shared<WindowsBackend>();
    }
  #endif
  #ifdef INOTIFY
    if (backend == "inotify" || backend == "default") {
      return std::make_shared<InotifyBackend>();
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

  result->run();
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

void Backend::run() {
  mThread = std::thread([this] () {
    start();
  });

  if (mThread.joinable()) {
    mStartedSignal.wait();
  }
}

void Backend::notifyStarted() {
  mStartedSignal.notify();
}

void Backend::start() {
  notifyStarted();
}

Backend::~Backend() {
  std::unique_lock<std::mutex> lock(mMutex);

  // Unwatch all subscriptions so that their state gets cleaned up
  for (auto it = mSubscriptions.begin(); it != mSubscriptions.end(); it++) {
    unwatch(**it);
  }

  // Wait for thread to stop
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
