#include <memory>
#include <tuple>

#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/fanotify.h>

#include "FAnotifyCrawlBackend.hh"

#ifdef FAN_REPORT_DIR_FID
#define FANOTIFY_EVENT_INFO_TYPE_FID FAN_EVENT_INFO_TYPE_DFID_NAME
#define FANOTIFY_EVENT_INFO_TYPE_FID_OLD FAN_EVENT_INFO_TYPE_OLD_DFID_NAME
#else
#define FANOTIFY_EVENT_INFO_TYPE_FID FAN_EVENT_INFO_TYPE_FID
#define FANOTIFY_EVENT_INFO_TYPE_FID_OLD FAN_EVENT_INFO_TYPE_FID
#endif

#define BUFFER_SIZE 8192
#define CONVERT_TIME(ts) ((uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec)

void FAnotifyCrawlBackend::start() {
  // Create a pipe that we will write to when we want to end the thread.
  int err = pipe2(mPipe, O_CLOEXEC | O_NONBLOCK);
  if (err == -1) {
    throw std::runtime_error(std::string("Unable to open pipe: ") + strerror(errno));
  }

  unsigned fanFlags = FAN_CLASS_NOTIF | FAN_REPORT_FID;

#ifdef FAN_REPORT_DIR_FID
  fanFlags |= FAN_REPORT_DIR_FID | FAN_REPORT_NAME;
#endif

  mFAnotifyFd = fanotify_init(fanFlags, O_RDONLY);
  if (mFAnotifyFd == -1) {
    throw std::runtime_error(std::string("Unable to initialize fanotify: ") + strerror(errno));
  }

  pollfd pollfds[2];
  pollfds[0].fd = mPipe[0];
  pollfds[0].events = POLLIN;
  pollfds[0].revents = 0;
  pollfds[1].fd = mFAnotifyFd;
  pollfds[1].events = POLLIN;
  pollfds[1].revents = 0;

  notifyStarted();

  // Loop until we get an event from the pipe.
  while (true) {
    int result = poll(pollfds, 2, 500);
    if (result < 0) {
      throw std::runtime_error(std::string("Unable to poll: ") + strerror(errno));
    }

    if (pollfds[0].revents) {
      break;
    }

    if (pollfds[1].revents) {
      handleEvents();
    }
  }

  close(mPipe[0]);
  close(mPipe[1]);
  close(mFAnotifyFd);

  mEndedSignal.notify();
}

FAnotifyCrawlBackend::~FAnotifyCrawlBackend() {
  std::ignore = write(mPipe[1], "X", 1);
  mEndedSignal.wait();
}

// This function is called by Backend::watch which takes a lock on mMutex
void FAnotifyCrawlBackend::subscribe(WatcherRef watcher) {
  // Build a full directory tree recursively, and watch each directory.
  std::shared_ptr<DirTree> tree = getTree(watcher);

  for (auto &e : tree->entries) {
    if (e.second.isDir) {
      bool success = watchDir(watcher, e.second.path, tree);
      if (!success) {
        throw WatcherError(std::string("watchDir on '") + e.second.path + "' failed: " + strerror(errno), watcher);
      }
    }
  }
}

bool FAnotifyCrawlBackend::watchDir(WatcherRef watcher, const std::string &path, std::shared_ptr<DirTree> tree) {
  auto markRc = fanotify_mark(mFAnotifyFd, FAN_MARK_ADD | FAN_MARK_DONT_FOLLOW | FAN_MARK_ONLYDIR, FANOTIFY_MASK, AT_FDCWD, path.c_str());

  if (markRc != 0) {
    return false;
  }

  std::shared_ptr<FAnotifySubscription> sub = std::make_shared<FAnotifySubscription>();
  sub->tree = tree;
  sub->path = path;
  sub->watcher = watcher;
  sub->mountFd = open(path.c_str(), O_RDONLY);

  struct statfs statFs;
  if (fstatfs(sub->mountFd, &statFs) < 0) {
    close(sub->mountFd);
    return false;
  }

  auto fsid = toString(&statFs.f_fsid);
  mSubscriptions.emplace(fsid, sub);

  return true;
}

void FAnotifyCrawlBackend::handleEvents() {
  char buf[BUFFER_SIZE];

  // Track all of the watchers that are touched so we can notify them at the end of the events.
  std::unordered_set<WatcherRef> watchers;

  while (true) {
    auto n = read(mFAnotifyFd, &buf, BUFFER_SIZE);
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }

      throw std::runtime_error(std::string("Error reading from fanotify: ") + strerror(errno));
    }

    if (n == 0) {
      break;
    }

    auto event = reinterpret_cast<fanotify_event_metadata *>(buf);
    while (FAN_EVENT_OK(event, n)) {
      if (event->vers != FANOTIFY_METADATA_VERSION) {
        throw std::runtime_error("fanotify: wrong version");
      }

      if (event->mask & FAN_Q_OVERFLOW) {
        continue;
      }

      auto eventLen = event->event_len;
      unsigned processedLen = event->metadata_len;

      fanotify_event_info_fid *fid = nullptr;
      fanotify_event_info_fid *fidTo = nullptr;

      while (eventLen > processedLen) {
        auto header = (fanotify_event_info_header *)((char *)event + processedLen);

        if (FANOTIFY_EVENT_INFO_TYPE_FID == header->info_type || FANOTIFY_EVENT_INFO_TYPE_FID_OLD == header->info_type) {
          fid = (fanotify_event_info_fid *)header;

          if (FANOTIFY_EVENT_INFO_TYPE_FID == header->info_type) {
            break;
          }
        }

#ifdef FAN_EVENT_INFO_TYPE_OLD_DFID_NAME
        if (FAN_EVENT_INFO_TYPE_NEW_DFID_NAME == header->info_type) {
          fidTo = (fanotify_event_info_fid *)header;

          if (fid != nullptr) {
            break;
          }
        }
#endif

        processedLen += header->len;
      }

      if (fid != nullptr) {
        handleEvent(event, fid, watchers, fidTo);
      }

      event = FAN_EVENT_NEXT(event, n);
    }
  }

  for (auto &w : watchers) {
    w->notify();
  }
}

void FAnotifyCrawlBackend::handleEvent(fanotify_event_metadata *metadata,
  fanotify_event_info_fid *fid,
  std::unordered_set<WatcherRef> &watchers,
  fanotify_event_info_fid *fidTo) {

  std::unique_lock<std::mutex> lock(mMutex);

  // Find the subscriptions for this watch descriptor
  auto range = mSubscriptions.equal_range(toString((fsid_t *)&fid->fsid));
  std::unordered_set<std::shared_ptr<FAnotifySubscription>> set;
  for (auto it = range.first; it != range.second; ++it) {
    set.insert(it->second);
  }

  std::string mountPathTo;

  if (fidTo != nullptr) {
    auto sTo = mSubscriptions.find(toString((fsid_t *)&fidTo->fsid));
    mountPathTo.assign(sTo->second->path);
  }

  for (auto &s : set) {
    if (handleSubscription(metadata, fid, s, fidTo, mountPathTo)) {
      watchers.insert(s->watcher);
    }
  }
}

bool FAnotifyCrawlBackend::handleSubscription(fanotify_event_metadata *metadata,
  fanotify_event_info_fid *fid,
  std::shared_ptr<FAnotifySubscription> sub,
  fanotify_event_info_fid *fidTo,
  const std::string &mountPathTo) {

  // Build full path and check if its in our ignore list.
  auto watcher = sub->watcher;
  std::string path(sub->path);
  std::string pathTo;
  bool isDir = (metadata->mask & FAN_ONDIR) == FAN_ONDIR;

#ifdef FAN_EVENT_INFO_TYPE_DFID_NAME
  if (FAN_EVENT_INFO_TYPE_DFID_NAME == fid->hdr.info_type || FAN_EVENT_INFO_TYPE_OLD_DFID_NAME == fid->hdr.info_type) {
    auto handle = (file_handle *)fid->handle;
    path.append("/").append((char *)handle + sizeof(file_handle) + handle->handle_bytes);
  }

  if (fidTo != nullptr) {
    auto handle = (file_handle *)fidTo->handle;
    pathTo.assign(mountPathTo).append("/").append((char *)handle + sizeof(file_handle) + handle->handle_bytes);
  }
#endif

  if (watcher->isIgnored(path)) {
    return false;
  }

  // If this is a create, check if it's a directory and start watching if it is.
  // In any case, keep the directory tree up to date.
  if (metadata->mask & (FAN_CREATE | FAN_MOVED_TO)) {
    watcher->mEvents.create(path, (metadata->mask & FAN_MOVED_TO) == FAN_MOVED_TO);

    struct stat st;
    // Use lstat to avoid resolving symbolic links that we cannot watch anyway
    // https://github.com/parcel-bundler/watcher/issues/76
    lstat(path.c_str(), &st);
    DirEntry *entry = sub->tree->add(path, CONVERT_TIME(st.st_mtim), S_ISDIR(st.st_mode));

    if (entry->isDir) {
      bool success = watchDir(watcher, path, sub->tree);
      if (!success) {
        sub->tree->remove(path);
        return false;
      }
    }
  }
  else if (metadata->mask & (FAN_MODIFY | FAN_ATTRIB)) {
    watcher->mEvents.update(path);

    struct stat st;
    stat(path.c_str(), &st);
    sub->tree->update(path, CONVERT_TIME(st.st_mtim));
  }
  else if (metadata->mask & (FAN_DELETE | FAN_DELETE_SELF | FAN_MOVED_FROM | FAN_MOVE_SELF)) {
    bool isSelfEvent = (metadata->mask & (FAN_DELETE_SELF | FAN_MOVE_SELF));
    // Ignore delete/move self events unless this is the recursive watch root
    if (isSelfEvent && path != watcher->mDir) {
      return false;
    }

    // If the entry being deleted/moved is a directory, remove it from the list of subscriptions
    if (isSelfEvent || isDir) {
      for (auto it = mSubscriptions.begin(); it != mSubscriptions.end();) {
        if (it->second->path == path) {
          it = mSubscriptions.erase(it);
        }
        else {
          ++it;
        }
      }
    }

    watcher->mEvents.remove(path, (metadata->mask & FAN_MOVED_FROM) == FAN_MOVED_FROM);
    sub->tree->remove(path);
  }
  else if (metadata->mask & (FAN_RENAME)) {
    watcher->mEvents.move(path, pathTo);
  }

  return true;
}

// This function is called by Backend::unwatch which takes a lock on mMutex
void FAnotifyCrawlBackend::unsubscribe(WatcherRef watcher) {
  // Find any subscriptions pointing to this watcher, and remove them.
  for (auto it = mSubscriptions.begin(); it != mSubscriptions.end();) {
    if (it->second->watcher == watcher) {
      if (mSubscriptions.count(it->first) == 1) {
        auto markRc = fanotify_mark(mFAnotifyFd, FAN_MARK_REMOVE, FANOTIFY_MASK, AT_FDCWD, it->second->path.c_str());
        if (markRc != 0) {
          throw WatcherError(std::string("Unable to remove watcher: ") + strerror(errno), watcher);
        }

        close(it->second->mountFd);
      }

      it = mSubscriptions.erase(it);
    }
    else {
      ++it;
    }
  }
}
