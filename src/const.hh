#ifndef CONST_H
#define CONST_H

#include <ios>
#include <istream>

const ino_t FAKE_INO = 0;
const std::string FAKE_FILEID = "";
enum Kind{IS_FILE, IS_DIR, IS_UNKNOWN};

inline std::istream &operator>>(std::istream &str, Kind &v) {
  unsigned int kind = IS_UNKNOWN;
  if (!(str >> kind))
    return str;
  if (kind >= IS_UNKNOWN) {
    str.setstate(str.rdstate() | std::ios::failbit);
    return str;
  }
  v = static_cast<Kind>(kind);
  return str;
}

#endif
