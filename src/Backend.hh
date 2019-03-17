#ifndef BACKEND_H
#define BACKEND_H

#include "./Event.hh"

struct Backend {
  std::string mDir;
  std::unordered_set<std::string> mIgnore;
  Backend(std::string dir, std::unordered_set<std::string> ignore) : mDir(dir), mIgnore(ignore) {}
  virtual void writeSnapshot(std::string *snapshotPath) = 0;
  virtual EventList *getEventsSince(std::string *snapshotPath) = 0;
  virtual ~Backend() {}
};

#ifdef FS_EVENTS
struct FSEventsBackend : public Backend {
  FSEventsBackend(std::string dir, std::unordered_set<std::string> ignore) : Backend(dir, ignore) {}
  void writeSnapshot(std::string *snapshotPath) override;
  EventList *getEventsSince(std::string *snapshotPath) override;
};
#endif

#ifdef WATCHMAN
struct WatchmanBackend : public Backend {
  static bool check();
  WatchmanBackend(std::string dir, std::unordered_set<std::string> ignore) : Backend(dir, ignore) {}
  void writeSnapshot(std::string *snapshotPath) override;
  EventList *getEventsSince(std::string *snapshotPath) override;
};
#endif

struct BruteForceBackend : public Backend {
  BruteForceBackend(std::string dir, std::unordered_set<std::string> ignore) : Backend(dir, ignore) {}
  void writeSnapshot(std::string *snapshotPath) override;
  EventList *getEventsSince(std::string *snapshotPath) override;
};

#endif
