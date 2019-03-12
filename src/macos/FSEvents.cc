#include <CoreServices/CoreServices.h>
#include <string>
#include "../Event.hh"

std::string getCurrentTokenImpl(std::string *dir) {
  FSEventStreamEventId id = FSEventsGetCurrentEventId();
  return std::to_string(id);
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
  EventList *list = (EventList *)clientCallBackInfo;

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
      CFRunLoopStop(CFRunLoopGetCurrent());
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
}

EventList *getEventsSinceImpl(std::string *dir, std::string *token) {
  FSEventStreamEventId id = std::stoull(*token);

  CFAbsoluteTime latency = 0.001;
  CFStringRef fileWatchPath = CFStringCreateWithCString(
    NULL,
    dir->c_str(),
    kCFStringEncodingUTF8
  );

  CFArrayRef pathsToWatch = CFArrayCreate(
    NULL,
    (const void **)&fileWatchPath,
    1,
    NULL
  );
  
  EventList *list = new EventList();

  FSEventStreamContext callbackInfo {0, (void *)list, nullptr, nullptr, nullptr};
  FSEventStreamRef stream = FSEventStreamCreate(
    NULL,
    &FSEventsCallback,
    &callbackInfo,
    pathsToWatch,
    id,
    latency,
    kFSEventStreamCreateFlagFileEvents
  );

  CFRunLoopRef runLoop = CFRunLoopGetCurrent();
  FSEventStreamScheduleWithRunLoop(stream, runLoop, kCFRunLoopDefaultMode);
  FSEventStreamStart(stream);
  CFRelease(pathsToWatch);
  CFRelease(fileWatchPath);

  CFRunLoopRun();

  FSEventStreamStop(stream);
  FSEventStreamUnscheduleFromRunLoop(stream, runLoop, kCFRunLoopDefaultMode);
  FSEventStreamInvalidate(stream);
  FSEventStreamRelease(stream);

  return list;
}
