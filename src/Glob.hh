#ifndef GLOB_H
#define GLOB_H

#include <unordered_set>
#include <regex>

struct Glob {
  std::size_t mHash;
  std::string mRaw;
  bool mNegated;
  #ifndef __wasm32__
  std::regex mRegex;
  #endif

  Glob(std::string raw);

  bool operator==(const Glob &other) const {
    return mHash == other.mHash;
  }

  bool matches(std::string relative_path) const;
  bool isNegated() const { return mNegated; }
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
