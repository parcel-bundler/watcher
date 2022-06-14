#include <sstream>
#include "../const.hh"
#include "./win_utils.hh"

std::wstring utf8ToUtf16(std::string input) {
  unsigned int len = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, NULL, 0);
  WCHAR *output = new WCHAR[len];
  MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, output, len);
  std::wstring res(output);
  delete output;
  return res;
}

std::string utf16ToUtf8(const WCHAR *input, size_t length) {
  unsigned int len = WideCharToMultiByte(CP_UTF8, 0, input, length, NULL, 0, NULL, NULL);
  char *output = new char[len + 1];
  WideCharToMultiByte(CP_UTF8, 0, input, length, output, len, NULL, NULL);
  output[len] = '\0';
  std::string res(output);
  delete output;
  return res;
}

std::wstring extendedWidePath(std::string path) {
  // Prevent truncation to MAX_PATH characters by adding the \\?\ prefix
  return utf8ToUtf16("\\\\?\\" + path);
}

std::string normalizePath(std::string path) {
  std::wstring p = extendedWidePath(path);

  // Get the required length for the output
  unsigned int len = GetLongPathNameW(p.data(), NULL, 0);
  if (!len) {
    return path;
  }

  // Allocate output array and get long path
  WCHAR *output = new WCHAR[len];
  len = GetLongPathNameW(p.data(), output, len);
  if (!len) {
    delete output;
    return path;
  }

  // Convert back to utf8
  std::string res = utf16ToUtf8(output + 4, len - 4);
  delete output;
  return res;
}

std::string getFileId(std::string path) {
  HANDLE hFind = INVALID_HANDLE_VALUE;
  DWORD flagsAndAttributes =
    FILE_ATTRIBUTE_NORMAL | // no specific file attributes
    FILE_FLAG_OPEN_REPARSE_POINT | // open symbolic links instead of their targets
    FILE_FLAG_OVERLAPPED | // allow simultaneous asynchronous I/O operations
    FILE_FLAG_BACKUP_SEMANTICS; // allow opening directories

  hFind = CreateFileW(
    extendedWidePath(path).data(), // path of of file to open
    0, // only allow querying metadata
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, // allow other processes to read or write to the file but not delete or rename it
    NULL, // no specific security attributes
    OPEN_EXISTING, // open file only if it already exists
    flagsAndAttributes, // attributes and flags of the opened file
    NULL // no template since reading only
  );
  if (hFind == INVALID_HANDLE_VALUE) {
    return FAKE_FILEID;
  }

  BY_HANDLE_FILE_INFORMATION fileInfo;
  bool success = GetFileInformationByHandle(hFind, &fileInfo);
  if (!success) {
    CloseHandle(hFind);
    return FAKE_FILEID;
  }
  // No need to keep the handle active anymore
  CloseHandle(hFind);

  // Format fileId as sprintf would with the "0x%08X%08X" format, with the high part coming first
  // We use a stringstream to build each hexadecimal character
  std::stringstream fileIdHigh;
  std::stringstream fileIdLow;

  fileIdHigh.flags(std::ios::right | std::ios::hex | std::ios::uppercase);
  fileIdHigh.width(8);
  fileIdHigh.fill('0');
  fileIdLow.copyfmt(fileIdHigh);

  fileIdHigh << fileInfo.nFileIndexHigh;
  fileIdLow << fileInfo.nFileIndexLow;

  return "0x" + fileIdHigh.str() + fileIdLow.str();
}
