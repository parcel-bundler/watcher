#ifdef FS_EVENTS
#include "macos/FSEvents.hh"
#endif

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
  // if (backend == "brute-force" || backend == "default") {
  //   return std::make_shared<BruteForceBackend>();
  // }

  // return getBackend("default");
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
