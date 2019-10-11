#include "StatCache.hh"
#include <unordered_map>
#include <sys/stat.h>

static std::unordered_map<std::string, int> statCache;

void StatCache::set(std::string path, int result) {
  statCache.emplace(path, result);
}

int StatCache::get(std::string path) {
  auto found = statCache.find(path);

  if (found != statCache.end()) {
    return found->second;
  }

  struct stat st;
  int result = stat(path.c_str(), &st);
  if (result == 0) {
    result = S_ISDIR(st.st_mode);
  }

  StatCache::set(path, result);
  return result;
}
