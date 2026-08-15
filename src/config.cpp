#include "config.hpp"
#include "config_default.hpp"

#include "feats/apps.hpp"
#include "filewatcher.hpp"
#include "log.hpp"
#include "utils.hpp"

#include "yaml-cpp/yaml.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>


std::string CConfig::getDir() const
{
	std::ostringstream path;

	const char* configDir = getenv("XDG_CONFIG_HOME"); //Most users should have this set iirc
	if (configDir)
	{
		path << configDir;
	}
	else
	{
		LOG_CUSTOM(k_ELogLevelWarn | k_ELogLevelOnce, "XDG_CONFIG_HOME not set! Falling back to HOME\n");

		const char* home = getenv("HOME");
		path << home << "/.config";
	}

	path << "/SLSsteam";

	return path.str();
}

std::string CConfig::getPath() const
{
	return getDir() + "/config.yaml";
}

bool CConfig::createFile() const
{
	const std::string path = getPath();
	if (!std::filesystem::exists(path))
	{
		const std::string dir = getDir();
		if (!std::filesystem::exists(dir))
		{
			if (!std::filesystem::create_directory(dir))
			{
				LOG_NOTIFY("Unable to create config directory at %s!\n", dir.c_str());
				return false;
			}

			LOG_DEBUG("Created config directory at %s\n", dir.c_str());
		}

		auto config = std::ofstream(path);
		if (!config.is_open())
		{
			LOG_NOTIFY("Unable to create %s!", path.c_str());
			return false;
		}

		config << defaultConfig;
		config.close();
	}

	return true;
}

static void onFileChange()
{
	g_config.loadSettings();
	LOG_NOTIFY("Config reloaded!");
}

bool CConfig::init()
{
	if (!createFile())
	{
		LOG_WARN("Config creation failed!\n");
		return false;
	}

	watcher = new CFileWatcher(onFileChange);
	watcher->addFile(getPath().c_str());
	watcher->start();

	loadSettings(true);
	return true;
}

CConfig::~CConfig()
{
	if (watcher)
	{
		delete watcher;
	}
}

void CConfig::setError(const ELoadError err, const char* keyName)
{
	const auto prev = __loadErrors.get();
	std::ostringstream msg;

	if (!prev.size())
	{
		msg << "Config loading issues encountered:\n";
	}
	else
	{
		msg << prev << "\n";
	}

	switch(err)
	{
		case ELoadError::MissingKey:
			msg << "Missing " << keyName;
			break;
	
		case ELoadError::ParsingException:
			msg << "Failed to parse " << keyName;
			break;

		default:
			break;
	}

	__loadErrors = msg.str();
}

bool CConfig::loadSettings(bool firstLoad)
{
	YAML::Node node;
	try
	{
		node = YAML::LoadFile(getPath());
	}
	catch (YAML::BadFile& bf)
	{
		LOG_NOTIFYLONG("Can not read config.yaml! %s\nUsing defaults", bf.msg.c_str());
		node = YAML::Node(); //Create empty node and let defaults kick in
	}
	catch (YAML::ParserException& pe)
	{
		LOG_NOTIFYLONG("Error parsing config.yaml! %s\nUsing defaults", pe.msg.c_str());
		node = YAML::Node(); //Create empty node and let defaults kick in
	}

	__loadErrors = std::string("");
	
	//Parse logLevels first, otherwise settings won't get logged
	logLevels = getSetting<uint32_t>(node, "LogLevels", 0xff, true);
	api = getSetting<bool>(node, "API", true);
	if (api.get())
	{
		logLevels = logLevels.get() | k_ELogLevelAPI;
	}

	//This is shitty, but to do it properly have to do something even shittier
	LOG_CUSTOM
	(
		k_ELogLevelInfo | k_ELogLevelOnce,
		"LogLevels is \"%s\"\n",

		ELogLevel_ToString(logLevels.get()).c_str()
	);

	disableFamilyLock = getSetting<bool>(node, "DisableFamilyShareLock", true);
	useWhiteList = getSetting<bool>(node, "UseWhitelist", false);
	maxSchemaTries = getSetting<uint32_t>(node, "MaxSchemaTries", 10);
	safeMode = getSetting<bool>(node, "SafeMode", false);
	warnHashMissmatch = getSetting<bool>(node, "WarnHashMissmatch", false);
	notifyInit = getSetting<bool>(node, "NotifyInit", true);
	fakeName = getSetting<std::string>(node, "FakeName", "");
	fakeEmail = getSetting<std::string>(node, "FakeEmail", "");
	fakeWalletBalance = getSetting<int32_t>(node, "FakeWalletBalance", 0);
	disableCloud = getSetting<bool>(node, "DisableCloud", true);
	disableUpdates = getSetting<bool>(node, "DisableUpdates", true);
	syncRemoteAdditionalApps = node["SyncRemoteAdditionalApps"]
		? getSetting<bool>(node, "SyncRemoteAdditionalApps", false)
		: false;
	dumpInterfaceMaps = getSetting<bool>(node, "DumpClientInterfaces", false);
	extendedLogging = getSetting<bool>(node, "ExtendedLogging", false);

	const std::lock_guard appsChanged(appsChangedMutex);
	const auto prevAppIds = addedAppIds.get();
	const auto _addedAppIds = getList<AppId_t>(node, "AdditionalApps");
	bool localAppsRemoved = false;

	if (!firstLoad)
	{
		for (const auto& appId : prevAppIds)
		{
			if (_addedAppIds.contains(appId))
			{
				continue;
			}

			removedApps.emplace(appId);
			localAppsRemoved = true;
			LOG_DEBUG("AppId %u removed from AdditionalApps\n", appId);
		}
		for (const auto& appId : _addedAppIds)
		{
			if (prevAppIds.contains(appId))
			{
				continue;
			}

			newApps.emplace(appId);
			LOG_DEBUG("AppId %u added to AdditionalApps\n", appId);
		}
	}

	addedAppIds = _addedAppIds;
	if (localAppsRemoved && syncRemoteAdditionalApps.get())
	{
		//Broadcast after addedAppIds contains the new snapshot.
		Apps::requestRemoteAppBroadcast();
		LOG_DEBUG("REMOTE BROADCAST QUEUED reason=config-remove\n");
	}

	appIds = getList<AppId_t>(node, "AppIds");
	fakeOffline = getList<AppId_t>(node, "FakeOffline");
	depotBlacklist = getList<AppId_t>(node, "DepotBlacklist");

	fakeAppIds = getMap<AppId_t, AppId_t>(node, "FakeAppIds");
	manifestIds = getMap<AppId_t, uint64_t>(node, "ManifestIds");
	appTokens = getMap<AppId_t, uint64_t>(node, "AppTokens");
	cdKeys = getMap<AppId_t, std::string>(node, "CDKeys", true);
	gameTitles = getMap<AppId_t, std::string>(node, "GameTitles");
	subscriptionTimestamps = getMap<AppId_t, uint32_t>(node, "SubscriptionTimestamps");
	steamIdOverride = getMap<AppId_t, uint64_t>(node, "SteamIdOverride");

	//Do not log the keys themself
	for (const auto& key : cdKeys.get())
	{
		LOG_CUSTOM(k_ELogLevelInfo | k_ELogLevelOnce, "Added CDKey for %u\n", key.first);
	}

	//Do not warn for these (yet?)
	const auto idleStatusNode = node["IdleStatus"];
	if (idleStatusNode)
	{
		try
		{
			const auto appId = idleStatusNode["AppId"].as<AppId_t>();
			const auto title = idleStatusNode["Title"].as<std::string>();

			idleStatus = FakeGame_t
			{
				appId,
				title
			};

			LOG_CUSTOM(k_ELogLevelInfo | k_ELogLevelOnce, "Idle status %s with AppId %u\n", title.c_str(), appId);
		}
		catch(...)
		{
			//LOG_NOTIFYWARN("Failed to parse IdleStatus!");A
			setError(ELoadError::ParsingException, "IdleStatus");
		}
	}

	const auto dlcDataNode = node["DlcData"];
	if (dlcDataNode)
	{
		auto _dlcData = dlcData.empty();

		for (auto& app : dlcDataNode)
		{
			try
			{
				const AppId_t parentId = app.first.as<AppId_t>();
				LOG_CUSTOM(k_ELogLevelInfo | k_ELogLevelOnce, "Parsing DlcData for %u\n", parentId);
				const auto dlcIds = getMap<AppId_t, std::string>(dlcDataNode, std::to_string(parentId).c_str());

				CDlcData& data = _dlcData[parentId];
				data.parentId = parentId;
				data.dlcIds = dlcIds;
			}
			catch(...)
			{
				//LOG_NOTIFY("Failed to parse DlcData!");
				setError(ELoadError::ParsingException, "DlcData");
				break;
			}
		}

		dlcData = _dlcData;
	}
	else
	{
		//LOG_NOTIFY("Missing DlcData entry in config!");
		setError(ELoadError::MissingKey, "DlcData");
	}

	const auto denuvoGamesNode = node["DenuvoGames"];
	if (denuvoGamesNode)
	{
		auto _denuvoGames = denuvoGames.empty();

		for (auto& steamIdNode : denuvoGamesNode)
		{
			try
			{
				const uint64_t steamId = steamIdNode.first.as<uint64_t>();
				_denuvoGames[steamId] = std::unordered_set<AppId_t>();

				for (auto& appIdNode : steamIdNode.second)
				{
					const AppId_t appId = appIdNode.as<AppId_t>();
					_denuvoGames[steamId].emplace(appId);

					//Again, not loggin SteamId because of privacy
					LOG_CUSTOM(k_ELogLevelInfo | k_ELogLevelOnce, "Added DenuvoGame %u\n", appId);
				}
			}
			catch (...)
			{
				//LOG_NOTIFY("Failed to parse DenuvoGames!");
				setError(ELoadError::ParsingException, "DenuvoGames");
			}
		}

		denuvoGames.set(_denuvoGames);
	}
	else
	{
		//LOG_NOTIFY("Missing DenuvoGames entry in config!");
		setError(ELoadError::MissingKey, "DenuvoGames");
	}

	const auto errors = __loadErrors.get();
	if (errors.size())
	{
		//We know this isn't build by user input, so disabling the warning is fine for this line
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wformat-security"

		LOG_NOTIFYWARN(errors.c_str());

		#pragma GCC diagnostic pop
	}

	return true;
}

bool CConfig::isAddedAppId(const AppId_t appId)
{
	return addedAppIds.get().contains(appId);
}

bool CConfig::shouldExcludeAppId(const AppId_t appId, const bool ignoreAdditionalApps)
{
	bool exclude = false;
	//Proper way would be with getAppType, but that seems broken so we need to do this instead
	constexpr AppId_t ONE_BILLION = 1E9; //Implicit cast from double to unsigned int, hopefully this does not break anything
	if (appId >= ONE_BILLION) //Higher and equal to 10^9 gets used by Steam Internally
	{
		exclude = true;
	}
	else
	{
		const bool whitelist = useWhiteList.get();
		const bool found = appIds.get().contains(appId);
		exclude = (!isAddedAppId(appId) || ignoreAdditionalApps) && ((whitelist && !found) || (!whitelist && found));

		if (!ignoreAdditionalApps)
		{
			const auto usr = g_pSteamEngine->getUser();
			const auto appInfo = usr->getClientApps();

			//Might be worth to check for APPTYPE_DLC, but knowing Valve & individual gamedevs
			//surely not every DLC will be tagged as such
			char chParent[16] { };
			const int len = usr ? appInfo->getAppData(appId, "parent", chParent, sizeof(chParent)) : 0;
			//Do not blindly trust len, nor the str included. Some devs just like to mess with Valve or something (for example appId 221300)
			if (len > 0 && Utils::isNumber(chParent))
			{
				//LOG_DEBUG("AppId %i, parent %s (%i)\n", appId, chParent, len);
				AppId_t parentId = std::stoul(chParent);

				if (whitelist && !shouldExcludeAppId(parentId, true))
				{
					//LOG_DEBUG("Override exclude %i with false, because parent %u isn't excluded\n", exclude, parentId);
					exclude = false;
				}
				else if (!whitelist && shouldExcludeAppId(parentId, true))
				{
					//LOG_DEBUG("Override exclude %i with true, because parent %u is excluded\n", exclude, parentId);
					exclude = true;
				}
			}
		}
	}

	LOG_ONCE("shouldExcludeAppId(%u) -> %i\n", appId, exclude);
	return exclude;
}

CSteamId CConfig::getDenuvoGameOwner(const AppId_t appId)
{
	for (const auto& tpl : denuvoGames.get())
	{
		if (tpl.second.contains(appId))
		{
			//LOG_ONCE("%u is DenuvoGame\n", appId);
			return CSteamId(tpl.first);
		}
	}

	return CSteamId();
}

CConfig g_config = CConfig();
