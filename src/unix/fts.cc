#include <string>

#define __THROW // weird error on linux

#include <sys/stat.h>
#include <dirent.h>

#include "../DirTree.hh"
#include "../shared/BruteForceBackend.hh"

#define CONVERT_TIME(ts) ((uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec)
#if __APPLE__
#define st_mtim st_mtimespec
#endif

bool Dots(const char *s) {
    return s[0] == '.' && (!s[1] || (s[1] == '.' && !s[2]));
}

// TODO: Figure out why some tests fail...
void iterateDir(Watcher &watcher, const std::shared_ptr <DirTree> tree, const char *dirname) {
    if (DIR * dir = opendir(dirname)) {
        while (struct dirent *ent = (errno = 0, readdir(dir))) {
            const char* fileName = ent->d_name;

            if (!Dots(fileName)) {
                std::string fullPath = dirname + std::string("/") + fileName;

                if (watcher.mIgnore.count(fullPath) == 0) {
                    const char* fullPathChars = fullPath.c_str();

                    struct stat attrib;
                    stat(fullPathChars, &attrib);
                    bool isDir = ent->d_type == DT_DIR;

                    tree->add(fullPath, CONVERT_TIME(attrib.st_mtim), isDir);

                    if (isDir) {
                        iterateDir(watcher, tree, fullPathChars);
                    }
                }
            }
        }

        closedir(dir);
    }

    if (errno) {
        throw WatcherError(strerror(errno), &watcher);
    }
}

void BruteForceBackend::readTree(Watcher &watcher, std::shared_ptr <DirTree> tree) {
    return iterateDir(watcher, tree, watcher.mDir.c_str());
}
