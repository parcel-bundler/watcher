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
#define ISDOT(a) (a[0] == '.' && (!a[1] || (a[1] == '.' && !a[2])))

void iterateDir(Watcher &watcher, const std::shared_ptr <DirTree> tree, const char *dirname) {
    if (DIR *dir = opendir(dirname)) {
        while (struct dirent *ent = (errno = 0, readdir(dir))) {
            if (ISDOT(ent->d_name)) continue;

            std::string fullPath = dirname + std::string("/") + ent->d_name;

            if (watcher.mIgnore.count(fullPath) == 0) {
                struct stat attrib;
                fstatat(dir->__dd_fd, fullPath.c_str(), &attrib, AT_SYMLINK_NOFOLLOW);
                bool isDir = ent->d_type == DT_DIR;

                tree->add(fullPath, CONVERT_TIME(attrib.st_mtim), isDir);

                if (isDir) {
                    iterateDir(watcher, tree, fullPath.c_str());
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
