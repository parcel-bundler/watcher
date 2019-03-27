#include <CoreServices/CoreServices.h>
#include <sys/stat.h>
#include <string>
#include <fstream>
#include <unordered_set>
#include "../Event.hh"
#include "../Backend.hh"
#include "./FSEvents.hh"
#include "../Watcher.hh"

void stopStream(FSEventStreamRef stream, CFRunLoopRef runLoop) {
  FSEventStreamStop(stream);
  FSEventStreamUnscheduleFromRunLoop(stream, runLoop, kCFRunLoopDefaultMode);
  FSEventStreamInvalidate(stream);
  FSEventStreamRelease(stream);
}

struct State {
  FSEventStreamRef stream;
  struct timespec since;
};

bool operator <(const timespec& lhs, const timespec& rhs) {
  return lhs.tv_sec == rhs.tv_sec ? lhs.tv_nsec < rhs.tv_nsec : lhs.tv_sec < rhs.tv_sec;
}

void FSEventsCallback(
  ConstFSEventStreamRef streamRef,
  void *clientCallBackInfo,
  size_t numEvents,
  void *eventPaths,
  const FSEventStreamEventFlags eventFlags[],
  const FSEventStreamEventId eventIds[]
) {
  char **paths = (char **)eventPaths;
  Watcher *watcher = (Watcher *)clientCallBackInfo;
  EventList *list = &watcher->mEvents;
  State *state = (State *)watcher->state;
  struct timespec since = state->since;

  for (size_t i = 0; i < numEvents; ++i) {
    bool isCreated = (eventFlags[i] & kFSEventStreamEventFlagItemCreated) == kFSEventStreamEventFlagItemCreated;
    bool isRemoved = (eventFlags[i] & kFSEventStreamEventFlagItemRemoved) == kFSEventStreamEventFlagItemRemoved;
    bool isModified = (eventFlags[i] & kFSEventStreamEventFlagItemModified) == kFSEventStreamEventFlagItemModified ||
                      (eventFlags[i] & kFSEventStreamEventFlagItemInodeMetaMod) == kFSEventStreamEventFlagItemInodeMetaMod ||
                      (eventFlags[i] & kFSEventStreamEventFlagItemFinderInfoMod) == kFSEventStreamEventFlagItemFinderInfoMod ||
                      (eventFlags[i] & kFSEventStreamEventFlagItemChangeOwner) == kFSEventStreamEventFlagItemChangeOwner ||
                      (eventFlags[i] & kFSEventStreamEventFlagItemXattrMod) == kFSEventStreamEventFlagItemXattrMod;
    bool isRenamed = (eventFlags[i] & kFSEventStreamEventFlagItemRenamed) == kFSEventStreamEventFlagItemRenamed;
    bool isDone = (eventFlags[i] & kFSEventStreamEventFlagHistoryDone) == kFSEventStreamEventFlagHistoryDone;
    bool isDir = (eventFlags[i] & kFSEventStreamEventFlagItemIsDir) == kFSEventStreamEventFlagItemIsDir;

    if (isDone) {
      watcher->notify();
      break;
    }

    // Handle unambiguous events first
    if (isCreated && !(isRemoved || isModified || isRenamed)) {
      watcher->mTree->add(paths[i], 0, isDir);
      list->create(paths[i]);
    } else if (isRemoved && !(isCreated || isModified || isRenamed)) {
      watcher->mTree->remove(paths[i]);
      list->remove(paths[i]);
    } else if (isModified && !(isCreated || isRemoved || isRenamed)) {
      watcher->mTree->update(paths[i], 0);
      list->update(paths[i]);
    } else {
      // If multiple flags were set, then we need to call `stat` to determine if the file really exists.
      // We also check our local cache of recently modified files to see if we knew about it before.
      // This helps disambiguate creates, updates, and deletes.
      auto existed = watcher->mTree->find(paths[i]);
      struct stat file;
      if (stat(paths[i], &file) != 0) {
        // File does not exist now. If it existed before in our local cache,
        // or was removed/renamed and we don't have a cache (querying since a snapshot)
        // then the file was probably removed. This is not exact since the flags set by
        // fsevents get coalesced together (e.g. created & deleted), so there is no way to
        // know whether the create and delete both happened since our snapshot (in which case
        // we'd rather ignore this event completely). This will result in some extra delete events 
        // being emitted for files we don't know about, but that is the best we can do.
        if (existed || (since.tv_sec != 0 && (isRemoved || isRenamed))) {
          watcher->mTree->remove(paths[i]);
          list->remove(paths[i]);
        }

        continue;
      }

      // If the file was modified, and existed before, then this is an update, otherwise a create.
      if (isModified && (existed || file.st_birthtimespec < since)) {
        watcher->mTree->update(paths[i], file.st_mtime);
        list->update(paths[i]);
      } else {
        watcher->mTree->add(paths[i], file.st_mtime, S_ISDIR(file.st_mode));
        list->create(paths[i]);
      }
    }
  }

  if (watcher->mWatched) {
    watcher->notify();
  }
}

void FSEventsBackend::startStream(Watcher &watcher, FSEventStreamEventId id) {
  watcher.mTree = new DirTree();
  
  CFAbsoluteTime latency = 0.01;
  CFStringRef fileWatchPath = CFStringCreateWithCString(
    NULL,
    watcher.mDir.c_str(),
    kCFStringEncodingUTF8
  );

  CFArrayRef pathsToWatch = CFArrayCreate(
    NULL,
    (const void **)&fileWatchPath,
    1,
    NULL
  );

  FSEventStreamContext callbackInfo {0, (void *)&watcher, nullptr, nullptr, nullptr};
  FSEventStreamRef stream = FSEventStreamCreate(
    NULL,
    &FSEventsCallback,
    &callbackInfo,
    pathsToWatch,
    id,
    latency,
    kFSEventStreamCreateFlagFileEvents
  );
  
  CFMutableArrayRef exclusions = CFArrayCreateMutable(NULL, watcher.mIgnore.size(), NULL);
  for (auto it = watcher.mIgnore.begin(); it != watcher.mIgnore.end(); it++) {
    CFStringRef path = CFStringCreateWithCString(
      NULL,
      it->c_str(),
      kCFStringEncodingUTF8
    );

    CFArrayAppendValue(exclusions, (const void *)path);
  }

  FSEventStreamSetExclusionPaths(stream, exclusions);

  FSEventStreamScheduleWithRunLoop(stream, mRunLoop, kCFRunLoopDefaultMode);
  FSEventStreamStart(stream);
  CFRelease(pathsToWatch);
  CFRelease(fileWatchPath);

  State *s = (State *)watcher.state;
  s->stream = stream;
}

void FSEventsBackend::start() {
  mRunLoop = CFRunLoopGetCurrent();
  CFRetain(mRunLoop);

  // Unlock once run loop has started.
  CFRunLoopPerformBlock(mRunLoop, kCFRunLoopDefaultMode, ^ {
    notifyStarted();
  });

  CFRunLoopWakeUp(mRunLoop);
  CFRunLoopRun();
}

FSEventsBackend::~FSEventsBackend() {
  std::unique_lock<std::mutex> lock(mMutex);
  CFRunLoopStop(mRunLoop);
  CFRelease(mRunLoop);
}

void FSEventsBackend::writeSnapshot(Watcher &watcher, std::string *snapshotPath) {
  std::unique_lock<std::mutex> lock(mMutex);
  FSEventStreamEventId id = FSEventsGetCurrentEventId();
  std::ofstream ofs(*snapshotPath);
  ofs << id;
  ofs << "\n";

  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  ofs << now.tv_sec;
  ofs << "\n";
  ofs << now.tv_nsec;
}

void FSEventsBackend::getEventsSince(Watcher &watcher, std::string *snapshotPath) {
  std::unique_lock<std::mutex> lock(mMutex);
  std::ifstream ifs(*snapshotPath);
  if (ifs.fail()) {
    return;
  }

  FSEventStreamEventId id;
  ifs >> id;

  struct timespec since;
  ifs >> since.tv_sec;
  ifs >> since.tv_nsec;

  State *s = new State;
  s->since = since;
  watcher.state = (void *)s;

  startStream(watcher, id);
  watcher.wait();
  stopStream(s->stream, mRunLoop);

  delete s;
  watcher.state = NULL;

  delete watcher.mTree;
  watcher.mTree = NULL;
}

void FSEventsBackend::subscribe(Watcher &watcher) {
  State *s = new State;
  struct timespec since;
  memset(&since, 0, sizeof(since));
  s->since = since;
  watcher.state = (void *)s;
  startStream(watcher, kFSEventStreamEventIdSinceNow);
}

void FSEventsBackend::unsubscribe(Watcher &watcher) {
  State *s = (State *)watcher.state;
  stopStream(s->stream, mRunLoop);

  delete s;
  watcher.state = NULL;

  delete watcher.mTree;
  watcher.mTree = NULL;
}
