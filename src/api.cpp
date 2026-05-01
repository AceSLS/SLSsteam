#include "api.hpp"

#include "config.hpp"
#include "filewatcher.hpp"
#include "sdk/IClientAppManager.hpp"
#include "utils.hpp"

#include <cstdlib>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

namespace SLSAPI
{
	std::string path;
	std::fstream fstream;
	CFileWatcher* watcher;
}

bool SLSAPI::isEnabled()
{
	return g_config.api.get() && fstream.is_open();
}

void SLSAPI::onFileChange()
{
	if (!isEnabled())
	{
		return;
	}

	struct stat st;
	if (stat(path.c_str(), &st) != 0 || st.st_uid != geteuid())
	{
		g_pLog->debug("API rejected file change: ownership mismatch or file missing.\n");
		return;
	}

	fstream.close();
	fstream.open(path, std::ios::in | std::ios::out);
	
	char cmd[128];
	fstream.getline(cmd, sizeof(cmd));
	g_pLog->debug("API Running %s\n", cmd);
	
	auto split = Utils::strsplit(cmd, "|");
	if (strcmp(split[0].c_str(), "install") == 0 && split.size() > 2)
	{
		try
		{
			uint32_t appId = std::strtoul(split[1].c_str(), nullptr, 10);
			uint32_t library = std::strtoul(split[2].c_str(), nullptr, 10);
			if (!g_pClientAppManager)
			{
				g_pLog->info("API g_pClientAppManager is nullptr! Aborting...\n");
				return;
			}
			g_pLog->info("API Installing %s to %s\n", split[1].c_str(), split[2].c_str());
			g_pClientAppManager->installApp(appId, library);
		}
		catch(...)
		{
			g_pLog->info("API Failed to parse %s or %s!\n", split[1].c_str(), split[2].c_str());
		}
	}
}

void SLSAPI::init()
{
	const char* xdg = std::getenv("XDG_RUNTIME_DIR");
	if (xdg && xdg[0] != '\0')
	{
		path = std::string(xdg) + "/SLSsteam.API";
	}
	else
	{
		const char* home = std::getenv("HOME");
		if (!home)
		{
			g_pLog->info("API init failed: XDG_RUNTIME_DIR and HOME not set.\n");
			return;
		}
		
		std::string dir = std::string(home) + "/.config/SLSsteam";
		struct stat st;
		if (lstat(dir.c_str(), &st) == 0)
		{
			if (S_ISLNK(st.st_mode))
			{
				g_pLog->info("API init failed: fallback directory is a symlink.\n");
				return;
			}
			if (st.st_uid != geteuid())
			{
				g_pLog->info("API init failed: fallback directory not owned by current user.\n");
				return;
			}
		}
		else
		{
			if (mkdir(dir.c_str(), 0700) != 0)
			{
				g_pLog->info("API init failed: could not create fallback directory.\n");
				return;
			}
		}
		path = dir + "/SLSsteam.API";
	}

	fstream.open(path, std::ios::in | std::ios::out | std::ios::trunc);
	chmod(path.c_str(), 0600);
	
	watcher = new CFileWatcher(onFileChange);
	watcher->addFile(path.c_str());
	watcher->start();
	
	g_pLog->debug("SLSsteam API initialized at %s\n", path.c_str());
}

void SLSAPI::deinit()
{
	delete watcher;
	watcher = nullptr;
	
	if (fstream.is_open())
	{
		fstream.close();
	}
	
	const char* xdg = std::getenv("XDG_RUNTIME_DIR");
	if (xdg && path.rfind(xdg, 0) == 0)
	{
		unlink(path.c_str());
	}
	
	path.clear();
}