#ifndef GLOB_H
#define GLOB_H

#include <unordered_set>
#include <regex>

struct Glob {
  std::size_t mHash;
  std::string mRaw;
  #ifdef __wasm32__
  bool mNoCase;
  #else
  std::regex mRegex;
  #endif

  Glob(std::string raw, bool noCase = false);

  bool operator==(const Glob &other) const {
    return mHash == other.mHash && mRaw == other.mRaw;
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
