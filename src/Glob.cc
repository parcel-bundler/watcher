#include "Glob.hh"

#ifdef __wasm32__
extern "C" bool wasm_regex_match(const char *s, const char *regex, bool nocase);
#endif

Glob::Glob(std::string raw, bool nocase) {
  mRaw = raw;
  mHash = std::hash<std::string>()(raw + (nocase ? "_nocase" : ""));
  #ifdef __wasm32__
    mNoCase = nocase;
  #else
    mRegex = std::regex(raw, nocase ? (std::regex::ECMAScript | std::regex::icase) : std::regex::ECMAScript);
  #endif
}

bool Glob::isIgnored(std::string relative_path) const {
  // Use native JS regex engine for wasm to reduce binary size.
  #ifdef __wasm32__
    return wasm_regex_match(relative_path.c_str(), mRaw.c_str(), mNoCase);
  #else
    return std::regex_match(relative_path, mRegex);
  #endif
}
