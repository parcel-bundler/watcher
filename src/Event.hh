#ifndef EVENT_H
#define EVENT_H

#include <string>
#include <napi.h>
#include <uv.h>
#include <v8.h>
#include <napi.h>

using namespace Napi;

struct Event {
  std::string mPath;
  std::string mType;
  Event(std::string path, std::string type) {
    mPath = path;
    mType = type;
  }

  Napi::Value toJS(const Napi::Env& env) {
    Napi::EscapableHandleScope scope(env);
    Napi::Object res = Napi::Object::New(env);
    res.Set(Napi::String::New(env, "path"), Napi::String::New(env, mPath.c_str()));
    res.Set(Napi::String::New(env, "type"), Napi::String::New(env, mType.c_str()));
    return scope.Escape(res);
  }
};

class EventList {
public:
  std::vector<Event *> mEvents;

  void push(std::string path, std::string type) {
    mEvents.push_back(new Event(path, type));
  }

  Napi::Value toJS(const Napi::Env& env) {
    Napi::EscapableHandleScope scope(env);
    Napi::Array arr = Napi::Array::New(env, mEvents.size());

    for (size_t i = 0; i < mEvents.size(); i++) {
      arr.Set(i, mEvents[i]->toJS(env));
    }

    return scope.Escape(arr);
  }
};

#endif
