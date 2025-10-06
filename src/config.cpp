#include "config.hpp"

#include "log.hpp"
#include "yaml-cpp/yaml.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

// TODO: Move into own .yaml file somehow
static const char *defaultConfig =
	"#Example AppIds Config for those not familiar with YAML:\n"
	"#AppIds:\n"
	"#  - 440\n"
	"#  - 730\n"
	"#Take care of not messing up your spaces! Otherwise it won't work\n\n"
	"#Example of DlcData:\n"
	"#DlcData:\n"
	"#  AppId:\n"
	"#    FirstDlcAppId: \"Dlc Name\"\n"
	"#    SecondDlcAppId: \"Dlc Name\"\n\n"
	"#Example of DenuvoGames:\n"
	"#DenuvoGames:\n"
	"#  SteamId:\n"
	"#    -  AppId1\n"
	"#    -  AppId2\n\n"
	"#Disables Family Share license locking for self and others\n"
	"DisableFamilyShareLock: yes\n\n"
	"#Switches to whitelist instead of the default blacklist\n"
	"UseWhitelist: no\n\n"
	"#Automatically filter Apps in CheckAppOwnership. Filters everything but "
	"Games and Applications. Should not affect DLC checks\n"
	"#Overrides black-/whitelist. Gets overriden by AdditionalApps\n"
	"AutoFilterList: yes\n\n"
	"#List of AppIds to ex-/include\n"
	"AppIds:\n\n"
	"#Enables playing of not owned games. Respects black-/whitelist AppIds\n"
	"PlayNotOwnedGames: no\n\n"
	"#Additional AppIds to inject (Overrides your black-/whitelist & also "
	"overrides OwnerIds for apps you got shared!) Best to use this only on "
	"games NOT in your library.\n"
	"AdditionalApps:\n\n"
	"#Extra Data for Dlcs belonging to a specific AppId. Only needed\n"
	"#when the App you're playing is hit by Steams 64 DLC limit\n"
	"DlcData:\n\n"
	"#Blocks games from unlocking on wrong accounts\n"
	"DenuvoGames:\n\n"
	"#Spoof Denuvo Games owner instead of blocking them\n"
	"DenuvoSpoof: no\n\n"
	"#Automatically disable SLSsteam when steamclient.so does not match a "
	"predefined file hash that is known to work\n"
	"#You should enable this if you're planing to use SLSsteam with Steam "
	"Deck's gamemode\n"
	"SafeMode: no\n\n"
	"#Toggles notifications via notify-send\n"
	"Notifications: yes\n\n"
	"#Warn user via notification when steamclient.so hash differs from known "
	"safe hash\n"
	"#Mostly useful for development so I don't accidentally miss an update\n"
	"WarnHashMissmatch: no\n\n"
	"#Notify when SLSsteam is done initializing\n"
	"NotifyInit: yes\n\n"
	"#Logs all calls to Steamworks (this makes the logfile huge! Only useful "
	"for debugging/analyzing\n"
	"ExtendedLogging: no";

std::string CConfig::getDir()
{
	std::filesystem::path configPath;
	// Use getenv to find the appropriate config directory path
	if (const char *configHome = std::getenv("XDG_CONFIG_HOME"))
	{
		configPath = configHome;
	}
	else if (const char *home = std::getenv("HOME"))
	{
		configPath = home;
		configPath /= ".config";
	}
	else
	{
		g_pLog->notify(
			"Could not find XDG_CONFIG_HOME or HOME for config directory!");
		return ""; // Return empty on failure
	}

	configPath /= "SLSsteam";
	return configPath.string();
}

std::string CConfig::getPath()
{
	std::filesystem::path dir = getDir();
	if (dir.empty())
	{
		return "";
	}
	return (dir / "config.yaml").string();
}

bool CConfig::createFile()
{
	std::string path = getPath();
	if (path.empty())
	{
		return false;
	}

	if (std::filesystem::exists(path))
	{
		return true; // File already exists
	}

	std::string dir = getDir();
	try
	{
		// Create the directory if it doesn't exist
		if (!std::filesystem::exists(dir))
		{
			if (!std::filesystem::create_directory(dir))
			{
				g_pLog->notify("Unable to create config directory at %s!\n",
							   dir.c_str());
				return false;
			}
			g_pLog->debug("Created config directory at %s\n", dir.c_str());
		}
	}
	catch (const std::filesystem::filesystem_error &e)
	{
		g_pLog->notify("Filesystem error while creating directory %s: %s\n",
					   dir.c_str(), e.what());
		return false;
	}

	std::ofstream file(path);
	if (!file.is_open())
	{
		g_pLog->notify("Unable to create config at %s!\n", path.c_str());
		return false;
	}

	file << defaultConfig;
	file.close();

	return true;
}

bool CConfig::init()
{
	if (createFile())
	{
		return loadSettings();
	}
	return false;
}

bool CConfig::loadSettings()
{
	YAML::Node node;
	try
	{
		node = YAML::LoadFile(getPath());
	}
	catch (const YAML::BadFile &bf)
	{
		g_pLog->notifyLong("Can not read config.yaml! %s\nUsing defaults",
						   bf.msg.c_str());
		node = YAML::Node(); // Create empty node and let defaults
	}
	catch (const YAML::ParserException &pe)
	{
		g_pLog->notifyLong("Error parsing config.yaml! %s\nUsing defaults",
						   pe.msg.c_str());
		node = YAML::Node(); // Create empty node and let defaults
	}

	// Smartly get and log settings
	disableFamilyLock =
		getAndLogSetting<bool>(node, "DisableFamilyShareLock", true);
	useWhiteList = getAndLogSetting<bool>(node, "UseWhitelist", false);
	automaticFilter = getAndLogSetting<bool>(node, "AutoFilterList", true);
	playNotOwnedGames =
		getAndLogSetting<bool>(node, "PlayNotOwnedGames", false);
	safeMode = getAndLogSetting<bool>(node, "SafeMode", false);
	notifications = getAndLogSetting<bool>(node, "Notifications", true);
	warnHashMissmatch =
		getAndLogSetting<bool>(node, "WarnHashMissmatch", false);
	notifyInit = getAndLogSetting<bool>(node, "NotifyInit", true);
	extendedLogging = getAndLogSetting<bool>(node, "ExtendedLogging", false);
	denuvoSpoof = getAndLogSetting<bool>(node, "DenuvoSpoof", false);

	// Use dedicated functions to parse complex nodes
	parseAppIdList(node, "AppIds", this->appIds);
	parseAppIdList(node, "AdditionalApps", this->addedAppIds);
	parseDlcData(node);
	parseDenuvoGames(node);

	return true;
}

// Helper to get a setting and also log its value.
template <typename T>
T CConfig::getAndLogSetting(const YAML::Node &node, const char *name, T defVal)
{
	T value = getSetting<T>(node, name, defVal);
	std::stringstream ss;
	ss << value;
	g_pLog->info("%s: %s\n", name, ss.str().c_str());
	return value;
}

// Explicit template instantiation for the linker
template bool CConfig::getAndLogSetting<bool>(const YAML::Node &, const char *,
											  bool);

// Helper function to parse a list of AppIDs from the YAML config.
void CConfig::parseAppIdList(const YAML::Node &parentNode, const char *key,
							 std::unordered_set<uint32_t> &targetSet)
{
	const YAML::Node &listNode = parentNode[key];
	if (!listNode)
	{
		g_pLog->notify("Missing %s entry in config!", key);
		return;
	}

	if (listNode.IsSequence())
	{
		for (const auto &itemNode : listNode)
		{
			try
			{
				uint32_t appId = itemNode.as<uint32_t>();
				targetSet.emplace(appId);
				g_pLog->info("Added %u to %s\n", appId, key);
			}
			catch (const YAML::Exception &)
			{
				g_pLog->notify("Failed to parse '%s' in %s!",
							   itemNode.as<std::string>().c_str(), key);
			}
		}
	}
}

// Helper function to parse the DlcData node.
void CConfig::parseDlcData(const YAML::Node &parentNode)
{
	const YAML::Node &dlcDataNode = parentNode["DlcData"];
	if (!dlcDataNode)
	{
		g_pLog->notify("Missing DlcData entry in config!");
		return;
	}

	if (dlcDataNode.IsMap())
	{
		for (const auto &app : dlcDataNode)
		{
			try
			{
				const uint32_t parentId = app.first.as<uint32_t>();

				CDlcData data;
				data.parentId = parentId;
				g_pLog->debug("Adding DlcData for %u\n", parentId);

				for (const auto &dlc : app.second)
				{
					const uint32_t dlcId = dlc.first.as<uint32_t>();
					const std::string dlcName = dlc.second.as<std::string>();

					data.dlcIds[dlcId] = dlcName;
					g_pLog->debug("DlcId %u -> %s\n", dlcId, dlcName.c_str());
				}
				dlcData[parentId] = data;
			}
			catch (const YAML::Exception &)
			{
				g_pLog->notify("Failed to parse DlcData!");
				break;
			}
		}
	}
}

// Helper function to parse the DenuvoGames node.
void CConfig::parseDenuvoGames(const YAML::Node &parentNode)
{
	const YAML::Node &denuvoGamesNode = parentNode["DenuvoGames"];
	if (!denuvoGamesNode)
	{
		g_pLog->notify("Missing DenuvoGames entry in config!");
		return;
	}

	if (denuvoGamesNode.IsMap())
	{
		for (const auto &steamIdNode : denuvoGamesNode)
		{
			try
			{
				const uint32_t steamId = steamIdNode.first.as<uint32_t>();
				denuvoGames[steamId] = std::unordered_set<uint32_t>();

				for (const auto &appIdNode : steamIdNode.second)
				{
					const uint32_t appId = appIdNode.as<uint32_t>();
					denuvoGames[steamId].emplace(appId);
					g_pLog->debug("Added DenuvoGame %u\n", appId);
				}
			}
			catch (const YAML::Exception &)
			{
				g_pLog->notify("Failed to parse DenuvoGames!");
			}
		}
	}
}

bool CConfig::isAddedAppId(uint32_t appId)
{
	return addedAppIds.contains(appId);
}

bool CConfig::addAdditionalAppId(uint32_t appId)
{
	if (isAddedAppId(appId))
		return false;

	addedAppIds.emplace(appId);
	g_pLog->once("Force owned %u\n", appId);
	return true;
}

bool CConfig::shouldExcludeAppId(uint32_t appId)
{
	bool exclude = false;
	constexpr uint32_t ONE_BILLION = 1'000'000'000;
	if (appId >= ONE_BILLION)  // Used by Steam internally
	{
		exclude = true;
	}
	else
	{
		bool found = appIds.contains(appId);
		exclude = !isAddedAppId(appId) &&
				  ((useWhiteList && !found) || (!useWhiteList && found));
	}

	g_pLog->once("shouldExcludeAppId(%u) -> %i\n", appId, exclude);
	return exclude;
}

uint32_t CConfig::getDenuvoGameOwner(uint32_t appId)
{
	for (const auto &tpl : denuvoGames)
	{
		if (tpl.second.contains(appId))
		{
			// g_pLog->once("%u is DenuvoGame\n", appId);
			return tpl.first;
		}
	}

	return 0;
}

CConfig g_config = CConfig();
