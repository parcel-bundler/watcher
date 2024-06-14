#include <memory>

#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/fanotify.h>

#include "FAnotifyFsBackend.hh"

void FAnotifyFsBackend::start() {
  throw std::runtime_error("not implemented");
}

FAnotifyFsBackend::~FAnotifyFsBackend() {
}

// This function is called by Backend::watch which takes a lock on mMutex
void FAnotifyFsBackend::subscribe(WatcherRef watcher) {
  throw std::runtime_error("not implemented");
}

// This function is called by Backend::unwatch which takes a lock on mMutex
void FAnotifyFsBackend::unsubscribe(WatcherRef watcher) {
  throw std::runtime_error("not implemented");
}
