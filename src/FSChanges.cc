#ifdef FS_EVENTS
#include "macos/FSEvents.hh"
#endif
#ifdef WATCHMAN
#include "watchman/watchman.hh"
#endif

#include <unordered_set>
#include <iostream>
#include <napi.h>
#include <node_api.h>
#include "Event.hh"
#include "Backend.hh"
#include "Watcher.hh"

using namespace Napi;

class FSAsyncRunner;
typedef void (*AsyncFunction)(FSAsyncRunner *);

std::unordered_set<std::string> getIgnore(Env env, Value opts) {
  std::unordered_set<std::string> ignore;

  if (opts.IsObject()) {
    Value v = opts.As<Object>().Get(String::New(env, "ignore"));
    if (v.IsArray()) {
      Array items = v.As<Array>();
      for (size_t i = 0; i < items.Length(); i++) {
        Value item = items.Get(Number::New(env, i));
        if (item.IsString()) {
          ignore.insert(std::string(item.As<String>().Utf8Value().c_str()));
        }
      }
    }
  }

  return ignore;
}

std::shared_ptr<Backend> getBackend(Env env, Value opts) {
  Value b = opts.As<Object>().Get(String::New(env, "backend"));
  std::string backendName;
  if (b.IsString()) {
    backendName = std::string(b.As<String>().Utf8Value().c_str());
  }

  return Backend::getShared(backendName);
}

class FSAsyncRunner {
public:
  const Env env;

  std::shared_ptr<Backend> backend;
  std::shared_ptr<Watcher> watcher;
  std::string snapshotPath;
  bool returnEvents;

  FSAsyncRunner(Env env, Value dir, Value snap, Value opts, Promise::Deferred r, AsyncFunction func)
    : env(env),
      snapshotPath(std::string(snap.As<String>().Utf8Value().c_str())),
      returnEvents(false),
      func(func), deferred(r) {

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

    watcher = Watcher::getShared(
      std::string(dir.As<String>().Utf8Value().c_str()),
      getIgnore(env, opts)
    );

    backend = getBackend(env, opts);
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

    if (this->returnEvents) {
      result = watcher->mEvents.toJS(env);
    } else {
      result = env.Null();
    }

    watcher->unref();
    backend->unref();
    this->deferred.Resolve(result);
  }

  void OnError(const Error& e) {
    watcher->unref();
    backend->unref();
    this->deferred.Reject(e.Value());
  }
};

void writeSnapshotAsync(FSAsyncRunner *runner) {
  runner->backend->writeSnapshot(*runner->watcher, &runner->snapshotPath);
}

void getEventsSinceAsync(FSAsyncRunner *runner) {
  runner->backend->getEventsSince(*runner->watcher, &runner->snapshotPath);
  runner->returnEvents = true;
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

Value subscribe(const CallbackInfo& info) {
  Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    TypeError::New(env, "Expected a string").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 2 || !info[1].IsFunction()) {
    TypeError::New(env, "Expected a function").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() >= 3 && !info[2].IsObject()) {
    TypeError::New(env, "Expected an object").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::shared_ptr<Watcher> watcher = Watcher::getShared(
    std::string(info[0].As<String>().Utf8Value().c_str()), 
    getIgnore(env, info[2])
  );

  bool added = watcher->watch(info[1].As<Function>());
  if (added) {
    std::shared_ptr<Backend> b = getBackend(env, info[2]);;
    b->watch(*watcher);
  }

  return env.Null();
}

Value unsubscribe(const CallbackInfo& info) {
  Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    TypeError::New(env, "Expected a string").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() < 2 || !info[1].IsFunction()) {
    TypeError::New(env, "Expected a function").ThrowAsJavaScriptException();
    return env.Null();
  }

  if (info.Length() >= 3 && !info[2].IsObject()) {
    TypeError::New(env, "Expected an object").ThrowAsJavaScriptException();
    return env.Null();
  }

  std::shared_ptr<Watcher> watcher = Watcher::getShared(
    std::string(info[0].As<String>().Utf8Value().c_str()),
    getIgnore(env, info[2])
  );

  bool removed =  watcher->unwatch(info[1].As<Function>());
  if (removed) {
    std::shared_ptr<Backend> b = getBackend(env, info[2]);;
    b->unwatch(*watcher);
  }
  
  return env.Null();
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
  exports.Set(
    String::New(env, "subscribe"),
    Function::New(env, subscribe)
  );
  exports.Set(
    String::New(env, "unsubscribe"),
    Function::New(env, unsubscribe)
  );
  return exports;
}

NODE_API_MODULE(fschanges, Init)
