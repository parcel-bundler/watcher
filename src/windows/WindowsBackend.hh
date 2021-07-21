#ifndef WINDOWS_H
#define WINDOWS_H

#include <windows.h>
#include <winsock2.h>

#include "../shared/BruteForceBackend.hh"

class WindowsBackend : public BruteForceBackend {
 public:
  void start() override;
  ~WindowsBackend();
  void subscribe(Watcher &watcher) override;
  void unsubscribe(Watcher &watcher) override;

 private:
  bool mRunning;
};

#endif
