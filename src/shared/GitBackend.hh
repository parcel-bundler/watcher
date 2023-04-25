#ifndef GIT_BACKEND_H
#define GIT_BACKEND_H

#include "../Backend.hh"
#include "../DirTree.hh"
#include "../Watcher.hh"

class GitBackend : public Backend {
public:
  void writeSnapshot(Watcher &watcher, std::string *snapshotPath) override;
  void getEventsSince(Watcher &watcher, std::string *snapshotPath) override;
  void subscribe(Watcher &watcher) override {
    throw "Brute force backend doesn't support subscriptions.";
  }

  void unsubscribe(Watcher &watcher) override {
    throw "Brute force backend doesn't support subscriptions.";
  }
private:
  void readTree(Watcher &watcher, std::shared_ptr<DirTree> tree);
};

#endif
