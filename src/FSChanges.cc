#include <node.h>
#include <uv.h>
#include <v8.h>
#include <nan.h>
#include "Event.hh"

using namespace v8;

void writeSnapshotImpl(std::string *dir, std::string *snapshotPath);
EventList *getEventsSinceImpl(std::string *dir, std::string *snapshotPath);

struct AsyncRequest {
  uv_work_t work;
  std::string directory;
  std::string snapshotPath;
  EventList *events;
  Nan::Persistent<Promise::Resolver> *resolver;

  AsyncRequest(Local<Value> d, Local<Value> s, Local<Promise::Resolver> r) {
    work.data = (void *)this;

    // copy the string since the JS garbage collector might run before the async request is finished
    Nan::Utf8String dir(d);
    Nan::Utf8String sp(s);
    directory = std::string(*dir);
    snapshotPath = std::string(*sp);
    events = NULL;

    resolver = new Nan::Persistent<Promise::Resolver>(r);
  }

  ~AsyncRequest() {
    if (events) {
      delete events;
    }

    resolver->Reset();
  }
};

void asyncCallback(uv_work_t *work) {
  Nan::HandleScope scope;
  AsyncRequest *req = (AsyncRequest *) work->data;
  Nan::AsyncResource async("asyncCallback");
  Local<Value> result;

  if (req->events) {
    result = req->events->toJS();
  } else {
    result = Nan::Null();
  }

  auto resolver = Nan::New(*req->resolver);
  resolver->Resolve(result);
  delete req;
}

void writeSnapshotAsync(uv_work_t *work) {
  AsyncRequest *req = (AsyncRequest *) work->data;
  writeSnapshotImpl(&req->directory, &req->snapshotPath);
}

void getEventsSinceAsync(uv_work_t *work) {
  AsyncRequest *req = (AsyncRequest *) work->data;
  req->events = getEventsSinceImpl(&req->directory, &req->snapshotPath);
}

NAN_METHOD(writeSnapshot) {
  if (info.Length() < 1 || !info[0]->IsString()) {
    return Nan::ThrowTypeError("Expected a string");
  }

  auto resolver = Promise::Resolver::New(info.GetIsolate());
  AsyncRequest *req = new AsyncRequest(info[0], info[1], resolver);
  uv_queue_work(uv_default_loop(), &req->work, writeSnapshotAsync, (uv_after_work_cb) asyncCallback);

  info.GetReturnValue().Set(resolver->GetPromise());
}

NAN_METHOD(getEventsSince) {
  if (info.Length() < 1 || !info[0]->IsString()) {
    return Nan::ThrowTypeError("Expected a string");
  }

  if (info.Length() < 2 || !info[1]->IsString()) {
    return Nan::ThrowTypeError("Expected a string");
  }

  auto resolver = Promise::Resolver::New(info.GetIsolate());
  AsyncRequest *req = new AsyncRequest(info[0], info[1], resolver);
  uv_queue_work(uv_default_loop(), &req->work, getEventsSinceAsync, (uv_after_work_cb) asyncCallback);

  info.GetReturnValue().Set(resolver->GetPromise());
}

NAN_MODULE_INIT(Init) {
  Nan::Export(target, "writeSnapshot", writeSnapshot);
  Nan::Export(target, "getEventsSince", getEventsSince);
}

NODE_MODULE(fschanges, Init)
