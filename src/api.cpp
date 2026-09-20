#include "api.hpp"

#include "config.hpp"
#include "filewatcher.hpp"
#include "lua.hpp"
#include "utils.hpp"

#include <cerrno>
#include <mutex>
#include <sstream>


namespace SLSAPI
{

	std::fstream fstream;
	std::unique_ptr<CFileWatcher> watcher;
	bool initialized;

	std::mutex cmdMutex;

	std::vector<CompatOp_t> compatOps;
	std::vector<LibraryOp_t> libraryOps;
}

bool SLSAPI::isEnabled()
{
	return g_config.api.copy() && initialized;
}

void SLSAPI::onFileChange(const std::filesystem::path& path, __attribute__((unused)) const int eventMask)
{
	//Hot reload support :)
	if (!isEnabled())
	{
		return;
	}

	//Reopen stream, since it gets invalidated when our file gets replaced
	fstream.open(path, std::fstream::in);

	if (!fstream.is_open())
	{
		LOG_ERROR("Failed to open %s for parsing the next cmd!\n", path.c_str());
		return;
	}

	const auto content = std::string(std::istreambuf_iterator(fstream), {});

	fstream.close();

	const auto cmds = Utils::strsplit(content, "\n");
	for (const auto& cmd : cmds)
	{
		parseCmd(cmd);
	}
}

void SLSAPI::parseCmd(const std::string& cmd)
{
	LOG_DEBUG("API Running %s\n", cmd.c_str());

	const auto split = Utils::strsplit(cmd, "|");

	const std::lock_guard guard(cmdMutex);

	//Compatibility Manager
	if (split[0] == "dumpcompat" && split.size() > 1)
	{
		AppId_t appId;
		if (!Utils::tryConvertToNumber(split[1].c_str(), appId))
		{
			LOG_ERROR("Failed to dump compat for %s (not a number)!\n", split[1].c_str());
			return;
		}

		compatOps.emplace_back(CompatOp_t { CompatOp_t::OpType::Dump, appId, "" } );
	}

	else if (split[0] == "getcompat" && split.size() > 1)
	{
		AppId_t appId;
		if (!Utils::tryConvertToNumber(split[1].c_str(), appId))
		{
			LOG_ERROR("Failed to get compat for %s (not a number)!\n", split[1].c_str());
			return;
		}

		compatOps.emplace_back(CompatOp_t { CompatOp_t::OpType::Get, appId, "" } );
	}

	else if (split[0] == "setcompat" && split.size() > 1)
	{
		AppId_t appId;
		if (!Utils::tryConvertToNumber(split[1].c_str(), appId))
		{
			LOG_ERROR("Failed to set compat for %s (not a number)!\n", split[1].c_str());
			return;
		}

		compatOps.emplace_back(CompatOp_t { CompatOp_t::OpType::Set, appId, split.size() > 2 ? split[2].c_str() : "" } );
	}

	//Application Manager
	else if (split[0] == "dumplibraries")
	{
		libraryOps.emplace_back(LibraryOp_t { LibraryOp_t::OpType::Dump, 0, 0 } );
	}

	else if (split[0] == "install" && split.size() > 2)
	{
		AppId_t appId;
		uint32_t library;

		if (!Utils::tryConvertToNumber(split[1].c_str(), appId))
		{
			LOG_ERROR("Failed to install %s (not a number)!\n", split[1].c_str());
			return;
		}

		if (!Utils::tryConvertToNumber(split[2].c_str(), library))
		{
			LOG_ERROR("Failed to install %u to %s (not a number)!\n", appId, split[2].c_str());
			return;
		}

		libraryOps.emplace_back(LibraryOp_t { LibraryOp_t::OpType::Install, appId, library } );
	}

	else if (split[0] == "uninstall" && split.size() > 1)
	{
		AppId_t appId;
		if (!Utils::tryConvertToNumber(split[1].c_str(), appId))
		{
			LOG_ERROR("Failed to uninstall %s (not a number)!\n", split[1].c_str());
			return;
		}

		libraryOps.emplace_back(LibraryOp_t { LibraryOp_t::OpType::Uninstall, appId, 0 } );
	}

	// Lua
	if (split[0] == "reloadlua")
	{
		Lua::init();
		LOG_API("Ran reloadlua!\n");
	}
}

void SLSAPI::init()
{
	fstream = std::fstream(PATH, std::fstream::in | std::fstream::out | std::fstream::trunc); //Open for reading, writing and also delete contents

	if (!fstream.is_open())
	{
		LOG_NOTIFYWARN("Failed to create %s (%s)!\n API will be unavailable", PATH, strerror(errno));
		return;
	}

	//Close stream, onFileChange reopens then closes it for us
	fstream.close();

	watcher = std::make_unique<CFileWatcher>(onFileChange);
	const int fd = watcher->addFile(PATH);
	if (fd == -1)
	{
		LOG_NOTIFYWARN("Failed to watch %s!\n API will be unavailable", PATH);
		return;
	}

	initialized = true;

	watcher->start();
	LOG_DEBUG("SLSsteam API initialized!\n");
}

void SLSAPI::runCompatOps()
{
	if (!g_pClientCompat)
	{
		return;
	}

	while (compatOps.size())
	{
		const auto op = compatOps.begin();

		switch(op->type)
		{
			case CompatOp_t::OpType::Dump:
			{
				//We do not allocate anything, it'll just mess up
				//Luckily the function allocates for us
				CUtlVector<CUtlString> tools { };
				g_pClientCompat->getCompatToolsForApp(op->appId, &tools);

				std::ostringstream toolsSS;
				for (size_t i = 0; i < tools.size; i++)
				{
					const char* name = tools.at(i)->get();
					const char* displayName = g_pClientCompat->getDisplayName(name);

					if (toolsSS.str().size() > 0)
					{
						toolsSS << ", ";
					}

					toolsSS << "\"" << displayName << "\" (" << name << ")";
				}

				const auto str = toolsSS.str();
				LOG_API("Dump compatibility tools for %u: %s\n", op->appId, str.c_str());

				break;
			}

			case CompatOp_t::OpType::Get:
			{
				if (!g_pClientCompat->isCompatToolEnabled(op->appId))
				{
					LOG_API("Get compatibility tool for %u: Compatibility tool is disabled!\n", op->appId);
					break;
				}

				const char* name = g_pClientCompat->getCompatToolName(op->appId);
				const char* displayName = g_pClientCompat->getDisplayName(name);

				LOG_API("Get compatibility tool for %u: \"%s\" (%s)\n", op->appId, displayName, name);

				break;
			}

			case CompatOp_t::OpType::Set:
			{
				//Steam calls them with the same values
				g_pClientCompat->specifyCompatTool(op->appId, op->tool.c_str(), "", 250);
				LOG_API("Set compatibility tool for %u to %s\n", op->appId, op->tool.c_str());

				break;
			}
		}


		compatOps.erase(op);
	}
}

void SLSAPI::runInstallOps()
{
	const auto usr = g_pSteamEngine->getUser();
	if (!usr)
	{
		return;
	}

	const auto appManager = usr->getAppManager();

	while (libraryOps.size())
	{
		const auto op = libraryOps.begin();

		switch(op->type)
		{
			case LibraryOp_t::OpType::Dump:
			{
				const size_t num = appManager->getNumLibraryFolders();
				char pathBuf[0x1000]; //Same size steam passes
				char labelBuf[0x80]; //Same size steam passes
				
				std::ostringstream outSS;
				for (size_t i = 0; i < num; i++)
				{
					size_t pathLen = appManager->getLibraryFolderPath(i, pathBuf, sizeof(pathBuf));
					size_t labelLen = appManager->getLibraryFolderLabel(i, labelBuf, sizeof(labelBuf));

					if (labelLen)
					{
						LOG_API("Library \"%s\" at \"%s\" has index %u\n", std::string(labelBuf, labelLen).c_str(), std::string(pathBuf, pathLen).c_str(), i);
					}
					else
					{
						LOG_API("Library at \"%s\" has index %u\n", std::string(pathBuf, pathLen).c_str(), i);
					}
				}

				break;
			}

			case LibraryOp_t::OpType::Install:
			{
				appManager->installApp(op->appId, op->libraryIndex);
				LOG_API("Installed %u to %u\n", op->appId, op->libraryIndex);

				break;
			}

			case LibraryOp_t::OpType::Uninstall:
			{
				appManager->uninstallApp(op->appId);
				LOG_API("Uninstalled %u\n", op->appId);

				break;
			}
		}

		libraryOps.erase(op);
	}
}

void SLSAPI::runIPCFrame()
{
	const std::lock_guard guard(cmdMutex);

	runCompatOps();
	runInstallOps();
}
