#include <napi.h>
#include <v8.h>
#include <napi.h>
#include <unordered_set>
#include "Event.hh"

void writeSnapshotImpl(std::string *dir, std::string *snapshotPath, std::unordered_set<std::string> *ignore);
EventList *getEventsSinceImpl(std::string *dir, std::string *snapshotPath, std::unordered_set<std::string> *ignore);

struct AsyncRequest {
  Napi::Env env;

  uv_work_t work;
  std::string directory;
  std::string snapshotPath;
  std::unordered_set<std::string> ignore;
  EventList *events;
  Napi::Promise::Deferred deferred;

  AsyncRequest(Napi::Env env, Napi::Value dir, Napi::Value snap, Napi::Value o, Napi::Promise::Deferred r) : env(env), deferred(r) {
    work.data = (void *)this;

    // copy the string since the JS garbage collector might run before the async request is finished
    directory = std::string(dir.As<Napi::String>().Utf8Value().c_str());
    snapshotPath = std::string(snap.As<Napi::String>().Utf8Value().c_str());
    events = NULL;

    if (o.IsObject()) {
      Napi::Value v = o.As<Napi::Object>().Get(Napi::String::New(env, "ignore"));
      if (v.IsArray()) {
        Napi::Array items = v.As<Napi::Array>();
        for (size_t i = 0; i < items.Length(); i++) {
          Napi::Value item = items.Get(Napi::Number::New(env, i));
          if (item.IsString()) {
            ignore.insert(std::string(item.As<Napi::String>().Utf8Value().c_str()));
          }
        }
      }
    }
  }

  ~AsyncRequest() {
    if (events) {
      delete events;
    }
  }
};

void asyncCallback(uv_work_t *work) {
  AsyncRequest *req = (AsyncRequest *) work->data;
  Napi::Env env = req->env;
  Napi::HandleScope scope(env);
  Napi::Value result;

  if (req->events) {
    result = req->events->toJS(env);
  } else {
    result = env.Null();
  }

  req->deferred.Resolve(result);
  delete req;
}

void writeSnapshotAsync(uv_work_t *work) {
  AsyncRequest *req = (AsyncRequest *) work->data;
  writeSnapshotImpl(&req->directory, &req->snapshotPath, &req->ignore);
}

void getEventsSinceAsync(uv_work_t *work) {
  AsyncRequest *req = (AsyncRequest *) work->data;
  req->events = getEventsSinceImpl(&req->directory, &req->snapshotPath, &req->ignore);
}

Napi::Value queueWork(const Napi::CallbackInfo& info, uv_work_cb cb) {
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(info.Env(), "Expected a string").ThrowAsJavaScriptException();
    return info.Env().Null();
  }

  if (info.Length() < 2 || !info[1].IsString()) {
    Napi::TypeError::New(info.Env(), "Expected a string").ThrowAsJavaScriptException();
    return info.Env().Null();
  }

  if (info.Length() >= 3 && !info[2].IsObject()) {
    Napi::TypeError::New(info.Env(), "Expected an object").ThrowAsJavaScriptException();
    return info.Env().Null();
  }

  auto deferred = Napi::Promise::Deferred::New(info.Env());
  AsyncRequest *req = new AsyncRequest(info.Env(), info[0], info[1], info[2], deferred);
  uv_queue_work(uv_default_loop(), &req->work, cb, (uv_after_work_cb) asyncCallback);

  return deferred.Promise();
}

Napi::Value writeSnapshot(const Napi::CallbackInfo& info) {
  return queueWork(info, writeSnapshotAsync);
}

Napi::Value getEventsSince(const Napi::CallbackInfo& info) {
  return queueWork(info, getEventsSinceAsync);
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set(
    Napi::String::New(env, "writeSnapshot"),
    Napi::Function::New(env, writeSnapshot)
  );
  exports.Set(
    Napi::String::New(env, "getEventsSince"),
    Napi::Function::New(env, getEventsSince)
  );
  return exports;
}

NODE_API_MODULE(fschanges, Init)
