#include <unordered_set>
#include <iostream>
#include <napi.h>
#include "Event.hh"

void writeSnapshotImpl(std::string *dir, std::string *snapshotPath, std::unordered_set<std::string> *ignore);
EventList *getEventsSinceImpl(std::string *dir, std::string *snapshotPath, std::unordered_set<std::string> *ignore);


class AsyncRunner {
public:
  void Queue() {
    napi_status status = napi_queue_async_work(env, work);
    // assert(status == napi_ok);
    // const napi_extended_error_info *error_info = 0;
    // napi_get_last_error_info(env, &error_info);
    // std::cout << error_info->error_message << "\n";
    NAPI_THROW_IF_FAILED_VOID(env, status);
  }
protected:
  AsyncRunner(Napi::Env env): env(env) {
      napi_status status = napi_create_async_work(this->env, nullptr, env.Undefined(), 
                                                  OnExecute, OnWorkComplete, this, &work);
      NAPI_THROW_IF_FAILED_VOID(env, status);
  }
  virtual ~AsyncRunner() {}
  virtual void Execute() = 0;
  virtual void OnOK() = 0;
  const Napi::Env env;

private:
  napi_async_work work;

  static void OnExecute(napi_env env, void* this_pointer) {
    AsyncRunner* self = (AsyncRunner*) this_pointer;
    self->Execute();
  }

  static void OnWorkComplete(napi_env env, napi_status status, void* this_pointer) {
    AsyncRunner* self = (AsyncRunner*) this_pointer;
    if (status != napi_cancelled) {
      HandleScope scope(self->env);
      self->OnOK();
    }
    napi_delete_async_work(env, self->work);
    delete self;
  }

};


class FSAsyncRunner;
typedef void (*AsyncFunction)(FSAsyncRunner *);

class FSAsyncRunner : public AsyncRunner {
public:
  FSAsyncRunner(Napi::Env env, Napi::Value dir, Napi::Value snap, Napi::Value o, Napi::Promise::Deferred r, AsyncFunction func)
    : AsyncRunner(env), 
      directory(std::string(dir.As<Napi::String>().Utf8Value().c_str())),
      snapshotPath(std::string(snap.As<Napi::String>().Utf8Value().c_str())),
      events(nullptr),  deferred(r), func(func) {

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

  std::string directory;
  std::string snapshotPath;

  std::unordered_set<std::string> ignore;
  EventList *events;
  Napi::Promise::Deferred deferred;
  AsyncFunction func;


  ~FSAsyncRunner() {
    if (events) {
      delete events;
    }
  }

  void Execute() {
    this->func(this);
  }

  void OnOK() {
    Napi::HandleScope scope(env);
    Napi::Value result;

    if (this->events) {
      result = this->events->toJS(env);
    } else {
      result = env.Null();
    }

    this->deferred.Resolve(result);
  }
};

void writeSnapshotAsync(FSAsyncRunner *runner) {
  writeSnapshotImpl(&runner->directory, &runner->snapshotPath, &runner->ignore);
}

void getEventsSinceAsync(FSAsyncRunner *runner) {
  runner->events = getEventsSinceImpl(&runner->directory, &runner->snapshotPath, &runner->ignore);
}

Napi::Value queueWork(const Napi::CallbackInfo& info, AsyncFunction func) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "Expected a string").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 2 || !info[1].IsString()) {
    Napi::TypeError::New(env, "Expected a string").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() >= 3 && !info[2].IsObject()) {
    Napi::TypeError::New(env, "Expected an object").ThrowAsJavaScriptException();
    return env.Null();
  }

  Napi::Promise::Deferred deferred = Napi::Promise::Deferred::New(env);
  FSAsyncRunner *runner = new FSAsyncRunner(info.Env(), info[0], info[1], info[2], deferred, func);
  runner->Queue();

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
