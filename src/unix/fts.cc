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

void iterateDir(const char *dirname, Watcher &watcher, std::shared_ptr <DirTree> tree) {
    if (DIR * dir = opendir(dirname)) {
        while (struct dirent *ent = (errno = 0, readdir(dir))) {
            if (!Dots(ent->d_name)) {
                // TODO: This can definitely be optimised, seems weird to convert chars to strings back to chars...
                std::string fullPath = std::string(dirname) + "/" + std::string(ent->d_name);

                // TODO: Ignore certain symlinks?  && (ent->d_type == DT_DIR || ent->d_type == DT_REG)
                // For some reason the previous implementation had less files in a snapshot?
                if (watcher.mIgnore.count(fullPath) == 0) {
                    struct stat attrib;
                    stat(fullPath.c_str(), &attrib);
                    bool isDir = ent->d_type == DT_DIR;

                    tree->add(fullPath, CONVERT_TIME(attrib.st_mtim), isDir);

                    if (isDir) {
                        iterateDir(fullPath.c_str(), watcher, tree);
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
    const char *dirname = watcher.mDir.c_str();

    return iterateDir(dirname, watcher, tree);
}
