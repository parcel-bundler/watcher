#ifndef FANOTIFY_CRAWL_H
#define FANOTIFY_CRAWL_H

#include <unordered_map>

#include "FAnotifyBackend.hh"
#include "../Signal.hh"

class FAnotifyCrawlBackend : public FAnotifyBackend {
public:
  void start() override;
  ~FAnotifyCrawlBackend();
  void subscribe(Watcher& watcher) override;
  void unsubscribe(Watcher& watcher) override;

private:
  int mPipe[2];
  int mFAnotifyFd;
  std::unordered_multimap<std::string, std::shared_ptr<FAnotifySubscription>> mSubscriptions;
  Signal mEndedSignal;

  bool watchDir(Watcher& watcher, const std::string& path, std::shared_ptr<DirTree> tree);
  void handleEvents();
  void handleEvent(fanotify_event_metadata* metadata, fanotify_event_info_fid* fid, std::unordered_set<Watcher*>& watchers);
  bool handleSubscription(fanotify_event_metadata* metadata, fanotify_event_info_fid* fid, std::shared_ptr<FAnotifySubscription> sub);
};

#endif
