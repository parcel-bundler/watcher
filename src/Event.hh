#ifndef EVENT_H
#define EVENT_H

#include <string>
#include <napi.h>

using namespace Napi;

struct Event {
  std::string mPath;
  std::string mType;
  Event(std::string path, std::string type) {
    mPath = path;
    mType = type;
  }

  Value toJS(const Env& env) {
    EscapableHandleScope scope(env);
    Object res = Object::New(env);
    res.Set(String::New(env, "path"), String::New(env, mPath.c_str()));
    res.Set(String::New(env, "type"), String::New(env, mType.c_str()));
    return scope.Escape(res);
  }
};

class EventList {
public:
  std::vector<Event> mEvents;

  void push(std::string path, std::string type) {
    mEvents.push_back(Event(path, type));
  }

  Value toJS(const Env& env) {
    EscapableHandleScope scope(env);
    Array arr = Array::New(env, mEvents.size());

    for (size_t i = 0; i < mEvents.size(); i++) {
      arr.Set(i, mEvents[i].toJS(env));
    }

    return scope.Escape(arr);
  }
};

#endif
