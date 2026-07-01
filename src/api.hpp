#pragma once

#include <fstream>
#include <string>

class CFileWatcher;
namespace SLSAPI
{
	extern std::string path;
	extern std::fstream fstream;
	extern CFileWatcher* watcher;
	bool isEnabled();
	void onFileChange();
	void init();
	void deinit();
}