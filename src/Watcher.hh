#ifndef WATCHER_H
#define WATCHER_H

#include <condition_variable>
#include <unordered_set>
#include <set>
#include "Glob.hh"
#include "Event.hh"
#include "Debounce.hh"
#include "DirTree.hh"
#include "Signal.hh"

// using namespace Napi;

struct Watcher;
using WatcherRef = std::shared_ptr<Watcher>;

typedef void (watcher_cb)(void *data, std::string error, std::vector<Event> events);

// struct Callback {
//   // Napi::ThreadSafeFunction tsfn;
//   // Napi::FunctionReference ref;
//   watcher_cb *fn;
//   void *data;
//   std::thread::id threadId;
// };

class Callback {
public:
  std::thread::id threadId;
  virtual void call(std::string error, std::vector<Event> events) = 0;
  virtual bool operator==(const Callback &other) const = 0;
  virtual ~Callback() = default;
};

// typedef std::function<void(std::string error, std::vector<Event> events)> Callback;

class WatcherState {
public:
    virtual ~WatcherState() = default;
};

struct Watcher {
  std::string mDir;
  std::unordered_set<std::string> mIgnorePaths;
  std::unordered_set<Glob> mIgnoreGlobs;
  EventList mEvents;
  std::shared_ptr<WatcherState> state;

  Watcher(std::string dir, std::unordered_set<std::string> ignorePaths, std::unordered_set<Glob> ignoreGlobs);
  ~Watcher();

  bool operator==(const Watcher &other) const {
    return mDir == other.mDir && mIgnorePaths == other.mIgnorePaths && mIgnoreGlobs == other.mIgnoreGlobs;
  }

  void wait();
  void notify();
  void notifyError(std::exception &err);
  bool watch(std::shared_ptr<Callback> callback);
  bool unwatch(std::shared_ptr<Callback> callback);
  void unref();
  bool isIgnored(std::string path);
  void destroy();

  static WatcherRef getShared(std::string dir, std::unordered_set<std::string> ignorePaths, std::unordered_set<Glob> ignoreGlobs);

private:
  std::mutex mMutex;
  std::condition_variable mCond;
  std::vector<std::shared_ptr<Callback>> mCallbacks;
  std::shared_ptr<Debounce> mDebounce;

  std::vector<std::shared_ptr<Callback>>::iterator findCallback(std::shared_ptr<Callback> callback);
  void clearCallbacks();
  void triggerCallbacks();
};

class WatcherError : public std::runtime_error {
public:
  WatcherRef mWatcher;
  WatcherError(std::string msg, WatcherRef watcher) : std::runtime_error(msg), mWatcher(watcher) {}
  WatcherError(const char *msg, WatcherRef watcher) : std::runtime_error(msg), mWatcher(watcher) {}
};

#endif
