#pragma once

#include "sdk/sdk.hpp"

#include "mtvar.hpp"
#include "log.hpp"

#include "yaml-cpp/yaml.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <pthread.h>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>


class CFileWatcher;

class CConfig {
public:
	//Using incomplete class to avoid runtime linking errors
	static std::unique_ptr<CFileWatcher> watcher;

	static std::filesystem::path getDir();
	static std::filesystem::path getPath();
	static void onFileChange(__attribute__((unused)) const std::filesystem::path& path, __attribute__((unused)) const int eventMask);

	typedef unsigned int SmartTicketsFlags_t;

	enum ESmartTickets : SmartTicketsFlags_t
	{
		k_ESmartTicketsSteamDRM = 1 << 0,
		k_ESmartTicketsDenuvo = 1 << 1,
	};

	struct FakeGame_t
	{
		AppId_t appId = 0;
		std::string title;
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
	MTVariable<std::string> __loadErrors;

	MTVariable<std::unordered_set<AppId_t>> appIds;
	MTVariable<std::unordered_set<AppId_t>> addedAppIds;
	MTVariable<std::unordered_map<AppId_t, CDlcData>> dlcData;
	MTVariable<std::unordered_map<AppId_t, uint64_t>> appTokens;
	MTVariable<std::unordered_map<AppId_t, std::string>> cdKeys;
	MTVariable<std::unordered_set<AppId_t>> fakeOffline;
	MTVariable<std::unordered_map<AppId_t, AppId_t>> fakeAppIds;
	MTVariable<std::unordered_map<AppId_t, uint64_t>> manifestIds;
	MTVariable<std::unordered_set<AppId_t>> depotBlacklist;
	MTVariable<FakeGame_t> idleStatus;
	MTVariable<std::unordered_map<AppId_t, std::string>> gameTitles;
	MTVariable<std::unordered_map<AppId_t, uint32_t>> subscriptionTimestamps;

	MTVariable<std::unordered_map<uint64_t, std::unordered_set<AppId_t>>> denuvoGames;
	MTVariable<std::unordered_map<AppId_t, uint64_t>> steamIdOverride;

	MTVariable<std::unordered_map<AppId_t, std::string>> launchOptions;

	MTVariable<bool> disableFamilyLock;
	MTVariable<bool> useWhiteList;
	MTVariable<uint32_t> maxSchemaTries;
	MTVariable<SmartTicketsFlags_t> smartTickets;
	MTVariable<bool> safeMode;
	MTVariable<bool> warnHashMissmatch;
	MTVariable<bool> notifyInit;
	MTVariable<bool> api;
	MTVariable<bool> plugins;
	MTVariable<bool> disableCloud;
	MTVariable<bool> disableUpdates;
	MTVariable<std::string> fakeName;
	MTVariable<std::string> fakeEmail;
	MTVariable<int32_t> fakeWalletBalance;
	MTVariable<LogLevelFlags_t> logLevels = 0xff;
	MTVariable<bool> dumpInterfaceMaps;
	MTVariable<bool> extendedLogging;

	MTVariable<std::unordered_set<AppId_t>> newApps;
	MTVariable<std::unordered_set<AppId_t>> removedApps;

	YAML::Node rootNode;

	bool createFile() const;
	bool init();

	void setError(const ELoadError err, const char* keyName);
	bool loadSettings(const bool firstLoad = false, const bool silent = false);

	template<typename T>
	T getSetting(const YAML::Node& node, const char* name, const T defVal, const bool silent = false)
	{
		if (!node[name])
		{
			//LOG_NOTIFYLONG("Missing %s in configfile! Using default", name);
			setError(ELoadError::MissingKey, name);
			return defVal;
		}

		try
		{
			const T val = node[name].as<T>();

			if (silent)
			{
				return val;
			}

			if constexpr (std::is_same_v<T, std::string>)
			{
				LOG_CUSTOM(k_ELogLevelInfo | k_ELogLevelOnce, "%s is \"%s\"\n", name, val.c_str());
			}
			else
			{
				LOG_CUSTOM(k_ELogLevelInfo | k_ELogLevelOnce, "%s is %s\n", name, std::to_string(val).c_str());
			}

			return val;
		}
		catch (...)
		{
			//LOG_NOTIFY("Failed to parse value of %s! Using default\n", name);
			setError(ELoadError::ParsingException, name);
			return defVal;
		}
	}

	template<typename T>
	std::unordered_set<T> getList(const YAML::Node& rootNode, const char* name, const bool silent = false)
	{
		auto list = std::unordered_set<T>();

		const auto node = rootNode[name];
		if (!node)
		{
			//LOG_NOTIFYLONG("Missing %s in configfile! Using default", name);
			setError(ELoadError::MissingKey, name);
			return list;
		}

		for (auto subNode : node)
		{
			try
			{
				const T val = subNode.as<T>();
				list.emplace(val);

				if (silent)
				{
					continue;
				}

				if constexpr (std::is_same_v<T, std::string>)
				{
					LOG_CUSTOM(k_ELogLevelInfo | k_ELogLevelOnce, "Adding \"%s\" to %s\n", val.c_str(), name);
				}
				else
				{
					LOG_CUSTOM(k_ELogLevelInfo | k_ELogLevelOnce, "Adding %s to %s\n", std::to_string(val).c_str(), name);
				}

			}
			catch(...)
			{
				//LOG_NOTIFY("Failed to parse %s!", name);
				setError(ELoadError::ParsingException, name);
			}
		}

		return list;
	}

	template<typename T, typename T2>
	std::unordered_map<T, T2> getMap(const YAML::Node& rootNode, const char* name, const bool silent = false)
	{
		auto map = std::unordered_map<T, T2>();

		const auto node = rootNode[name];
		if (!node)
		{
			//LOG_NOTIFYLONG("Missing %s in configfile! Using default", name);
			setError(ELoadError::MissingKey, name);
			return map;
		}

		for (auto& subNode : node)
		{
			try
			{
				//TODO: Add error checks for failed parsing since yaml-cpp does not throw
				const auto k = subNode.first.as<T>();
				const auto v = subNode.second.as<T2>();

				map[k] = v;

				if (silent)
				{
					continue;
				}

				if constexpr (std::is_same_v<T2, std::string>)
				{
					LOG_CUSTOM(k_ELogLevelInfo | k_ELogLevelOnce, "Adding %s: \"%s\" to %s\n", std::to_string(k).c_str(), v.c_str(), name);
				}
				else
				{
					LOG_CUSTOM(k_ELogLevelInfo | k_ELogLevelOnce, "Adding %s: %s to %s\n", std::to_string(k).c_str(), std::to_string(v).c_str(), name);
				}
			}
			catch(...)
			{
				//LOG_NOTIFY("Failed to parse %s!", name);
				setError(ELoadError::ParsingException, name);
			}
		}

		return map;
	}

	bool isAddedAppId(const AppId_t appId);
	bool addAdditionalAppId(const AppId_t appId);
	void setAdditionalApps(const std::unordered_set<AppId_t>& apps, const bool firstLoad = false);

	bool shouldExcludeAppId(const AppId_t appId, const bool ignoreAdditionalApps = false);
	CSteamId getDenuvoGameOwner(const AppId_t appId);
};

extern CConfig g_config;
