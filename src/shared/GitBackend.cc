#include <fstream>
#include <string>
#include "../DirTree.hh"
#include "../Event.hh"
#include "./GitBackend.hh"

#include <iostream>

std::string exec(const char *cmd) {
	std::array<char, 128> buffer;
	std::string result;
	std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
	if (!pipe) {
		throw std::runtime_error("popen() failed!");
	}
	while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
		result += buffer.data();
	}
	return result;
}

namespace {
std::string getCurrentCommit(std::string &dir) {
	std::string cmd = "cd " + dir + " && git rev-parse HEAD";
	std::string result = exec(cmd.c_str());
  // result.t
  return result;
}
} // namespace

void GitBackend::writeSnapshot(Watcher &watcher, std::string *snapshotPath) {
	std::cout << "commit: " << getCurrentCommit(watcher.mDir) << "\n";
	// std::unique_lock<std::mutex> lock(mMutex);
	// std::ofstream ofs(*snapshotPath);
	// tree->write(ofs);
}

void GitBackend::getEventsSince(Watcher &watcher, std::string *snapshotPath) {
	std::unique_lock<std::mutex> lock(mMutex);
	std::ifstream ifs(*snapshotPath);
	if (ifs.fail()) {
		return;
	}

	// watcher.mEvents
}
