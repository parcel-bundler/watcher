#ifndef EVENT_H
#define EVENT_H

#include <string>
#include <mutex>
#include <map>

#include <node_api.h>
#include <napi.h>

#include "wasm/include.h"

using namespace Napi;

struct Event {
  std::string path;
  std::string pathTo;
  std::string pathFrom;
  bool isCreated;
  bool isDeleted;
  bool isMoved;

  Event(const std::string &path)
      : path(path)
      , isCreated(false)
      , isDeleted(false)
      , isMoved(false) {
  }

  Value toJS(const Env &env) {
    EscapableHandleScope scope(env);
    Object res = Object::New(env);
    res.Set(String::New(env, "path"), String::New(env, path.c_str()));
    res.Set(String::New(env, "pathFrom"), String::New(env, pathFrom.c_str()));
    res.Set(String::New(env, "pathTo"), String::New(env, pathTo.c_str()));
    res.Set(String::New(env, "type"), String::New(env, (isMoved ? "move" : isCreated ? "create" : isDeleted ? "delete" : "update")));
    return scope.Escape(res);
  }
};

class EventList {
public:
  void create(const std::string &path, bool isMoved = false) {
    std::lock_guard<std::mutex> l(mMutex);
    Event *event = internalUpdate(path);
    event->isMoved = isMoved;

    if (isMoved) {
      event->pathTo = path;
    }

    if (event->isDeleted) {
      // Assume update event when rapidly removed and created
      // https://github.com/parcel-bundler/watcher/issues/72
      event->isDeleted = false;
    }
    else {
      event->isCreated = true;
    }
  }

  Event *update(const std::string &path) {
    std::lock_guard<std::mutex> l(mMutex);
    return internalUpdate(path);
  }

  void move(const std::string &path, const std::string &pathTo) {
    std::lock_guard<std::mutex> l(mMutex);
    Event *eventFrom = internalUpdate(path);
    eventFrom->pathTo = pathTo;
    eventFrom->pathFrom = path;
    eventFrom->isMoved = true;

    Event *eventTo = internalUpdate(pathTo);
    eventTo->pathTo = pathTo;
    eventTo->pathFrom = path;
    eventTo->isMoved = true;
  }

  void remove(const std::string &path, bool isMoved = false) {
    std::lock_guard<std::mutex> l(mMutex);
    Event *event = internalUpdate(path);
    event->isDeleted = true;
    event->isMoved = isMoved;

    if (isMoved) {
      event->pathFrom = path;
    }
  }

  size_t size() {
    std::lock_guard<std::mutex> l(mMutex);
    return mEvents.size();
  }

  std::vector<Event> getEvents() {
    std::lock_guard<std::mutex> l(mMutex);
    std::vector<Event> eventsCloneVector;
    for (auto &e : mEvents) {
      if (!(e.second.isCreated && e.second.isDeleted)) {
        eventsCloneVector.push_back(e.second);
      }
    }
    return eventsCloneVector;
  }

  void clear() {
    std::lock_guard<std::mutex> l(mMutex);
    mEvents.clear();
  }

private:
  mutable std::mutex mMutex;
  std::map<std::string, Event> mEvents;
  Event *internalUpdate(const std::string &path) {
    auto found = mEvents.find(path);
    if (found == mEvents.end()) {
      auto it = mEvents.emplace(path, Event(path));
      return &it.first->second;
    }

    return &found->second;
  }
};

#endif
