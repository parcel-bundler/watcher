#include "Glob.hh"

#ifdef __wasm32__
extern "C" bool wasm_regex_match(const char *s, const char *regex, bool noCase);
#endif

Glob::Glob(std::string raw, bool noCase) {
  mRaw = raw;
  mHash = std::hash<std::string>()(raw + (noCase ? "_nocase" : ""));
  #ifdef __wasm32__
    mNoCase = noCase;
  #else
    if (noCase) {
      mRegex = std::regex(raw, std::regex::ECMAScript | std::regex::icase);
    } else {
      mRegex = std::regex(raw);
    }
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
