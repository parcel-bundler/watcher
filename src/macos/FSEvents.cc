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
    } else if (isDone) {
      watcher->notify();
      break;
    } else {
      // If multiple flags were set, then we need to call `stat` to determine if the file really exists.
      // We also check our local cache of recently modified files to see if we knew about it before.
      // This helps disambiguate creates, updates, and deletes.
      struct stat file;
      if (stat(paths[i], &file) != 0) {
        if (watcher->mTree->find(paths[i])) {
          watcher->mTree->remove(paths[i]);
          list->remove(paths[i]);
        }

        continue;
      }

      if (watcher->mTree->find(paths[i])) {
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
  
  CFAbsoluteTime latency = 0.0;
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

  watcher.state = (void *)stream;
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
}

void FSEventsBackend::getEventsSince(Watcher &watcher, std::string *snapshotPath) {
  std::unique_lock<std::mutex> lock(mMutex);
  std::ifstream ifs(*snapshotPath);
  if (ifs.fail()) {
    return;
  }

  FSEventStreamEventId id;
  ifs >> id;

  startStream(watcher, id);
  watcher.wait();
  stopStream((FSEventStreamRef)watcher.state, mRunLoop);

  delete watcher.mTree;
  watcher.mTree = NULL;
}

void FSEventsBackend::subscribe(Watcher &watcher) {
  startStream(watcher, kFSEventStreamEventIdSinceNow);
}

void FSEventsBackend::unsubscribe(Watcher &watcher) {
  FSEventStreamRef stream = (FSEventStreamRef)watcher.state;
  stopStream(stream, mRunLoop);

  delete watcher.mTree;
  watcher.mTree = NULL;
}
