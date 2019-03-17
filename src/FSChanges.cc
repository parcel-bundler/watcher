#include <unordered_set>
#include <iostream>
#include <napi.h>
#include <node_api.h>
#include "Event.hh"
#include "Backend.hh"

using namespace Napi;

class FSAsyncRunner;
typedef void (*AsyncFunction)(FSAsyncRunner *);

class FSAsyncRunner {
public:
  const Env env;

  std::string directory;
  std::string snapshotPath;
  std::string backend;

  std::unordered_set<std::string> ignore;
  EventList *events;

  FSAsyncRunner(Env env, Value dir, Value snap, Value opts, Promise::Deferred r, AsyncFunction func)
    : env(env), directory(std::string(dir.As<String>().Utf8Value().c_str())),
      snapshotPath(std::string(snap.As<String>().Utf8Value().c_str())),
      events(nullptr), func(func), deferred(r) {

    napi_status status = napi_create_async_work(env, nullptr, env.Undefined(), 
                                                OnExecute, OnWorkComplete, this, &this->work);
    if(status != napi_ok) {
      work = nullptr;
      const napi_extended_error_info *error_info = 0;
      napi_get_last_error_info(env, &error_info);
      if(error_info->error_message)
        Error::New(env, error_info->error_message).ThrowAsJavaScriptException();
      else
        Error::New(env).ThrowAsJavaScriptException();
    }

    if (opts.IsObject()) {
      Value v = opts.As<Object>().Get(String::New(env, "ignore"));
      if (v.IsArray()) {
        Array items = v.As<Array>();
        for (size_t i = 0; i < items.Length(); i++) {
          Value item = items.Get(Number::New(env, i));
          if (item.IsString()) {
            this->ignore.insert(std::string(item.As<String>().Utf8Value().c_str()));
          }
        }
      }
    }

    Value b = opts.As<Object>().Get(String::New(env, "backend"));
    if (b.IsString()) {
      backend = std::string(b.As<String>().Utf8Value().c_str());
    }
  }

  ~FSAsyncRunner() {
    if (this->events) {
      delete this->events;
    }
  }

  void Queue() {
    if(this->work) {
      napi_status status = napi_queue_async_work(env, this->work);
      NAPI_THROW_IF_FAILED_VOID(env, status);
    }
  }

private:
  napi_async_work work;
  AsyncFunction func;
  Promise::Deferred deferred;

  static void OnExecute(napi_env env, void* this_pointer) {
    FSAsyncRunner* self = (FSAsyncRunner*) this_pointer;
    self->Execute();
  }

  static void OnWorkComplete(napi_env env, napi_status status, void* this_pointer) {
    FSAsyncRunner* self = (FSAsyncRunner*) this_pointer;
    if (status != napi_cancelled) {
      HandleScope scope(self->env);
      if(status == napi_ok) {
        status = napi_delete_async_work(self->env, self->work);
        if(status == napi_ok) {
          self->OnOK();
          delete self;
          return;
        }
      }
    }

    // fallthrough for error handling
    const napi_extended_error_info *error_info = 0;
    napi_get_last_error_info(env, &error_info);
    if(error_info->error_message){
      self->OnError(Error::New(env, error_info->error_message));
    } else {
      self->OnError(Error::New(env));
    }
    delete self;
  }


  void Execute() {
    this->func(this);
  }

  void OnOK() {
    HandleScope scope(env);
    Value result;

    if (this->events) {
      result = this->events->toJS(env);
    } else {
      result = env.Null();
    }

    this->deferred.Resolve(result);
  }

  void OnError(const Error& e) {
    this->deferred.Reject(e.Value());
  }
};

void writeSnapshotAsync(FSAsyncRunner *runner) {
  GET_BACKEND(runner->backend, writeSnapshot)(&runner->directory, &runner->snapshotPath, &runner->ignore);
}

void getEventsSinceAsync(FSAsyncRunner *runner) {
  runner->events = GET_BACKEND(runner->backend, getEventsSince)(&runner->directory, &runner->snapshotPath, &runner->ignore);
}

Value queueWork(const CallbackInfo& info, AsyncFunction func) {
  Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    TypeError::New(env, "Expected a string").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 2 || !info[1].IsString()) {
    TypeError::New(env, "Expected a string").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() >= 3 && !info[2].IsObject()) {
    TypeError::New(env, "Expected an object").ThrowAsJavaScriptException();
    return env.Null();
  }

  Promise::Deferred deferred = Promise::Deferred::New(env);
  FSAsyncRunner *runner = new FSAsyncRunner(info.Env(), info[0], info[1], info[2], deferred, func);
  runner->Queue();

  return deferred.Promise();
}

Value writeSnapshot(const CallbackInfo& info) {
  return queueWork(info, writeSnapshotAsync);
}

Value getEventsSince(const CallbackInfo& info) {
  return queueWork(info, getEventsSinceAsync);
}

Object Init(Env env, Object exports) {
  exports.Set(
    String::New(env, "writeSnapshot"),
    Function::New(env, writeSnapshot)
  );
  exports.Set(
    String::New(env, "getEventsSince"),
    Function::New(env, getEventsSince)
  );
  return exports;
}

NODE_API_MODULE(fschanges, Init)
