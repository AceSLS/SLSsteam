#include "config.hpp"
#include "config_default.hpp"

#include "sdk/IClientApps.hpp"

#include "filewatcher.hpp"
#include "log.hpp"

#include "yaml-cpp/yaml.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>


std::string CConfig::getDir() const
{
	char pathBuf[255];
	const char* configDir = getenv("XDG_CONFIG_HOME"); //Most users should have this set iirc
	if (configDir != NULL)
	{
		sprintf(pathBuf, "%s/SLSsteam", configDir);
	}
	else
	{
		const char* home = getenv("HOME");
		sprintf(pathBuf, "%s/.config/SLSsteam", home);
	}

	return std::string(pathBuf);
}

std::string CConfig::getPath() const
{
	return getDir().append("/config.yaml");
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
				g_pLog->notify("Unable to create config directory at %s!\n", dir.c_str());
				return false;
			}

			g_pLog->debug("Created config directory at %s\n", dir.c_str());
		}

		FILE* file = fopen(path.c_str(), "w");
		if (!file)
		{
			g_pLog->notify("Unable to create config at %s!\n", path.c_str());
			return false;
		}

		fputs(defaultConfig, file);
		fflush(file);
		fclose(file);
	}

	return true;
}

static void onFileChange()
{
	g_config.loadSettings();
	g_pLog->notify("Config reloaded!");
}

bool CConfig::init()
{
	if(createFile())
	{
		watcher = new CFileWatcher(onFileChange);
		watcher->addFile(getPath().c_str());
		watcher->start();
	}

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


void CConfig::setError(ELoadError err)
{
	if (__loadErrors.get() > err)
	{
		return;
	}

	__loadErrors = err;
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
		g_pLog->notifyLong("Can not read config.yaml! %s\nUsing defaults", bf.msg.c_str());
		node = YAML::Node(); //Create empty node and let defaults kick in
	}
	catch (YAML::ParserException& pe)
	{
		g_pLog->notifyLong("Error parsing config.yaml! %s\nUsing defaults", pe.msg.c_str());
		node = YAML::Node(); //Create empty node and let defaults kick in
	}

	__loadErrors = ELoadError::None;
	
	disableFamilyLock = getSetting<bool>(node, "DisableFamilyShareLock", true);
	useWhiteList = getSetting<bool>(node, "UseWhitelist", false);
	maxSchemaTries = getSetting<uint32_t>(node, "MaxSchemaTries", 10);
	safeMode = getSetting<bool>(node, "SafeMode", false);
	notifications = getSetting<bool>(node, "Notifications", true);
	warnHashMissmatch = getSetting<bool>(node, "WarnHashMissmatch", false);
	notifyInit = getSetting<bool>(node, "NotifyInit", true);
	api = getSetting<bool>(node, "API", true);
	fakeEmail = getSetting<std::string>(node, "FakeEmail", "");
	fakeWalletBalance = getSetting<int32_t>(node, "FakeWalletBalance", 0);
	disableCloud = getSetting<bool>(node, "DisableCloud", true);
	disableUpdates = getSetting<bool>(node, "DisableUpdates", true);
	dumpInterfaceMaps = getSetting<bool>(node, "DumpClientInterfaces", false);
	extendedLogging = getSetting<bool>(node, "ExtendedLogging", false);
	logLevel = getSetting<unsigned int>(node, "LogLevel", 2);

	//TODO: Create smart logging function to log them automatically via getSetting
	g_pLog->info("DisableFamilyShareLock: %i\n", disableFamilyLock.get());
	g_pLog->info("UseWhitelist: %i\n", useWhiteList.get());
	g_pLog->info("MaxSchemaTries: %u\n", maxSchemaTries.get());
	g_pLog->info("SafeMode: %i\n", safeMode.get());
	g_pLog->info("Notifications: %i\n", notifications.get());
	g_pLog->info("WarnHashMissmatch: %i\n", warnHashMissmatch.get());
	g_pLog->info("NotifyInit: %i\n", notifyInit.get());
	g_pLog->info("API: %i\n", api.get());
	g_pLog->info("FakeEmail: %s\n", fakeEmail.get().c_str());
	g_pLog->info("FakeWalletBalance: %i\n", fakeWalletBalance.get());
	g_pLog->info("DisableCloud: %i\n", disableCloud.get());
	g_pLog->info("DisableUpdates: %i\n", disableUpdates.get());
	g_pLog->info("DumpClientInterfaces: %i\n", dumpInterfaceMaps.get());
	g_pLog->info("ExtendedLogging: %i\n", extendedLogging.get());
	g_pLog->info("LogLevel: %i\n", logLevel.get());

	const std::lock_guard appsChanged(appsChangedMutex);
	const auto prevAppIds = addedAppIds.get();
	const auto _addedAppIds = getList<AppId_t>(node, "AdditionalApps");

	if (!firstLoad)
	{
		for(const auto& appId : prevAppIds)
		{
			if (_addedAppIds.contains(appId))
			{
				continue;
			}

			removedApps.emplace(appId);
			g_pLog->debug("AppId %u removed from AdditionalApps\n", appId);
		}
		for(const auto& appId : _addedAppIds)
		{
			if (prevAppIds.contains(appId))
			{
				continue;
			}

			newApps.emplace(appId);
			g_pLog->debug("AppId %u added to AdditionalApps\n", appId);
		}
	}

	reloadDepots = true;

	std::lock_guard packagesChanged(packagesChangedMutex);
	const auto prevPackageIds = addedPackageIds.get();
	auto _addedPackageIds = getList<uint32_t>(node, "AdditionalPackages");

	if (!firstLoad)
	{
		for(const auto& pkg : _addedPackageIds)
		{
			if (prevPackageIds.contains(pkg))
			{
				continue;
			}

			newPackages.emplace(pkg);
			g_pLog->debug("PackageId %u added to AdditionalPackageIds\n", pkg);
		}

		for(const auto& pkg : prevPackageIds)
		{
			if (_addedPackageIds.contains(pkg))
			{
				continue;
			}

			removedPackages.emplace(pkg);
			g_pLog->debug("PackageId %u removed from AdditionalPackageIds\n", pkg);
		}
	}

	addedAppIds = _addedAppIds;
	addedPackageIds = _addedPackageIds;

	appIds = getList<AppId_t>(node, "AppIds");
	addedDepotIds = getList<AppId_t>(node, "AdditionalDepots");
	fakeOffline = getList<AppId_t>(node, "FakeOffline");
	depotBlacklist = getList<AppId_t>(node, "DepotBlacklist");

	fakeAppIds = getMap<AppId_t, AppId_t>(node, "FakeAppIds");
	manifestIds = getMap<AppId_t, uint64_t>(node, "ManifestIds");
	appTokens = getMap<AppId_t, uint64_t>(node, "AppTokens");
	gameTitles = getMap<AppId_t, std::string>(node, "GameTitles");
	subscriptionTimestamps = getMap<AppId_t, uint32_t>(node, "SubscriptionTimestamps");

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

			g_pLog->info("Idle status %s with AppId %u\n", title.c_str(), appId);
		}
		catch(...)
		{
			//g_pLog->warn("Failed to parse IdleStatus!");A
			setError(ELoadError::ParsingException);
		}
	}

	const auto dlcDataNode = node["DlcData"];
	if(dlcDataNode)
	{
		auto _dlcData = dlcData.empty();

		for(auto& app : dlcDataNode)
		{
			try
			{
				const AppId_t parentId = app.first.as<AppId_t>();

				CDlcData data;
				data.parentId = parentId;
				g_pLog->info("Adding DlcData for %u\n", parentId);

				for(auto& dlc : app.second)
				{
					const AppId_t dlcId = dlc.first.as<AppId_t>();
					//There's more efficient types to store strings, but they mostly do not work
					const std::string dlcName = dlc.second.as<std::string>();

					data.dlcIds[dlcId] = dlcName;
					g_pLog->info("DlcId %u -> %s\n", dlcId, dlcName.c_str());
				}

				_dlcData[parentId] = data;
			}
			catch(...)
			{
				//g_pLog->notify("Failed to parse DlcData!");
				setError(ELoadError::ParsingException);
				break;
			}
		}

		dlcData = _dlcData;
	}
	else
	{
		//g_pLog->notify("Missing DlcData entry in config!");
		setError(ELoadError::MissingKey);
	}

	const auto inventoryItemsNode = node["InventoryItems"];
	if(inventoryItemsNode)
	{
		auto _inventoryItems = inventoryItems.empty();

		for(auto& app : inventoryItemsNode)
		{
			try
			{
				const uint32_t appId = app.first.as<uint32_t>();

				InventoryItems_t items;
				items.appId = appId;
				g_pLog->info("Adding InventoryItems for %u\n", appId);

				for(auto& item : app.second)
				{
					const uint32_t id = item.first.as<uint32_t>();
					const uint32_t count = item.second.as<uint32_t>();

					items.items[id] = count;
					g_pLog->info("InventoryItem %u -> %u\n", id, count);
				}

				_inventoryItems[appId] = items;
			}
			catch(...)
			{
				g_pLog->notify("Failed to parse InventoryItems!");
				break;
			}
		}

		inventoryItems = _inventoryItems;
	}
	else
	{
		g_pLog->notify("Missing InventoryItems entry in config!");
	}

	const auto denuvoGamesNode = node["DenuvoGames"];
	if (denuvoGamesNode)
	{
		auto _denuvoGames = denuvoGames.empty();

		for (auto& steamIdNode : denuvoGamesNode)
		{
			try
			{
				const uint32_t steamId = steamIdNode.first.as<uint32_t>();
				_denuvoGames[steamId] = std::unordered_set<AppId_t>();

				for (auto& appIdNode : steamIdNode.second)
				{
					const AppId_t appId = appIdNode.as<AppId_t>();
					_denuvoGames[steamId].emplace(appId);

					//Again, not loggin SteamId because of privacy
					g_pLog->info("Added DenuvoGame %u\n", appId);
				}
			}
			catch (...)
			{
				//g_pLog->notify("Failed to parse DenuvoGames!");
				setError(ELoadError::ParsingException);
			}
		}

		denuvoGames.set(_denuvoGames);
	}
	else
	{
		//g_pLog->notify("Missing DenuvoGames entry in config!");
		setError(ELoadError::MissingKey);
	}

	switch(__loadErrors.get())
	{
		case ELoadError::MissingKey:
			g_pLog->notify("Issues during config loading encountered! Missing key(s)");
			break;
		case ELoadError::ParsingException:
			g_pLog->notify("Issues during config loading encountered! Parsing error(s)");
			break;

		default:
			break;
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
			//Might be worth to check for APPTYPE_DLC, but knowing Valve & individual gamedevs
			//surely not every DLC will be tagged as such
			char chParent[16];
			const int len = g_pClientApps ? g_pClientApps->getAppData(appId, "parent", chParent, sizeof(chParent)) : 0;
			if (len > 0)
			{
				//g_pLog->debug("AppId %i, parent %s (%i)\n", appId, chParent, len);
				AppId_t parentId = std::stoul(chParent);

				if (whitelist && !shouldExcludeAppId(parentId, true))
				{
					//g_pLog->debug("Override exclude %i with false, because parent %u isn't excluded\n", exclude, parentId);
					exclude = false;
				}
				else if(!whitelist && shouldExcludeAppId(parentId, true))
				{
					//g_pLog->debug("Override exclude %i with true, because parent %u is excluded\n", exclude, parentId);
					exclude = true;
				}
			}
		}
	}

	g_pLog->once("shouldExcludeAppId(%u) -> %i\n", appId, exclude);
	return exclude;
}

uint32_t CConfig::getDenuvoGameOwner(const AppId_t appId)
{
	for(const auto& tpl : denuvoGames.get())
	{
		if (tpl.second.contains(appId))
		{
			//g_pLog->once("%u is DenuvoGame\n", appId);
			return tpl.first;
		}
	}

	return 0;
}

CConfig g_config = CConfig();
