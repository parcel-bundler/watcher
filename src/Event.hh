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
    Event *event = update(path);
    event->isCreated = true;
  }

  Event *update(std::string path) {
    auto found = mEvents.find(path);
    if (found == mEvents.end()) {
      auto it = mEvents.emplace(path, Event(path));
      return &it.first->second;
    }

    return &found->second;
  }

  void remove(std::string path) {
    Event *event = update(path);
    if (event->isCreated) {
      mEvents.erase(path);
    } else {
      event->isDeleted = true;
    }
  }

  void clear() {
    mEvents.clear();
  }

  size_t size() {
    return mEvents.size();
  }

  Value toJS(const Env& env) {
    EscapableHandleScope scope(env);
    Array arr = Array::New(env, mEvents.size());
    size_t i = 0;

    for (auto it = mEvents.begin(); it != mEvents.end(); it++) {
      arr.Set(i++, it->second.toJS(env));
    }

    return scope.Escape(arr);
  }

private:
  std::map<std::string, Event> mEvents;
};

#endif
