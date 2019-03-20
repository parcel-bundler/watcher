#ifndef BRUTE_FORCE_H
#define BRUTE_FORCE_H

#include "../Backend.hh"

class BruteForceBackend : public Backend {
public:
  void writeSnapshot(Watcher &watcher, std::string *snapshotPath) override;
  void getEventsSince(Watcher &watcher, std::string *snapshotPath) override;
  void subscribe(Watcher &watcher) override {
    throw "Brute force backend doesn't support subscriptions.";
  }

  void unsubscribe(Watcher &watcher) override {
    throw "Brute force backend doesn't support subscriptions.";
  }
};

#endif
