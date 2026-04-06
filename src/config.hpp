#pragma once

#include "sdk/steam.hpp"

#include "mtvar.hpp"
#include "log.hpp"

#include "yaml-cpp/exceptions.h"
#include "yaml-cpp/node/node.h"
#include "yaml-cpp/yaml.h"

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <pthread.h>
#include <string>
#include <unordered_map>
#include <unordered_set>


class CFileWatcher;

class CConfig {
public:
	struct FakeGame_t
	{
		AppId_t appId = 0;
		std::string title;
	};

	struct InventoryItems_t
	{
	public:
		uint32_t appId;
		std::unordered_map<uint32_t, uint32_t> items;
	};

	class CDlcData
	{
	public:
		AppId_t parentId;
		std::unordered_map<AppId_t, std::string> dlcIds;
		//No default constructor, otherwise dlcData will complain that no matching one was found
		//without implementing it ourself anyway
	};

	enum class ELoadError : uint32_t
	{
		None,
		MissingKey,
		ParsingException
	};
	MTVariable<ELoadError> __loadErrors;

	MTVariable<std::unordered_set<AppId_t>> appIds;
	MTVariable<std::unordered_set<AppId_t>> addedAppIds;
	MTVariable<std::unordered_set<AppId_t>> addedDepotIds;
	MTVariable<std::unordered_set<uint32_t>> addedPackageIds;
	MTVariable<std::unordered_map<AppId_t, CDlcData>> dlcData;
	MTVariable<std::unordered_map<AppId_t, uint64_t>> appTokens;
	MTVariable<std::unordered_set<AppId_t>> fakeOffline;
	MTVariable<std::unordered_map<AppId_t, AppId_t>> fakeAppIds;
	MTVariable<std::unordered_map<AppId_t, uint64_t>> manifestIds;
	MTVariable<std::unordered_set<AppId_t>> depotBlacklist;
	MTVariable<std::unordered_map<AppId_t, InventoryItems_t>> inventoryItems;
	MTVariable<FakeGame_t> idleStatus;
	MTVariable<std::unordered_map<AppId_t, std::string>> gameTitles;
	MTVariable<std::unordered_map<AppId_t, uint32_t>> subscriptionTimestamps;

	MTVariable<std::unordered_map<uint32_t, std::unordered_set<AppId_t>>> denuvoGames;

	MTVariable<bool> disableFamilyLock;
	MTVariable<bool> useWhiteList;
	MTVariable<uint32_t> maxSchemaTries;
	MTVariable<bool> safeMode;
	MTVariable<bool> notifications;
	MTVariable<bool> warnHashMissmatch;
	MTVariable<bool> notifyInit;
	MTVariable<bool> api;
	MTVariable<bool> disableCloud;
	MTVariable<bool> disableUpdates;
	MTVariable<std::string> fakeEmail;
	MTVariable<int32_t> fakeWalletBalance;
	MTVariable<unsigned int> logLevel;
	MTVariable<bool> dumpInterfaceMaps;
	MTVariable<bool> extendedLogging;

	std::mutex appsChangedMutex;
	std::unordered_set<AppId_t> newApps;
	std::unordered_set<AppId_t> removedApps;

	//Using incomplete class to avoid runtime linking errors
	CFileWatcher* watcher;

	~CConfig();

	std::string getDir() const;
	std::string getPath() const;
	bool createFile() const;
	bool init();

	void setError(const ELoadError err);
	bool loadSettings(const bool firstLoad = false);

	template<typename T>
	T getSetting(YAML::Node& node, const char* name, const T defVal)
	{
		if (!node[name])
		{
			//g_pLog->notifyLong("Missing %s in configfile! Using default", name);
			setError(ELoadError::MissingKey);
			return defVal;
		}

		try
		{
			 return node[name].as<T>();
		}
		catch (YAML::BadConversion& er)
		{
			//g_pLog->notify("Failed to parse value of %s! Using default\n", name);
			setError(ELoadError::ParsingException);
			return defVal;
		}
	}

	template<typename T>
	std::unordered_set<T> getList(YAML::Node& rootNode, const char* name)
	{
		auto list = std::unordered_set<T>();

		const auto node = rootNode[name];
		if (!node)
		{
			//g_pLog->notifyLong("Missing %s in configfile! Using default", name);
			setError(ELoadError::MissingKey);
			return list;
		}

		for(auto subNode : node)
		{
			try
			{
				const T val = subNode.as<T>();
				list.emplace(val);

				//TODO: Find better way to log shit
				if (std::is_same_v<T, uint32_t>)
				{
					g_pLog->info("Added %u to %s\n", val, name);
				}
			}
			catch(...)
			{
				//g_pLog->notify("Failed to parse %s!", name);
				setError(ELoadError::ParsingException);
			}
		}

		return list;
	}

	template<typename T, typename T2>
	std::unordered_map<T, T2> getMap(YAML::Node& rootNode, const char* name)
	{
		auto map = std::unordered_map<T, T2>();

		const auto node = rootNode[name];
		if (!node)
		{
			//g_pLog->notifyLong("Missing %s in configfile! Using default", name);
			setError(ELoadError::MissingKey);
			return map;
		}

		for(auto& subNode : node)
		{
			try
			{
				//TODO: Add error checks for failed parsing since yaml-cpp does not throw
				const auto k = subNode.first.as<T>();
				const auto v = subNode.second.as<T2>();

				map[k] = v;

				if (std::is_same_v<T, uint32_t> && std::is_same_v<T, T2>)
				{
					g_pLog->info("Added %u to %u in %s\n", k, v, name);
				}
				else if (std::is_same_v<T, uint32_t> && std::is_same_v<T2, uint64_t>)
				{
					g_pLog->info("Added %u to %llu in %s\n", k, v, name);
				}
			}
			catch(...)
			{
				//g_pLog->notify("Failed to parse %s!", name);
				setError(ELoadError::ParsingException);
			}
		}

		return map;
	}

	bool isAddedAppId(const AppId_t appId);
	bool addAdditionalAppId(const AppId_t appId);

	bool shouldExcludeAppId(const AppId_t appId, const bool ignoreAdditionalApps = false);
	uint32_t getDenuvoGameOwner(const AppId_t appId);
};

extern CConfig g_config;
