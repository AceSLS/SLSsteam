#include "config.hpp"
#include "globals.hpp"
#include "hooks.hpp"
#include "utils.hpp"

#include "libmem/libmem.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <link.h>
#include <pthread.h>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <unistd.h>

const char* EXPECTED_STEAMCLIENT_HASH = "075e8017db915e75b4e68cda3c363bad63afff0e033d4ace3fd3b27cc5e265d0";

bool cleanEnvVar(const char* varName)
{
	char* var = getenv(varName);
	if (var == NULL)
		return false;

	auto splits = Utils::strsplit(var, ":");
	auto newEnv = std::string();

	for(unsigned int i = 0; i < splits.size(); i++)
	{
		auto split = splits.at(i);
		if (split.ends_with("SLSsteam.so"))
		{
			Utils::log("Removed SLSsteam.so from $%s\n", varName);
			continue;
		}

		if(newEnv.size() > 0)
		{
			newEnv.append(":");
		}
		newEnv.append(split);
	}

	setenv(varName, newEnv.c_str(), 1);
	//Utils::log("Set %s to %s\n", varName, newEnv.c_str());

	return true;
}

bool verifySteamClientHash()
{
	LM_FindModule("steamclient.so", &g_modSteamClient);

	auto path = std::filesystem::path(g_modSteamClient.path);
	auto dir = path.parent_path();
	Utils::log("steamclient.so loaded from %s/%s at %p\n", dir.filename().c_str(), path.filename().c_str(), g_modSteamClient.base);

	try
	{
		std::string sha256 = Utils::getFileSHA256(path.c_str());
		Utils::log("steamclient.so hash is %s\n", sha256.c_str());

		//TODO: Research if there's a better way to compare const char* to std::string
		return strcmp(sha256.c_str(), EXPECTED_STEAMCLIENT_HASH) == 0;
	}
	catch(std::runtime_error& err)
	{
		Utils::log("Unable to read steamclient.so hash!\n");
		return false;
	}
}

static pthread_t SLSsteam_mainThread;

void* SLSsteam_init(void*)
{
	lm_process_t proc {};
	if (!LM_GetProcess(&proc))
	{
		exit(1);
	}

	//Do not do anything in other processes
	if (strcmp(proc.name, "steam") != 0)
	{
		return nullptr;
	}

	Utils::init();
	Utils::log("SLSsteam loading in %s\n", proc.name);

	cleanEnvVar("LD_AUDIT");
	cleanEnvVar("LD_PRELOAD");

	g_config.init();

	if (!verifySteamClientHash())
	{
		if (g_config.safeMode)
		{
			Utils::warn("Unknown steamclient.so hash! Aborting...");
			Utils::shutdown();
			return nullptr;
		}
		else if (g_config.warnHashMissmatch)
		{
			Utils::warn("steamclient.so hash missmatch! Please update :)");
		}
	}

	if (!Hooks::setup())
	{
		Utils::shutdown();
		return nullptr;
	}

	Utils::notify("SLSsteam loaded");
	return nullptr;
}

unsigned int la_version(unsigned int)
{
	return LAV_CURRENT;
}

unsigned int la_objopen(struct link_map *map, __attribute__((unused)) Lmid_t lmid, __attribute__((unused)) uintptr_t *cookie)
{
	if (std::string(map->l_name).ends_with("/steamclient.so"))
	{
		if(pthread_create(&SLSsteam_mainThread, nullptr, SLSsteam_init, nullptr))
		{
			Utils::warn("Failed to create SLSsteam thread!\nAborting");
		}
	}
	return 0;
}
