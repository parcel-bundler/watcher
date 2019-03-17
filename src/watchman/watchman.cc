#include <string>
#include <fstream>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "../DirTree.hh"
#include "../Event.hh"
#include "./BSER.hh"

static int sock = -1;

template<typename T>
BSER readBSER(T &&do_read) {
  std::stringstream oss;
  char buffer[256];
  int r;
  int64_t len = -1;
  do {
    r = do_read(buffer, sizeof(buffer));
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
  fp = popen("/bin/sh -c 'watchman --output-encoding=bser get-sockname'", "r");
  if (fp == NULL) {
    printf("Failed to run command\n");
    exit(1);
  }

  BSER b = readBSER([fp] (char *buf, size_t len) {
    return fread(buf, sizeof(char), len, fp);
  });

  pclose(fp);
  return b.objectValue().find("sockname")->second.stringValue();
}

void watchmanConnect() {
  if (sock != -1) {
    return;
  }

  std::string path = getSockPath();
  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  std::strncpy(addr.sun_path, path.c_str(), path.size());

  sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (connect(sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_un))) {
    printf("error connecting\n");
    exit(1);
  }
}

void watchmanWrite(BSER b) {
  std::string cmd = b.encode();

  int r = 0;
  for (unsigned int i = 0; i != cmd.size(); i += r) {
    r = write(sock, &cmd[i], cmd.size() - i);
    if (r == -1) {
      if (errno == EAGAIN) {
        r = 0;
      } else {
        printf("write error\n");
        exit(1);
      }
    }
  }
}

BSER watchmanRead() {
  return readBSER([] (char *buf, size_t len) {
    return read(sock, buf, len);
  });
}

void watchmanWatch(std::string *dir) {
  std::vector<BSER> cmd;
  cmd.push_back("watch-project");
  cmd.push_back(*dir);
  watchmanWrite(cmd);

  BSER b = watchmanRead();
  auto obj = b.objectValue();
  auto error = obj.find("error");
  if (error != obj.end()) {
    printf("error watching project\n");
    exit(1);
  }
}

void writeSnapshotImpl(std::string *dir, std::string *snapshotPath, std::unordered_set<std::string> *ignore) {
  watchmanConnect();
  watchmanWatch(dir);

  std::vector<BSER> cmd;
  cmd.push_back("clock");
  cmd.push_back(*dir);
  watchmanWrite(cmd);

  BSER b = watchmanRead();
  auto obj = b.objectValue();
  auto found = obj.find("clock");
  if (found == obj.end()) {
    printf("error\n");
    exit(1);
  }

  auto str = found->second.stringValue();
  std::ofstream ofs(*snapshotPath);
  ofs << str;
}

EventList *getEventsSinceImpl(std::string *dir, std::string *snapshotPath, std::unordered_set<std::string> *ignore) {
  EventList *list = new EventList();

  std::ifstream ifs(*snapshotPath);
  if (ifs.fail()) {
    return list;
  }

  std::string clock;
  ifs >> clock;

  watchmanConnect();

  std::vector<BSER> cmd;
  cmd.push_back("since");
  cmd.push_back(*dir);
  cmd.push_back(clock);
  watchmanWrite(cmd);

  BSER b = watchmanRead();
  auto obj = b.objectValue();
  auto found = obj.find("files");
  if (found == obj.end()) {
    printf("error\n");
    exit(1);
  }
  
  auto files = found->second.arrayValue();
  for (auto it = files.begin(); it != files.end(); it++) {
    auto file = it->objectValue();
    auto name = file.find("name")->second.stringValue();
    auto isNew = file.find("new")->second.boolValue();
    auto exists = file.find("exists")->second.boolValue();
    if (isNew && exists) {
      list->push(name, "create");
    } else if (exists) {
      list->push(name, "update");
    } else if (!isNew) {
      list->push(name, "delete");
    }
  }

  return list;
}
