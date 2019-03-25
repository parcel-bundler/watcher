#ifndef WATCHMAN_H
#define WATCHMAN_H

#include "../Backend.hh"
#include "./BSER.hh"
#include "../Signal.hh"

class WatchmanBackend : public Backend {
public:
  static bool checkAvailable();
  void start() override;
  WatchmanBackend() : mSock(-1), mStopped(false), mEnded(false) {};
  ~WatchmanBackend();
  void writeSnapshot(Watcher &watcher, std::string *snapshotPath) override;
  void getEventsSince(Watcher &watcher, std::string *snapshotPath) override;
  void subscribe(Watcher &watcher) override;
  void unsubscribe(Watcher &watcher) override;
private:
  int mSock;
  Signal mRequestSignal;
  Signal mResponseSignal;
  BSER::Object mResponse;
  std::unordered_map<std::string, Watcher *> mSubscriptions;
  bool mStopped;
  bool mEnded;
  Signal mEndedSignal;

  std::string clock(Watcher &watcher);
  void watchmanWatch(std::string dir);
  BSER::Object watchmanRequest(BSER cmd);
  void handleSubscription(BSER::Object obj);
};

#endif