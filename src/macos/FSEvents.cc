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

    if (isCreated && !(isRemoved || isModified || isRenamed)) {
      list->push(paths[i], "create");
    } else if (isRemoved && !(isCreated || isModified || isRenamed)) {
      list->push(paths[i], "delete");
    } else if (isModified && !(isCreated || isRemoved || isRenamed)) {
      list->push(paths[i], "update");
    } else if (isRenamed && !(isCreated || isModified || isRemoved)) {
      list->push(paths[i], "rename");
    } else if (isDone) {
      stopStream((FSEventStreamRef)streamRef, CFRunLoopGetCurrent());
    } else {
      struct stat file;
      if (stat(paths[i], &file) != 0) {
        list->push(paths[i], "delete");
        return;
      }

      if (file.st_birthtimespec.tv_sec != file.st_mtimespec.tv_sec) {
        list->push(paths[i], "update");
      } else {
        list->push(paths[i], "create");
      }
    }
  }

  watcher->notify();
}

void FSEventsBackend::startStream(Watcher &watcher, FSEventStreamEventId id) {
  CFAbsoluteTime latency = 0.001;
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

  // Unlock once run loop has started.
  CFRunLoopPerformBlock(mRunLoop, kCFRunLoopDefaultMode, ^ {
    mMutex.unlock();
  });

  CFRunLoopWakeUp(mRunLoop);
  CFRunLoopRun();
}

FSEventsBackend::~FSEventsBackend() {
  std::lock_guard<std::mutex> lock(mMutex);
  CFRunLoopStop(mRunLoop);
}

void FSEventsBackend::writeSnapshot(Watcher &watcher, std::string *snapshotPath) {
  std::lock_guard<std::mutex> lock(mMutex);
  FSEventStreamEventId id = FSEventsGetCurrentEventId();
  std::ofstream ofs(*snapshotPath);
  ofs << id;
}

void FSEventsBackend::getEventsSince(Watcher &watcher, std::string *snapshotPath) {
  std::lock_guard<std::mutex> lock(mMutex);
  std::ifstream ifs(*snapshotPath);
  if (ifs.fail()) {
    return;
  }

  FSEventStreamEventId id;
  ifs >> id;

  startStream(watcher, id);
}

void FSEventsBackend::subscribe(Watcher &watcher) {
  std::lock_guard<std::mutex> lock(mMutex);
  startStream(watcher, kFSEventStreamEventIdSinceNow);
}

void FSEventsBackend::unsubscribe(Watcher &watcher) {
  std::lock_guard<std::mutex> lock(mMutex);
  FSEventStreamRef stream = (FSEventStreamRef)watcher.state;
  stopStream(stream, mRunLoop);
}
