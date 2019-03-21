#ifndef DEBOUNCE_H
#define DEBOUNCE_H

#include <thread>
#include "Signal.hh"

class Debounce {
public:
  static std::shared_ptr<Debounce> getShared() {
    static std::weak_ptr<Debounce> sharedInstance;
    std::shared_ptr<Debounce> shared = sharedInstance.lock();
    if (!shared) {
      shared = std::make_shared<Debounce>();
      sharedInstance = shared;
    }

    return shared;
  }

  Debounce() {
    mRunning = true;
    mTriggered = false;
    mThread = std::thread([this] () {
      loop();
    });
  }

  ~Debounce() {
    mRunning = false;
    mWaitSignal.notify();
    mThread.join();
  }

  void add(std::function<void()> cb) {
    std::unique_lock<std::mutex> lock(mMutex);
    mCallbacks.push_back(cb);
  }

  void trigger() {
    std::unique_lock<std::mutex> lock(mMutex);
    mTriggered = true;
    mWaitSignal.notify();
  }
  
private:
  bool mRunning;
  bool mTriggered;
  std::mutex mMutex;
  Signal mWaitSignal;
  Signal mNotifySignal;
  std::thread mThread;
  std::vector<std::function<void()>> mCallbacks;

  void loop() {
    while (mRunning) {
      if (!mTriggered) {
        mWaitSignal.wait();
      }

      auto status = mWaitSignal.waitFor(std::chrono::milliseconds(500));
      if (status == std::cv_status::timeout) {
        notify();
      }
    }
  }

  void notify() {
    std::unique_lock<std::mutex> lock(mMutex);

    for (auto it = mCallbacks.begin(); it != mCallbacks.end(); it++) {
      auto cb = *it;
      cb();
    }

    mTriggered = false;
  }
};

#endif
