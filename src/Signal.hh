#ifndef SIGNAL_H
#define SIGNAL_H

#include <mutex>
#include <condition_variable>

class Signal {
public:
  void wait() {
    std::unique_lock<std::mutex> lock(mMutex);
    mCond.wait(lock);
  }

  void notify() {
    mCond.notify_all();
  }

private:
  std::mutex mMutex;
  std::condition_variable mCond;
};

#endif
