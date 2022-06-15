#ifndef EVENT_H
#define EVENT_H

#include <string>
#include <napi.h>
#include <mutex>
#include <map>

using namespace Napi;

struct Event {
  std::string path;
  bool isCreated;
  bool isDeleted;
  Event(std::string path) : path(path), isCreated(false), isDeleted(false) {}

  Value toJS(const Env& env) {
    EscapableHandleScope scope(env);
    Object res = Object::New(env);
    std::string type = isCreated ? "create" : isDeleted ? "delete" : "update";
    res.Set(String::New(env, "path"), String::New(env, path.c_str()));
    res.Set(String::New(env, "type"), String::New(env, type.c_str()));
    return scope.Escape(res);
  }
};

class EventList {
public:
  void create(std::string path) {
    std::lock_guard<std::mutex> l(mMutex);
    Event *event = internalUpdate(path);
    if (event->isDeleted) {
      // Assume update event when rapidly removed and created
      // https://github.com/parcel-bundler/watcher/issues/72
      event->isDeleted = false;
    } else {
      event->isCreated = true;
    }
  }

  Event *update(std::string path) {
    std::lock_guard<std::mutex> l(mMutex);
    return internalUpdate(path);
  }

  void remove(std::string path) {
    std::lock_guard<std::mutex> l(mMutex);
    Event *event = internalUpdate(path);
    if (event->isCreated) {
      // Ignore event when rapidly created and removed
      erase(path);
    } else {
      event->isDeleted = true;
    }
  }

  size_t size() {
    std::lock_guard<std::mutex> l(mMutex);
    return mEvents.size();
  }

  std::vector<Event> getEvents() {
    std::lock_guard<std::mutex> l(mMutex);
    std::vector<Event> eventsCloneVector;
    for(auto event : mEvents) {
      eventsCloneVector.push_back(event);
    }
    return eventsCloneVector;
  }

  void clear() {
    std::lock_guard<std::mutex> l(mMutex);
    mEvents.clear();
  }

private:
  mutable std::mutex mMutex;
  std::vector<Event> mEvents;
  Event *internalUpdate(std::string path) {
    Event *event;

    event = find(path);
    if (!event) {
      mEvents.push_back(Event(path));
      event = &(mEvents.back());
    }

    return event;
  }
  Event *find(std::string path) {
    for(unsigned i=0; i<mEvents.size(); i++) {
      if (mEvents.at(i).path == path) {
        return &(mEvents.at(i));
      }
    }
    return nullptr;
  }
  void erase(std::string path) {
    for(auto it = mEvents.begin(); it != mEvents.end(); ++it) {
      if (it->path == path) {
        mEvents.erase(it);
        return;
      }
    }
  }
};

#endif
