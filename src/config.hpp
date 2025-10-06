#pragma once

#include "log.hpp"

#include "yaml-cpp/exceptions.h"
#include "yaml-cpp/node/node.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>

class CConfig
{
  public:
	class CDlcData
	{
	  public:
		uint32_t parentId;
		std::unordered_map<uint32_t, std::string> dlcIds;
	};

	std::unordered_set<uint32_t> appIds;
	std::unordered_set<uint32_t> addedAppIds;
	std::unordered_map<uint32_t, CDlcData> dlcData;
	std::unordered_map<uint32_t, std::unordered_set<uint32_t>> denuvoGames;
	bool denuvoSpoof;

	bool disableFamilyLock;
	bool useWhiteList;
	bool automaticFilter;
	bool playNotOwnedGames;
	bool safeMode;
	bool notifications;
	bool warnHashMissmatch;
	bool notifyInit;
	bool extendedLogging;

	std::string getDir();
	std::string getPath();
	bool createFile();
	bool init();
	bool loadSettings();

	bool isAddedAppId(uint32_t appId);
	bool addAdditionalAppId(uint32_t appId);

	bool shouldExcludeAppId(uint32_t appId);
	uint32_t getDenuvoGameOwner(uint32_t appId);

  private:
	// Gets a setting from the YAML node.
	template <typename T>
	T getSetting(const YAML::Node &node, const char *name, T defVal)
	{
		if (!node[name])
		{
			g_pLog->notifyLong("Missing %s in configfile! Using default", name);
			return defVal;
		}

		try
		{
			return node[name].as<T>();
		}
		catch (const YAML::BadConversion &er)
		{
			g_pLog->notify(
				"Failed to parse value of %s! Using default. Error: %s\n", name,
				er.what());
			return defVal;
		}
	};

	// Gets a setting and logs its value.
	template <typename T>
	T getAndLogSetting(const YAML::Node &node, const char *name, T defVal);

	// Parses lists and maps from the YAML file.
	void parseAppIdList(const YAML::Node &parentNode, const char *key,
						std::unordered_set<uint32_t> &targetSet);
	void parseDlcData(const YAML::Node &parentNode);
	void parseDenuvoGames(const YAML::Node &parentNode);
};

extern CConfig g_config;