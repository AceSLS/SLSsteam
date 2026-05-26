#pragma once

#include <cstdint>
#include <fstream>


class CFileWatcher;

namespace SLSAPI
{
	extern const char* path;
	extern std::fstream fstream;
	extern CFileWatcher* watcher;

	bool isEnabled();
	uint32_t currentInstallAppId();
	void onFileChange();
	void runPendingRequests();
	void init();
}
