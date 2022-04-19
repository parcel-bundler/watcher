#ifndef GLOB_H
#define GLOB_H

#include <unordered_set>
#include <regex>

struct Glob {
  std::string mRaw;
  std::regex mRegex;

  Glob(std::string raw);
  Glob(std::string raw, std::regex regex);

  bool operator==(const Glob &other) const {
    return mRaw == other.mRaw;
  }

  bool isIgnored(std::string path) const;
};

namespace std
{
  template <>
  struct hash<Glob>
  {
    size_t operator()(const Glob& g) const {
      return std::hash<std::string>()(g.mRaw);
    }
  };

  template <>
  struct equal_to<Glob>
  {
    size_t operator()(const Glob& a, const Glob& b) const {
      return a.mRaw == b.mRaw;
    }
  };
}

#endif
