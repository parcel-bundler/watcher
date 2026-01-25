#include <unordered_set>
#include <node_api.h>
#include "wasm/include.h"
#include <napi.h>
#include "Glob.hh"
#include "Event.hh"
#include "Backend.hh"
#include "Watcher.hh"
#include "PromiseRunner.hh"

using namespace Napi;

std::unordered_set<std::string> getIgnorePaths(Env env, Value opts) {
  std::unordered_set<std::string> result;

  if (opts.IsObject()) {
    Value v = opts.As<Object>().Get(String::New(env, "ignorePaths"));
    if (v.IsArray()) {
      Array items = v.As<Array>();
      for (size_t i = 0; i < items.Length(); i++) {
        Value item = items.Get(Number::New(env, static_cast<double>(i)));
        if (item.IsString()) {
          result.insert(std::string(item.As<String>().Utf8Value().c_str()));
        }
      }
    }
  }

  return result;
}

std::unordered_set<Glob> getIgnoreGlobs(Env env, Value opts) {
  std::unordered_set<Glob> result;

  if (opts.IsObject()) {
    Value v = opts.As<Object>().Get(String::New(env, "ignoreGlobs"));
    if (v.IsArray()) {
      Array items = v.As<Array>();
      for (size_t i = 0; i < items.Length(); i++) {
        Value item = items.Get(Number::New(env, static_cast<double>(i)));
        if (item.IsString()) {
          auto key = item.As<String>().Utf8Value();
          try {
            result.emplace(key);
          } catch (const std::regex_error& e) {
            Error::New(env, e.what()).ThrowAsJavaScriptException();
          }
        }
      }
    }
  }

  return result;
}

std::shared_ptr<Backend> getBackend(Env env, Value opts) {
  Value b = opts.As<Object>().Get(String::New(env, "backend"));
  std::string backendName;
  if (b.IsString()) {
    backendName = std::string(b.As<String>().Utf8Value().c_str());
  }

  return Backend::getShared(backendName);
}

struct CallbackData {
  std::string error;
  std::vector<Event> events;
  CallbackData(std::string error, std::vector<Event> events) : error(error), events(events) {}
};

Value callbackEventsToJS(const Env &env, std::vector<Event> &events) {
  EscapableHandleScope scope(env);
  Array arr = Array::New(env, events.size());
  uint32_t currentEventIndex = 0;
  for (auto eventIterator = events.begin(); eventIterator != events.end(); eventIterator++) {
    Object res = Object::New(env);
    std::string type = eventIterator->isCreated ? "create" : eventIterator->isDeleted ? "delete" : "update";
    res.Set(String::New(env, "path"), String::New(env, eventIterator->path.c_str()));
    res.Set(String::New(env, "type"), String::New(env, type.c_str()));
    arr.Set(currentEventIndex++, res);
  }
  return scope.Escape(arr);
}

void callJSFunction(Napi::Env env, Function jsCallback, CallbackData *data) {
  HandleScope scope(env);
  auto err = data->error.size() > 0 ? Error::New(env, data->error).Value() : env.Null();
  auto events = callbackEventsToJS(env, data->events);
  jsCallback.Call({err, events});
  delete data;

  // Throw errors from the callback as fatal exceptions
  // If we don't handle these node segfaults...
  if (env.IsExceptionPending()) {
    Napi::Error err = env.GetAndClearPendingException();
    napi_fatal_exception(env, err.Value());
  }
}

class NapiCallback: public Callback {
public:
  Napi::ThreadSafeFunction tsfn;
  Napi::FunctionReference ref;

  virtual void call(std::string error, std::vector<Event> events) {
    CallbackData *data = new CallbackData(error, events);
    tsfn.BlockingCall(data, callJSFunction);
  }

  virtual bool operator==(const Callback &other) const {
    if (const NapiCallback *cb = dynamic_cast<const NapiCallback*>(&other)) {
      return ref.Value() == cb->ref.Value();
    } else {
      return false;
    }
  }

  virtual ~NapiCallback() {
    tsfn.Release();
    ref.Unref();
  }
};

class WriteSnapshotRunner : public PromiseRunner {
public:
  WriteSnapshotRunner(Env env, Value dir, Value snap, Value opts)
    : PromiseRunner(env),
      snapshotPath(std::string(snap.As<String>().Utf8Value().c_str())) {
    watcher = Watcher::getShared(
      std::string(dir.As<String>().Utf8Value().c_str()),
      getIgnorePaths(env, opts),
      getIgnoreGlobs(env, opts)
    );

    backend = getBackend(env, opts);
  }

  ~WriteSnapshotRunner() {
    watcher->unref();
    backend->unref();
  }
private:
  std::shared_ptr<Backend> backend;
  WatcherRef watcher;
  std::string snapshotPath;

  void execute() override {
    backend->writeSnapshot(watcher, &snapshotPath);
  }
};

class GetEventsSinceRunner : public PromiseRunner {
public:
  GetEventsSinceRunner(Env env, Value dir, Value snap, Value opts)
    : PromiseRunner(env),
      snapshotPath(std::string(snap.As<String>().Utf8Value().c_str())) {
    watcher = std::make_shared<Watcher>(
      std::string(dir.As<String>().Utf8Value().c_str()),
      getIgnorePaths(env, opts),
      getIgnoreGlobs(env, opts)
    );

    backend = getBackend(env, opts);
  }

  ~GetEventsSinceRunner() {
    watcher->unref();
    backend->unref();
  }
private:
  std::shared_ptr<Backend> backend;
  WatcherRef watcher;
  std::string snapshotPath;

  void execute() override {
    backend->getEventsSince(watcher, &snapshotPath);
    if (watcher->mEvents.hasError()) {
      throw std::runtime_error(watcher->mEvents.getError());
    }
  }

  Value getResult() override {
    std::vector<Event> events = watcher->mEvents.getEvents();
    Array eventsArray = Array::New(env, events.size());
    uint32_t i = 0;
    for (auto it = events.begin(); it != events.end(); it++) {
      eventsArray.Set(i++, it->toJS(env));
    }
    return eventsArray;
  }
};

template<class Runner>
Value queueSnapshotWork(const CallbackInfo& info) {
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

  Runner *runner = new Runner(info.Env(), info[0], info[1], info[2]);
  return runner->queue();
}

Value writeSnapshot(const CallbackInfo& info) {
  return queueSnapshotWork<WriteSnapshotRunner>(info);
}

Value getEventsSince(const CallbackInfo& info) {
  return queueSnapshotWork<GetEventsSinceRunner>(info);
}

class SubscribeRunner : public PromiseRunner {
public:
  SubscribeRunner(Env env, Value dir, Value fn, Value opts) : PromiseRunner(env) {
    watcher = Watcher::getShared(
      std::string(dir.As<String>().Utf8Value().c_str()),
      getIgnorePaths(env, opts),
      getIgnoreGlobs(env, opts)
    );

    backend = getBackend(env, opts);
    // watcher->watch(fn.As<Function>());

    auto callback = fn.As<Function>();
    auto tsfn = ThreadSafeFunction::New(
      callback.Env(),
      callback,
      "Watcher callback",
      0, // Unlimited queue
      1 // Initial thread count
    );
    auto data = std::make_shared<NapiCallback>();
    data->tsfn = tsfn;
    data->ref = Napi::Persistent(callback);
    watcher->watch(data);
  }

private:
  WatcherRef watcher;
  std::shared_ptr<Backend> backend;
  FunctionReference callback;

  void execute() override {
    try {
      backend->watch(watcher);
    } catch (std::exception&) {
      watcher->destroy();
      throw;
    }
  }
};

class UnsubscribeRunner : public PromiseRunner {
public:
  UnsubscribeRunner(Env env, Value dir, Value fn, Value opts) : PromiseRunner(env) {
    watcher = Watcher::getShared(
      std::string(dir.As<String>().Utf8Value().c_str()),
      getIgnorePaths(env, opts),
      getIgnoreGlobs(env, opts)
    );

    backend = getBackend(env, opts);

    auto callback = fn.As<Function>();
    auto tsfn = ThreadSafeFunction::New(
      callback.Env(),
      callback,
      "Watcher callback",
      0, // Unlimited queue
      1 // Initial thread count
    );

    auto data = std::make_shared<NapiCallback>();
    data->tsfn = tsfn;
    data->ref = Napi::Persistent(callback);

    shouldUnwatch = watcher->unwatch(data);
  }

private:
  WatcherRef watcher;
  std::shared_ptr<Backend> backend;
  bool shouldUnwatch;

  void execute() override {
    if (shouldUnwatch) {
      backend->unwatch(watcher);
    }
  }
};

template<class Runner>
Value queueSubscriptionWork(const CallbackInfo& info) {
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

  Runner *runner = new Runner(info.Env(), info[0], info[1], info[2]);
  return runner->queue();
}

Value subscribe(const CallbackInfo& info) {
  return queueSubscriptionWork<SubscribeRunner>(info);
}

Value unsubscribe(const CallbackInfo& info) {
  return queueSubscriptionWork<UnsubscribeRunner>(info);
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

NODE_API_MODULE(watcher, Init)
