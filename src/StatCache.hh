#ifndef STAT_CACHE_H
#define STAT_CACHE_H

#include <string>

namespace StatCache {
  void set(std::string path, int result);
  int get(std::string path);
}

#endif
