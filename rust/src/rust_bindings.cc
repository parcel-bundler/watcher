#include <unordered_set>
#include "Glob.hh"
#include "Event.hh"
#include "Backend.hh"
#include "Watcher.hh"

typedef void (c_watcher_cb)(void *data, const char *error, Event *events, int len);

class RustCallback: public Callback {
public:
  c_watcher_cb *fn;
  void *data;

  virtual void call(std::string error, std::vector<Event> events) {
    const char *c_error = error.empty() ? NULL : error.c_str();
    (fn)(data, c_error, events.data(), events.size());
  }

  virtual bool operator==(const Callback &other) const {
    if (const RustCallback *cb = dynamic_cast<const RustCallback*>(&other)) {
      return fn == cb->fn && data == cb->data;
    } else {
      return false;
    }
  }

  virtual ~RustCallback() {}
};

extern "C" void parcel_watcher_subscribe(const char *dir, c_watcher_cb *cb, void *data) {
  auto watcher = Watcher::getShared(
    std::string(dir),
    {},
    {}
  );

  auto d = std::make_shared<RustCallback>();
  d->fn = cb;
  d->data = data;

  bool watched = watcher->watch(d);
  if (watched) {
    auto backend = Backend::getShared(std::string("default"));

    try {
      backend->watch(watcher);
    } catch (std::exception &err) {
      watcher->destroy();
    } catch (const char *e) {
      printf("ERROR: %s\n", e);
    }
  }
}

extern "C" const char *parcel_watcher_event_get_path(const Event *events, int index) {
  return events[index].path.c_str();
}

extern "C" int parcel_watcher_event_get_type(const Event *events, int index) {
  return events[index].isCreated ? 0 : events[index].isDeleted ? 1 : 2;
}

extern "C" void parcel_watcher_unsubscribe(const char *dir, c_watcher_cb *cb, void *data) {
  auto watcher = Watcher::getShared(
    std::string(dir),
    {},
    {}
  );

  auto d = std::make_shared<RustCallback>();
  d->fn = cb;
  d->data = data;

  watcher->unwatch(d);
}
