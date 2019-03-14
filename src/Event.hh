#ifndef EVENT_H
#define EVENT_H

#include <string>
#include <node.h>
#include <uv.h>
#include <v8.h>
#include <nan.h>

using namespace v8;

struct Event {
  std::string mPath;
  std::string mType;
  Event(std::string path, std::string type) {
    mPath = path;
    mType = type;
  }

  Local<Object> toJS() {
    Nan::EscapableHandleScope scope;
    Local<Object> res = Nan::New<Object>();
    res->Set(Nan::New<String>("path").ToLocalChecked(), Nan::New<String>(mPath.c_str()).ToLocalChecked());
    res->Set(Nan::New<String>("type").ToLocalChecked(), Nan::New<String>(mType.c_str()).ToLocalChecked());
    return scope.Escape(res);
  }
};

class EventList {
public:
  std::vector<Event *> mEvents;

  void push(std::string path, std::string type) {
    mEvents.push_back(new Event(path, type));
  }

  Local<Array> toJS() {
    Nan::EscapableHandleScope scope;
    Local<Array> arr = Nan::New<Array>(mEvents.size());

    for (size_t i = 0; i < mEvents.size(); i++) {
      arr->Set(i, mEvents[i]->toJS());
    }

    return scope.Escape(arr);
  }
};

#endif
