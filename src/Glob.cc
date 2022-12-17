#include "Glob.hh"
#include <iostream>

Glob::Glob(std::string raw) : Glob(raw, std::regex(raw))
  { }

Glob::Glob(std::string raw, std::regex regex)
  : mHash(std::hash<std::string>()(raw)),
    mRegex(regex),
    mRaw(raw)
  { }

bool Glob::isIgnored(std::string relative_path) const {
  return std::regex_match(relative_path, mRegex);
}
