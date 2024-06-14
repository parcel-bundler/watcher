#ifndef EVENT_H
#define EVENT_H

#include <string>
#include <node_api.h>
#include "wasm/include.h"
#include <napi.h>
#include <mutex>
#include <map>

using namespace Napi;

struct Event {
  std::string path;
  bool isCreated;
  bool isDeleted;
  bool isMoved;

  Event(const std::string& path) : path(path), isCreated(false), isDeleted(false), isMoved(false) {}

  Value toJS(const Env& env) {
    EscapableHandleScope scope(env);
    Object res = Object::New(env);
    res.Set(String::New(env, "path"), String::New(env, path.c_str()));
    res.Set(String::New(env, "type"), String::New(env, (isCreated ? "create" : isDeleted ? "delete" : isMoved ? "move" : "update")));
    return scope.Escape(res);
  }
};

class EventList {
public:
  void create(const std::string& path) {
    std::lock_guard<std::mutex> l(mMutex);
    Event* event = internalUpdate(path);
    if (event->isDeleted) {
      // Assume update event when rapidly removed and created
      // https://github.com/parcel-bundler/watcher/issues/72
      event->isDeleted = false;
    }
    else {
      event->isCreated = true;
    }
  }

  Event* update(const std::string& path) {
    std::lock_guard<std::mutex> l(mMutex);
    return internalUpdate(path);
  }

  void move(const std::string& path) {
    std::lock_guard<std::mutex> l(mMutex);
    Event* event = internalUpdate(path);
    event->isMoved = true;
  }

  void remove(const std::string& path) {
    std::lock_guard<std::mutex> l(mMutex);
    Event* event = internalUpdate(path);
    event->isDeleted = true;
  }

  size_t size() {
    std::lock_guard<std::mutex> l(mMutex);
    return mEvents.size();
  }

  std::vector<Event> getEvents() {
    std::lock_guard<std::mutex> l(mMutex);
    std::vector<Event> eventsCloneVector;
    for (auto it = mEvents.begin(); it != mEvents.end(); ++it) {
      if (!(it->second.isCreated && it->second.isDeleted)) {
        eventsCloneVector.push_back(it->second);
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
  Event* internalUpdate(const std::string& path) {
    auto found = mEvents.find(path);
    if (found == mEvents.end()) {
      auto it = mEvents.emplace(path, Event(path));
      return &it.first->second;
    }

    return &found->second;
  }
};

#endif
