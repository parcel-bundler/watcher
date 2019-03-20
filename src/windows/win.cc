#include <string>
#include <sstream>
#include <stack>
#include <winsock2.h>
#include <windows.h>
#include "../DirTree.hh"
#include "../shared/BruteForceBackend.hh"

#define CONVERT_TIME(ft) ULARGE_INTEGER{ft.dwLowDateTime, ft.dwHighDateTime}.QuadPart

DirTree *BruteForceBackend::getDirTree(std::string *dir, std::unordered_set<std::string> *ignore) {
  DirTree *tree = new DirTree();
  HANDLE hFind = INVALID_HANDLE_VALUE;
  std::stack<std::string> directories;
  
  directories.push(*dir);

  while (!directories.empty()) {
    std::string path = directories.top();
    std::string spec = path + "\\*";
    directories.pop();

    WIN32_FIND_DATA ffd;
    hFind = FindFirstFile(spec.c_str(), &ffd);

    if (hFind == INVALID_HANDLE_VALUE)  {
      printf("error\n");
    }

    do {
      if (strcmp(ffd.cFileName, ".") != 0 && strcmp(ffd.cFileName, "..") != 0) {
        std::string fullPath = path + "\\" + ffd.cFileName;
        tree->entries.insert(DirEntry(fullPath, CONVERT_TIME(ffd.ftLastWriteTime)));
        if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && ignore->count(fullPath) == 0) {
          directories.push(fullPath);
        }
      }
    } while (FindNextFile(hFind, &ffd) != 0);
  }

  FindClose(hFind);
  return tree;
}
