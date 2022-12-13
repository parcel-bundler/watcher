#ifndef GLOB_H
#define GLOB_H

#include <unordered_set>
#include <regex>

struct Glob {
  std::size_t mHash;
  std::regex mRegex;
  std::string mRaw;

  Glob(std::string raw);
  Glob(std::string raw, std::regex regex);

  bool operator==(const Glob &other) const {
    return mHash == other.mHash;
  }

  bool isIgnored(std::string relative_path) const;
};

namespace std
{
  template <>
  struct hash<Glob>
  {
    size_t operator()(const Glob& g) const {
      return g.mHash;
    }
  };
}

#endif
