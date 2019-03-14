#include <string>
#include <sstream>
#include "../DirTree.hh"
#include "../Event.hh"

DirTree *getDirTree(std::string *dir);

std::string getCurrentTokenImpl(std::string *dir) {
  auto tree = getDirTree(dir);
  std::ostringstream os;
  tree->write(os);
  return os.str();
}

EventList *getEventsSinceImpl(std::string *dir, std::string *token) {
  std::istringstream is(*token);
  auto snapshot = new DirTree(is);
  auto now = getDirTree(dir);
  return now->getChanges(snapshot);
}
