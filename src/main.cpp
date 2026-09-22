#include "sdk/sdk.hpp"

#include "api.hpp"
#include "config.hpp"
#include "decompiler.hpp"
#include "filewatcher.hpp"
#include "globals.hpp"
#include "hooks.hpp"
#include "lua.hpp"
#include "log.hpp"
#include "memhlp.hpp"
#include "patterns.hpp"
#include "update.hpp"
#include "utils.hpp"
#include "version.hpp"
#include "vftableinfo.hpp"

#include "libmem/libmem.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <link.h>
#include <memory>
#include <string>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>


static bool cleanEnvVar(const char* varName, const char* endsWith)
{
	char* var = getenv(varName);
	if (var == NULL)
		return false;

	const auto splits = Utils::strsplit(var, ":");
	auto newEnv = std::string();

	for (unsigned int i = 0; i < splits.size(); i++)
	{
		const auto split = splits.at(i);
		if (split.ends_with(endsWith))
		{
			LOG_DEBUG("Removed %s from $%s\n", endsWith, varName);
			continue;
		}

		if (newEnv.size() > 0)
		{
			newEnv.append(":");
		}
		newEnv.append(split);
	}

	if (newEnv.size())
	{
		setenv(varName, newEnv.c_str(), true);
	}
	else
	{
		unsetenv(varName);
	}
	//LOG_DEBUG("Set %s to %s\n", varName, newEnv.c_str());

	return true;
}

//Looking at /proc/self/maps it seems like this isn't needed for processes that aren't steam
//__attribute__((noreturn))
static void unload()
{
	//Hooks::remove();

	//This is absolutely unnessecary for applications loading SLSsteam where it cancels from setup()
	//Would be nice to run have for failed load() attempts though 
	//lm_module_t mod;
	//if (LM_FindModule("SLSsteam.so", &mod))
	//{
	//	//TODO: Investigate crash ?
	//	//Possibly: Might be because we're unmapping what ever thread we're running in
	//	//munmap(reinterpret_cast<void*>(mod.base), mod.size);
	//}
	//exit(0);
}

//TODO: Remove when unload() works properly since it should not be needed anymore after that
static bool setupSuccess = false;

static void setup()
{
	lm_process_t proc {};
	if (!LM_GetProcess(&proc))
	{
		unload();
		return;
	}

	//Do not do anything in other processes
	if (strcmp(proc.name, "steam") != 0)
	{
		unload();
		return;
	}

	g_pLog = std::unique_ptr<CLog>(CLog::createDefaultLog());
	//Won't happen, log throws a runtime exception when creation fails.
	//But just in case I decide to refactor one day
	if (!g_pLog)
	{
		unload();
		return;
	}

	LOG_INFO("SLSsteam (%s -> %s) loading in %s\n", BUILD_BRANCH, LAST_COMMIT_HASH, proc.name);

	CFileWatcher::installSigHandler();

	//Any release
	cleanEnvVar("LD_AUDIT", "SLSsteam.so");
	cleanEnvVar("LD_AUDIT", "library-inject.so");

	//Arch release
	cleanEnvVar("LD_AUDIT", "libSLSsteam.so");
	cleanEnvVar("LD_AUDIT", "libSLS-library-inject.so");
	//TODO: Investigate weird logging. Not like it's necessary anymore
	//cleanEnvVar("LD_PRELOAD");

	if (!g_config.init())
	{
		unload();
		return;
	}

	//Since we can't statically link everything and some distros seem to respect LD_LIBRARY_PATH
	//more or less than mine does we just force append those
	//Hopefully this won't mess anything else up
	auto ldLibPath = std::string(getenv("LD_LIBRARY_PATH"));
	ldLibPath.append("/usr/lib:/usr/lib32");
	setenv("LD_LIBRARY_PATH", ldLibPath.c_str(), true);

	Updater::init();

	setupSuccess = true;
}

static void load()
{
	if (!setupSuccess)
	{
		return;
	}

	if (!g_modSteamClient || !g_modSteamUI || !g_modTier0)
	{
		return;
	}

	const auto path = std::filesystem::path(g_modSteamClient->path);
	const auto dir = path.parent_path();

	LOG_INFO
	(
		"steamclient.so loaded from %s/%s at 0x%x to 0x%x\n",
		dir.filename().c_str(),
		path.filename().c_str(),
		g_modSteamClient->base,
		g_modSteamClient->end
	);
	LOG_INFO
	(
		"steamui.so loaded at 0x%x to 0x%x\n",
		g_modSteamUI->base,
		g_modSteamUI->end
	);

	if (!Updater::verifySafeModeHash())
	{
		if (g_config.safeMode.copy())
		{
			LOG_NOTIFYERROR("Unknown steamclient.so hash! Aborting...");
			unload();
			return;
		}
		else if (g_config.warnHashMissmatch.copy())
		{
			LOG_NOTIFYWARN("steamclient.so hash missmatch! Please update :)");
		}
	}

	if (!Steam::init())
	{
		LOG_NOTIFYERROR("Failed to find steam exports!\n");
		return;
	}

	if (!VFTIndexes::init())
	{
		LOG_NOTIFYERROR("Failed to parse VFTables! Aborting...");
		return;
	}

	if (!Patterns::init())
	{
		LOG_NOTIFYERROR("Failed to find all patterns! Aborting...");
		return;
	}

	if (!Hooks::init())
	{
		LOG_NOTIFYERROR("Failed to setup & place hooks! Aborting...");
		return;
	}

	Lua::init(true);
	SLSAPI::init();
	//Disabled, since the Lua API can use it to find VFTables
	//Decompiler::cleanUp();

	if (g_config.notifyInit.copy())
	{
		const auto now = std::chrono::time_point { std::chrono::system_clock::now() };
		const auto ymd = std::chrono::year_month_day { std::chrono::floor<std::chrono::days>(now) };

		//Funsy easter egg :)
		if (static_cast<unsigned int>(ymd.month()) == 2 && static_cast<unsigned int>(ymd.day()) == 22)
		{
			LOG_NOTIFY("Happy birthday SLSsteam!");
		}
		else
		{
			LOG_NOTIFY("Loaded successfully");
		}
	}
}

unsigned int la_version(unsigned int)
{
	return LAV_CURRENT;
}

unsigned int la_objopen(struct link_map *map, __attribute__((unused)) Lmid_t lmid, __attribute__((unused)) uintptr_t *cookie)
{
	const std::string name = map->l_name;

	if (name.ends_with("/steamclient.so"))
	{
		//Analyse modules before any relocations get applied
		g_modSteamClient = MemHlp::getModule("steamclient.so");
		Decompiler::parseModule(*g_modSteamClient);
		//This is wasteful, but we have to analyse right away otherwise the offset get turned into
		//addresses messing up the analysis.
		//We could workaround it by only loading after a late module has been loaded
		for (auto& vft : Decompiler::vftables)
		{
			vft.second.analyze();
		}

		load();
	}
	if (name.ends_with("/steamui.so"))
	{
		//Analyse modules before any relocations get applied
		g_modSteamUI = MemHlp::getModule("steamui.so");
		Decompiler::parseModule(*g_modSteamUI);
		//This is wasteful, but we have to analyse right away otherwise the offset get turned into
		//addresses messing up the analysis.
		//We could workaround it by only loading after a late module has been loaded
		for (auto& vft : Decompiler::vftables)
		{
			vft.second.analyze();
		}

		load();
	}
	if (name.ends_with("/libtier0_s.so"))
	{
		g_modTier0 = MemHlp::getModule("libtier0_s.so");

		load();
	}

	return 0;
}

void la_preinit(__attribute__((unused)) uintptr_t *cookie)
{
	setup();
}
