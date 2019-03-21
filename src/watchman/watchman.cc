#include <string>
#include <fstream>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "../DirTree.hh"
#include "../Event.hh"
#include "./BSER.hh"
#include "./watchman.hh"

template<typename T>
BSER readBSER(T &&do_read) {
  std::stringstream oss;
  char buffer[256];
  int r;
  int64_t len = -1;
  do {
    r = do_read(buffer, sizeof(buffer));
    if (r < 0) {
      throw strerror(errno);
    }

    oss << std::string(buffer, r);

    if (len == -1) {
      len = BSER::decodeLength(oss);
    }

    len -= r;
  } while (len > 0);

  return BSER(oss);
}

std::string getSockPath() {
  auto var = getenv("WATCHMAN_SOCK");
  if (var && *var) {
    return std::string(var);
  }

  FILE *fp;
  fp = popen("watchman --output-encoding=bser get-sockname", "r");
  if (fp == NULL || errno == ECHILD) {
    throw "Failed to execute watchman";
  }

  BSER b = readBSER([fp] (char *buf, size_t len) {
    return fread(buf, sizeof(char), len, fp);
  });

  pclose(fp);
  return b.objectValue().find("sockname")->second.stringValue();
}

int watchmanConnect() {
  std::string path = getSockPath();
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (connect(sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_un))) {
    throw "Error connecting to watchman";
  }

  return sock;
}

BSER watchmanRead(int sock) {
  return readBSER([sock] (char *buf, size_t len) {
    return read(sock, buf, len);
  });
}

BSER::Object WatchmanBackend::watchmanRequest(BSER b) {
  std::string cmd = b.encode();

  int r = 0;
  for (unsigned int i = 0; i != cmd.size(); i += r) {
    r = write(mSock, &cmd[i], cmd.size() - i);
    if (r == -1) {
      if (errno == EAGAIN) {
        r = 0;
      } else {
        throw "Write error";
      }
    }
  }

  mRequestSignal.notify();
  mResponseSignal.wait();
  return mResponse;
}

void WatchmanBackend::watchmanWatch(std::string dir) {
  std::vector<BSER> cmd;
  cmd.push_back("watch-project");
  cmd.push_back(dir);
  watchmanRequest(cmd);
}

bool WatchmanBackend::checkAvailable() {
  try {
    int sock = watchmanConnect();
    close(sock);
    return true;
  } catch (const char *err) {
    return false;
  }
}

void handleFiles(Watcher &watcher, BSER::Object obj) {
  auto found = obj.find("files");
  if (found == obj.end()) {
    throw "Error reading changes from watchman";
  }
  
  auto files = found->second.arrayValue();
  for (auto it = files.begin(); it != files.end(); it++) {
    auto file = it->objectValue();
    auto name = file.find("name")->second.stringValue();
    auto isNew = file.find("new")->second.boolValue();
    auto exists = file.find("exists")->second.boolValue();
    if (isNew && exists) {
      watcher.mEvents.push(name, "create");
    } else if (exists) {
      watcher.mEvents.push(name, "update");
    } else if (!isNew) {
      watcher.mEvents.push(name, "delete");
    }
  }
}

void WatchmanBackend::handleSubscription(BSER::Object obj) {
  std::unique_lock<std::mutex> lock(mMutex);
  auto root = obj.find("root")->second.stringValue();
  auto it = mSubscriptions.find(root);
  if (it == mSubscriptions.end()) {
    return;
  }

  auto watcher = it->second;
  handleFiles(*watcher, obj);
  watcher->notify();
}

void WatchmanBackend::start() {
  mSock = watchmanConnect();
  notifyStarted();

  while (true) {
    // If there are no subscriptions we are reading, wait for a request.
    if (mSubscriptions.size() == 0) {
      mRequestSignal.wait();
    }

    // Break out of loop if we are stopped.
    if (mStopped) {
      break;
    }

    // Attempt to read from the socket.
    // If there is an error and we are stopped, break.
    BSER b;
    try {
      b = watchmanRead(mSock);
    } catch (const char *err) {
      if (mStopped) {
        break;
      } else {
        throw err;
      }
    }

    auto obj = b.objectValue();
    auto error = obj.find("error");
    if (error != obj.end()) {
      throw error->second.stringValue().c_str();
    }

    // If this message is for a subscription, handle it, otherwise notify the request.
    auto subscription = obj.find("subscription");
    if (subscription != obj.end()) {
      handleSubscription(obj);
    } else {
      mResponse = obj;
      mResponseSignal.notify();
    }
  }

  mEnded = true;
  mEndedSignal.notify();
}

WatchmanBackend::~WatchmanBackend() {
  std::unique_lock<std::mutex> lock(mMutex);

  // Mark the watcher as stopped, close the socket, and trigger the lock.
  // This will cause the read loop to be broken and the thread to exit.
  mStopped = true;
  close(mSock);
  mRequestSignal.notify();

  // If not ended yet, wait.
  if (!mEnded) {
    mEndedSignal.wait();
  }
}

std::string WatchmanBackend::clock(Watcher &watcher) {
  BSER::Array cmd;
  cmd.push_back("clock");
  cmd.push_back(watcher.mDir);

  BSER::Object obj = watchmanRequest(cmd);
  auto found = obj.find("clock");
  if (found == obj.end()) {
    throw "Error reading clock from watchman";
  }

  return found->second.stringValue();
}

void WatchmanBackend::writeSnapshot(Watcher &watcher, std::string *snapshotPath) {
  std::unique_lock<std::mutex> lock(mMutex);
  watchmanWatch(watcher.mDir);

  std::ofstream ofs(*snapshotPath);
  ofs << clock(watcher);
}

void WatchmanBackend::getEventsSince(Watcher &watcher, std::string *snapshotPath) {
  std::unique_lock<std::mutex> lock(mMutex);
  std::ifstream ifs(*snapshotPath);
  if (ifs.fail()) {
    return;
  }

  std::string clock;
  ifs >> clock;

  BSER::Array cmd;
  cmd.push_back("since");
  cmd.push_back(watcher.mDir);
  cmd.push_back(clock);

  BSER::Object obj = watchmanRequest(cmd);
  handleFiles(watcher, obj);
}

void WatchmanBackend::subscribe(Watcher &watcher) {
  mSubscriptions.emplace(watcher.mDir, &watcher);

  BSER::Array cmd;
  cmd.push_back("subscribe");
  cmd.push_back(watcher.mDir);
  cmd.push_back("fschanges");

  BSER::Array fields;
  fields.push_back("name");
  fields.push_back("exists");
  fields.push_back("new");

  BSER::Object opts;
  opts.emplace("fields", fields);
  opts.emplace("since", clock(watcher));
  cmd.push_back(opts);

  watchmanRequest(cmd);
}

void WatchmanBackend::unsubscribe(Watcher &watcher) {
  mSubscriptions.erase(watcher.mDir);
  
  BSER::Array cmd;
  cmd.push_back("unsubscribe");
  cmd.push_back(watcher.mDir);
  cmd.push_back("fschanges");

  watchmanRequest(cmd);
}
