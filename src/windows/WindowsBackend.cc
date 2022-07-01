#include <string>
#include <stack>
#include "../DirTree.hh"
#include "../shared/BruteForceBackend.hh"
#include "./WindowsBackend.hh"
#include "./win_utils.hh"

#define DEFAULT_BUF_SIZE 1024 * 1024
#define NETWORK_BUF_SIZE 64 * 1024
#define CONVERT_TIME(ft) ULARGE_INTEGER{ft.dwLowDateTime, ft.dwHighDateTime}.QuadPart

bool isDir(DWORD fileAttributes) {
  // Returns true if file attributes contain the directory attribute but not the
  // symlink attribute.
  return (fileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      && !(fileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
}

void BruteForceBackend::readTree(Watcher &watcher, std::shared_ptr<DirTree> tree) {
  std::stack<std::string> directories;

  directories.push(watcher.mDir);

  while (!directories.empty()) {
    HANDLE hFind = INVALID_HANDLE_VALUE;

    std::string path = directories.top();
    std::string spec = path + "\\*";
    directories.pop();

    WIN32_FIND_DATAW ffd;
    hFind = FindFirstFileW(extendedWidePath(spec).data(), &ffd);

    if (hFind == INVALID_HANDLE_VALUE)  {
      if (path == watcher.mDir) {
        FindClose(hFind);
        throw WatcherError("Error opening directory", &watcher);
      }

      tree->remove(path);
      continue;
    }

    do {
      if (wcscmp(ffd.cFileName, L".") != 0 && wcscmp(ffd.cFileName, L"..") != 0) {
        std::string fullPath = path + "\\" + utf16ToUtf8(ffd.cFileName, sizeof(ffd.cFileName));
        if (watcher.mIgnore.count(fullPath) > 0) {
          continue;
        }

        Kind kind = isDir(ffd.dwFileAttributes) ? IS_DIR : IS_FILE;
        std::string fileId = getFileId(fullPath);
        tree->add(fullPath, FAKE_INO, CONVERT_TIME(ffd.ftLastWriteTime), kind, fileId);
        if (kind == IS_DIR) {
          directories.push(fullPath);
        }
      }
    } while (FindNextFileW(hFind, &ffd) != 0);

    FindClose(hFind);
  }
}

void WindowsBackend::start() {
  mRunning = true;
  notifyStarted();

  while (mRunning) {
    SleepEx(INFINITE, true);
  }
}

WindowsBackend::~WindowsBackend() {
  // Mark as stopped, and queue a noop function in the thread to break the loop
  mRunning = false;
  QueueUserAPC([](__in ULONG_PTR) {}, mThread.native_handle(), (ULONG_PTR)this);
}

class Subscription {
public:
  Subscription(WindowsBackend *backend, Watcher *watcher, std::shared_ptr<DirTree> tree) {
    mRunning = true;
    mBackend = backend;
    mWatcher = watcher;
    mTree = tree;
    ZeroMemory(&mOverlapped, sizeof(OVERLAPPED));
    mOverlapped.hEvent = this;
    mReadBuffer.resize(DEFAULT_BUF_SIZE);
    mWriteBuffer.resize(DEFAULT_BUF_SIZE);

    mDirectoryHandle = CreateFileW(
      extendedWidePath(watcher->mDir).data(),
      FILE_LIST_DIRECTORY,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      NULL,
      OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
      NULL
    );

    if (mDirectoryHandle == INVALID_HANDLE_VALUE) {
      throw WatcherError("Invalid handle", mWatcher);
    }

    // Ensure that the path is a directory
    BY_HANDLE_FILE_INFORMATION info;
    bool success = GetFileInformationByHandle(
      mDirectoryHandle,
      &info
    );

    if (!success) {
      throw WatcherError("Could not get file information", mWatcher);
    }

    if (!isDir(info.dwFileAttributes)) {
      throw WatcherError("Not a directory", mWatcher);
    }
  }

  ~Subscription() {
    stop();
  }

  void run() {
    try {
      poll();
    } catch (WatcherError &err) {
      mBackend->handleWatcherError(err);
    }
  }

  void stop() {
    if (mRunning) {
      mRunning = false;
      CancelIo(mDirectoryHandle);
      CloseHandle(mDirectoryHandle);
    }
  }

  void poll() {
    if (!mRunning) {
      return;
    }

    // Asynchronously wait for changes.
    int success = ReadDirectoryChangesW(
      mDirectoryHandle,
      mWriteBuffer.data(),
      static_cast<DWORD>(mWriteBuffer.size()),
      TRUE, // recursive
      FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES
        | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
      NULL,
      &mOverlapped,
      [](DWORD errorCode, DWORD numBytes, LPOVERLAPPED overlapped) {
        auto subscription = reinterpret_cast<Subscription *>(overlapped->hEvent);
        try {
          subscription->processEvents(errorCode);
        } catch (WatcherError &err) {
          subscription->mBackend->handleWatcherError(err);
        }
      }
    );

    if (!success) {
      throw WatcherError("Failed to read changes", mWatcher);
    }

    auto now = std::chrono::system_clock::now();
    for (auto it = pendingMoves.begin(); it != pendingMoves.end();) {
      if (now - it->second.created > std::chrono::seconds(5)) {
        it = pendingMoves.erase(it);
      } else {
        ++it;
      }
    }
  }

  void processEvents(DWORD errorCode) {
    if (!mRunning) {
      return;
    }

    switch (errorCode) {
      case ERROR_OPERATION_ABORTED:
        return;
      case ERROR_INVALID_PARAMETER:
        // resize buffers to network size (64kb), and try again
        mReadBuffer.resize(NETWORK_BUF_SIZE);
        mWriteBuffer.resize(NETWORK_BUF_SIZE);
        poll();
        return;
      case ERROR_NOTIFY_ENUM_DIR:
        throw WatcherError("Buffer overflow. Some events may have been lost.", mWatcher);
      case ERROR_ACCESS_DENIED: {
        // This can happen if the watched directory is deleted. Check if that is the case,
        // and if so emit a delete event. Otherwise, fall through to default error case.
        DWORD attrs = GetFileAttributesW(extendedWidePath(mWatcher->mDir).data());
        Kind kind = attrs == INVALID_FILE_ATTRIBUTES ? IS_UNKNOWN : isDir(attrs) ? IS_DIR : IS_FILE;
        if (kind == IS_UNKNOWN) {
          mWatcher->mEvents.remove(mWatcher->mDir, IS_DIR, FAKE_INO);
          mTree->remove(mWatcher->mDir);
          mWatcher->notify();
          stop();
          return;
        }
      }
      default:
        if (errorCode != ERROR_SUCCESS) {
          throw WatcherError("Unknown error", mWatcher);
        }
    }

    // Swap read and write buffers, and poll again
    std::swap(mWriteBuffer, mReadBuffer);
    poll();

    // Read change events
    BYTE *base = mReadBuffer.data();
    while (true) {
      PFILE_NOTIFY_INFORMATION info = (PFILE_NOTIFY_INFORMATION)base;
      processEvent(info);

      if (info->NextEntryOffset == 0) {
        break;
      }

      base += info->NextEntryOffset;
    }

    mWatcher->notify();
  }

  void processEvent(PFILE_NOTIFY_INFORMATION info) {
    auto now = std::chrono::system_clock::now();

    std::string path = mWatcher->mDir + "\\" + utf16ToUtf8(info->FileName, info->FileNameLength / sizeof(WCHAR));
    if (mWatcher->isIgnored(path)) {
      return;
    }

    switch (info->Action) {
      case FILE_ACTION_ADDED:
      case FILE_ACTION_RENAMED_NEW_NAME: {
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (GetFileAttributesExW(extendedWidePath(path).data(), GetFileExInfoStandard, &data)) {
          Kind kind = isDir(data.dwFileAttributes) ? IS_DIR : IS_FILE;
          std::string fileId = getFileId(path);

          auto found = pendingMoves.find(fileId);
          if (found != pendingMoves.end()) {
            PendingMove pending = found->second;

            if (kind == IS_DIR) {
              std::string dirPath = pending.path + DIR_SEP;
              // Replace parent dir path in tree
              for (auto it = mTree->entries.begin(); it != mTree->entries.end();) {
                DirEntry entry = it->second;
                if (entry.path.rfind(dirPath.c_str(), 0) == 0) {
                  entry.path.replace(0, pending.path.length(), path);
                  mTree->entries.emplace(entry.path, entry);
                  it = mTree->entries.erase(it);
                } else {
                 it++;
                }
              }
            }

            mWatcher->mEvents.rename(pending.path, path, kind, FAKE_INO, fileId);
            pendingMoves.erase(found);
          } else {
            mWatcher->mEvents.create(path, kind, FAKE_INO, fileId);
          }
          mTree->add(path, FAKE_INO, CONVERT_TIME(data.ftLastWriteTime), kind, fileId);
        }
        break;
      }
      case FILE_ACTION_MODIFIED: {
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (GetFileAttributesExW(extendedWidePath(path).data(), GetFileExInfoStandard, &data)) {
          Kind kind = isDir(data.dwFileAttributes) ? IS_DIR : IS_FILE;
          std::string fileId = getFileId(path);
          mTree->update(path, FAKE_INO, CONVERT_TIME(data.ftLastWriteTime), fileId);
          if (kind != IS_DIR) {
            mWatcher->mEvents.update(path, FAKE_INO, fileId);
          }
        }
        break;
      }
      case FILE_ACTION_REMOVED:
      case FILE_ACTION_RENAMED_OLD_NAME:
        auto entry = mTree->find(path);
        if (entry) {
          pendingMoves.emplace(entry->fileId, PendingMove(now, path));
          mWatcher->mEvents.remove(path, entry->kind, entry->ino, entry->fileId);
        } else {
          mWatcher->mEvents.remove(path, IS_UNKNOWN, FAKE_INO);
        }
        mTree->remove(path);
        break;
    }
  }

private:
  WindowsBackend *mBackend;
  Watcher *mWatcher;
  std::shared_ptr<DirTree> mTree;
  std::unordered_multimap<std::string, PendingMove> pendingMoves;
  bool mRunning;
  HANDLE mDirectoryHandle;
  std::vector<BYTE> mReadBuffer;
  std::vector<BYTE> mWriteBuffer;
  OVERLAPPED mOverlapped;
};

// This function is called by Backend::watch which takes a lock on mMutex
void WindowsBackend::subscribe(Watcher &watcher) {
  // Create a subscription for this watcher
  Subscription *sub = new Subscription(this, &watcher, getTree(watcher, false, false));
  watcher.state = (void *)sub;

  // Queue polling for this subscription in the correct thread.
  bool success = QueueUserAPC([](__in ULONG_PTR ptr) {
    Subscription *sub = (Subscription *)ptr;
    sub->run();
  }, mThread.native_handle(), (ULONG_PTR)sub);

  if (!success) {
    throw std::runtime_error("Unable to queue APC");
  }
}

// This function is called by Backend::unwatch which takes a lock on mMutex
void WindowsBackend::unsubscribe(Watcher &watcher) {
  Subscription *sub = (Subscription *)watcher.state;
  delete sub;
}
