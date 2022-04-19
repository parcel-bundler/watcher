#include "Glob.hh"

Glob::Glob(std::string raw) : Glob(raw, std::regex(raw))
  { }

Glob::Glob(std::string raw, std::regex regex)
  : mRaw(raw),
    mRegex(regex)
  { }

bool Glob::isIgnored(std::string path) {
  // for (auto it = mIgnore.begin(); it != mIgnore.end(); it++) {
  //   auto dir = *it + DIR_SEP;
  //   if (*it == path || path.compare(0, dir.size(), dir) == 0) {
  //     return true;
  //   }
  // }

  return false;
}
