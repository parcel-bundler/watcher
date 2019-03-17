#ifndef BACKEND_H
#define BACKEND_H

#include "./Event.hh"

#ifdef FS_EVENTS
struct FSEventsBackend {
  static void writeSnapshot(std::string *dir, std::string *snapshotPath, std::unordered_set<std::string> *ignore);
  static EventList *getEventsSince(std::string *dir, std::string *snapshotPath, std::unordered_set<std::string> *ignore);
};
#endif

#ifdef WATCHMAN
struct WatchmanBackend {
  static bool check();
  static void writeSnapshot(std::string *dir, std::string *snapshotPath, std::unordered_set<std::string> *ignore);
  static EventList *getEventsSince(std::string *dir, std::string *snapshotPath, std::unordered_set<std::string> *ignore);
};
#endif

struct BruteForceBackend {
  static void writeSnapshot(std::string *dir, std::string *snapshotPath, std::unordered_set<std::string> *ignore);
  static EventList *getEventsSince(std::string *dir, std::string *snapshotPath, std::unordered_set<std::string> *ignore);
};

// Use FSEvents on macOS by default.
// Use watchman by default if available on other platforms.
// Fall back to brute force.
#ifdef FS_EVENTS
#define DEFAULT_BACKEND(method) FSEventsBackend::method
#elif WATCHMAN
#define DEFAULT_BACKEND(method) (WatchmanBackend::check() ? WatchmanBackend::method : BruteForceBackend::method)
#else
#define DEFAULT_BACKEND(method) BruteForceBackend::method
#endif

#define GET_BACKEND(backend, method) \
  (backend == "watchman" && WATCHMAN && WatchmanBackend::check() ? WatchmanBackend::method \
    : backend == "fs-events" && FS_EVENTS ? FSEventsBackend::method \
    : backend == "brute-force" ? BruteForceBackend::method \
    : DEFAULT_BACKEND(method))

#endif
