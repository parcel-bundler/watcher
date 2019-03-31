#ifndef IPC_H
#define IPC_H

#include <string>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif

class IPC {
public:
  IPC(std::string path) {
    #ifdef _WIN32
      mPipe = CreateFile( 
        path.data(), // pipe name 
        GENERIC_READ | GENERIC_WRITE, // read and write access 
        0, // no sharing 
        NULL, // default security attributes
        OPEN_EXISTING, // opens existing pipe 
        FILE_FLAG_OVERLAPPED, // attributes 
        NULL // no template file
      );

      if (mPipe == INVALID_HANDLE_VALUE) {
        printf("Could not open pipe\n");
        throw "Could not open pipe";
      }

      mReader = CreateEvent(NULL, true, false, NULL);
      mWriter = CreateEvent(NULL, true, false, NULL);
    #else
      struct sockaddr_un addr;
      memset(&addr, 0, sizeof(addr));
      addr.sun_family = AF_UNIX;
      strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

      mSock = socket(AF_UNIX, SOCK_STREAM, 0);
      if (connect(mSock, (struct sockaddr *) &addr, sizeof(struct sockaddr_un))) {
        throw "Error connecting to socket";
      }
    #endif
  }

  ~IPC() {
    #ifdef _WIN32
      CancelIo(mPipe);
      CloseHandle(mPipe);
      CloseHandle(mReader);
      CloseHandle(mWriter);
    #else
      shutdown(mSock, SHUT_RDWR);
    #endif
  }

  void write(std::string buf) {
    #ifdef _WIN32
      OVERLAPPED overlapped;
      overlapped.hEvent = mWriter;
      bool success = WriteFile(
        mPipe, // pipe handle 
        buf.data(), // message 
        buf.size(), // message length 
        NULL, // bytes written 
        &overlapped // overlapped 
      );

      if (!success) {
        if (GetLastError() != ERROR_IO_PENDING) {
          throw "Write error";
        }
      }

      DWORD written;
      success = GetOverlappedResult(mPipe, &overlapped, &written, true);
      if (!success) {
        printf("GetOverlappedResult failed\n");
        throw "GetOverlappedResult failed";
      }

      if (written != buf.size()) {
        printf("Wrong number of bytes written\n");
        throw "Wrong number of bytes written";
      }
    #else
      int r = 0;
      for (unsigned int i = 0; i != buf.size(); i += r) {
        r = ::write(mSock, &buf[i], buf.size() - i);
        if (r == -1) {
          if (errno == EAGAIN) {
            r = 0;
          } else {
            throw "Write error";
          }
        }
      }
    #endif
  }

  int read(char *buf, size_t len) {
    #ifdef _WIN32
      OVERLAPPED overlapped;
      overlapped.hEvent = mReader;
      bool success = ReadFile(
        mPipe, // pipe handle 
        buf, // buffer to receive reply 
        len, // size of buffer 
        NULL, // number of bytes read 
        &overlapped // overlapped 
      );

      if (!success) {
        if (GetLastError() != ERROR_IO_PENDING) {
          printf("Read error\n");
          throw "Read error";
        }
      }

      DWORD read;
      success = GetOverlappedResult(mPipe, &overlapped, &read, true);
      if (!success) {
        printf("GetOverlappedResult failed\n");
        throw "GetOverlappedResult failed";
      }
      
      return read;
    #else
      int r = ::read(mSock, buf, len);
      if (r < 0) {
        throw strerror(errno);
      }

      return r;
    #endif
  }

private:
  #ifdef _WIN32
    HANDLE mPipe;
    HANDLE mReader;
    HANDLE mWriter;
  #else
    int mSock;
  #endif
};

#endif
