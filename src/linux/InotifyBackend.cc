#include <memory>
#include <poll.h>
#include <unistd.h>
#include "InotifyBackend.hh"

#define INOTIFY_MASK \
  IN_ATTRIB | IN_CREATE | IN_DELETE | \
  IN_DELETE_SELF | IN_MODIFY | IN_MOVE_SELF | IN_MOVED_FROM | \
  IN_MOVED_TO | IN_DONT_FOLLOW | IN_ONLYDIR | IN_EXCL_UNLINK
#define BUFFER_SIZE 8192

void InotifyBackend::start() {
  // Create a pipe that we will write to when we want to end the thread.
  int err = pipe2(mPipe, O_CLOEXEC | O_NONBLOCK);
  if (err == -1) {
    throw "Unable to open pipe";
  }

  // Init inotify file descriptor.
  mInotify = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (mInotify == -1) {
    throw "Unable to initialize inotify";
  }

  pollfd pollfds[2];
  pollfds[0].fd = mPipe[0];
  pollfds[0].events = POLLIN;
  pollfds[0].revents = 0;
  pollfds[1].fd = mInotify;
  pollfds[1].events = POLLIN;
  pollfds[1].revents = 0;

  notifyStarted();

  // Loop until we get an event from the pipe.
  while (true) {
    int result = poll(pollfds, 2, 500);
    if (result < 0) {
      throw "Unable to poll";
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
  close(mInotify);

  mEndedSignal.notify();
}

InotifyBackend::~InotifyBackend() {
  write(mPipe[1], "X", 1);
  mEndedSignal.wait();
}

void InotifyBackend::subscribe(Watcher &watcher) {
  // Build a full directory tree recursively, and watch each directory.
  DirTree *tree = getTree(watcher);
  
  for (auto it = tree->entries.begin(); it != tree->entries.end(); it++) {
    if (it->second.isDir) {
      watchDir(watcher, (DirEntry *)&it->second);
    }
  }
}

void InotifyBackend::watchDir(Watcher &watcher, DirEntry *entry) {
  int wd = inotify_add_watch(mInotify, entry->path.c_str(), INOTIFY_MASK);
  if (wd == -1) {
    throw "inotify_add_watch failed";
  }

  entry->state = (void *)&watcher;
  mSubscriptions.emplace(wd, entry);
}

void InotifyBackend::handleEvents() {
  char buf[BUFFER_SIZE] __attribute__ ((aligned(__alignof__(struct inotify_event))));;
  struct inotify_event *event;

  // Track all of the watchers that are touched so we can notify them at the end of the events.
  std::unordered_set<Watcher *> watchers;

  while (true) {
    int n = read(mInotify, &buf, BUFFER_SIZE);
    if (n < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;
      }

      throw "Error reading from inotify";
    }

    if (n == 0) {
      break;
    }

    for (char *ptr = buf; ptr < buf + n; ptr += sizeof(*event) + event->len) {
      event = (struct inotify_event *)ptr;

      if ((event->mask & IN_Q_OVERFLOW) == IN_Q_OVERFLOW) {
        // overflow
        continue;
      }

      Watcher *watcher = handleEvent(event);
      if (watcher) {
        watchers.insert(watcher);
      }
    }
  }

  for (auto it = watchers.begin(); it != watchers.end(); it++) {
    (*it)->notify();
  }
}

Watcher *InotifyBackend::handleEvent(struct inotify_event *event) {
  std::unique_lock<std::mutex> lock(mMutex);

  // Find a subscription for this watch descriptor
  auto entry = mSubscriptions.find(event->wd);
  if (entry == mSubscriptions.end()) {
    // Unknown path
    return NULL;
  }

  // Build full path and check if its in our ignore list.
  Watcher *watcher = (Watcher *)entry->second->state;
  std::string path = std::string(entry->second->path);
  if (event->len > 0) { 
    path += "/" + std::string(event->name);
  }

  if (watcher->mIgnore.count(path) > 0) {
    return NULL;
  }

  // If this is a create, check if it's a directory and start watching if it is.
  // In any case, keep the directory tree up to date.
  if (event->mask & (IN_CREATE | IN_MOVED_TO)) {
    watcher->mEvents.create(path);

    struct stat st;
    stat(path.c_str(), &st);
    DirEntry *entry = watcher->mTree->add(path, st.st_mtime, S_ISDIR(st.st_mode));

    if (entry->isDir) {
      watchDir(*watcher, entry);
    }
  } else if (event->mask & (IN_MODIFY | IN_ATTRIB)) {
    watcher->mEvents.update(path);

    struct stat st;
    stat(path.c_str(), &st);
    watcher->mTree->update(path, st.st_mtime);
  } else if (event->mask & (IN_DELETE | IN_DELETE_SELF | IN_MOVED_FROM | IN_MOVE_SELF)) {
    // Ignore delete/move self events unless this is the recursive watch root
    if ((event->mask & (IN_DELETE_SELF | IN_MOVE_SELF)) && path != watcher->mDir) {
      return NULL;
    }

    // If the entry being deleted/moved is a directory, remove it from the list of subscriptions
    auto entry = watcher->mTree->find(path);
    if (entry && entry->isDir) {
      for (auto it = mSubscriptions.begin(); it != mSubscriptions.end(); it++) {
        if (it->second == &*entry) {
          it->second->state = NULL;
          mSubscriptions.erase(it);
          break;
        }
      }
    }

    watcher->mEvents.remove(path);
    watcher->mTree->remove(path);
  }

  return watcher;
}

void InotifyBackend::unsubscribe(Watcher &watcher) {
  // Find any subscriptions pointing to this watcher, and remove them.
  for (auto it = mSubscriptions.begin(); it != mSubscriptions.end();) {
    if (it->second->state == &watcher) {
      int err = inotify_rm_watch(mInotify, it->first);
      if (err == -1) {
        throw "Unable to remove watcher";
      }

      it->second->state = NULL;
      it = mSubscriptions.erase(it);
    } else {
      it++;
    }
  }

  delete watcher.mTree;
  watcher.mTree = NULL;
}
