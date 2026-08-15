#include "apps.hpp"

#include "../config.hpp"
#include "../globals.hpp"
#include "../utils.hpp"

#include "fakeappid.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>


bool Apps::applistRequested;
std::unordered_set<AppId_t> Apps::privateApps = std::unordered_set<AppId_t>();

std::mutex Apps::pendingLicenseChangesMutex;
std::unordered_set<AppId_t> Apps::pendingLicenseChanges = std::unordered_set<AppId_t>();

namespace
{
	std::mutex remoteAppsMutex;
	std::unordered_set<AppId_t> remoteApps;
	std::atomic_uint64_t remoteAppBroadcastGeneration = 0;
}

bool Apps::updateRemoteApps(const std::unordered_set<AppId_t>& appIds)
{
	std::unordered_set<AppId_t> added;
	std::unordered_set<AppId_t> removed;

	{
		const std::lock_guard lock(remoteAppsMutex);

		for (const auto appId : appIds)
		{
			if (!remoteApps.contains(appId))
			{
				added.emplace(appId);
			}
		}

		for (const auto appId : remoteApps)
		{
			if (!appIds.contains(appId))
			{
				removed.emplace(appId);
			}
		}

		remoteApps = appIds;
	}

	if (added.empty() && removed.empty())
	{
		return false;
	}

	{
		const std::lock_guard lock(g_config.appsChangedMutex);

		for (const auto appId : added)
		{
			g_config.removedApps.erase(appId);
			if (!g_config.isAddedAppId(appId))
			{
				g_config.newApps.emplace(appId);
				LOG_DEBUG("REMOTE REGISTER app=%u queued for AppInfo\n", appId);
			}
			else
			{
				LOG_DEBUG("REMOTE REGISTER app=%u already present locally\n", appId);
			}
		}

		for (const auto appId : removed)
		{
			g_config.newApps.erase(appId);
			if (!g_config.isAddedAppId(appId))
			{
				g_config.removedApps.emplace(appId);
			}
			LOG_DEBUG("REMOTE REMOVE app=%u\n", appId);
		}
	}

	if (!removed.empty())
	{
		const std::lock_guard lock(pendingLicenseChangesMutex);
		for (const auto appId : removed)
		{
			pendingLicenseChanges.erase(appId);
		}
	}

	return !added.empty();
}

bool Apps::isRemoteApp(const AppId_t appId)
{
	const std::lock_guard lock(remoteAppsMutex);
	return remoteApps.contains(appId);
}

void Apps::requestRemoteAppBroadcast()
{
	remoteAppBroadcastGeneration.fetch_add(1);
}

uint64_t Apps::getRemoteAppBroadcastGeneration()
{
	return remoteAppBroadcastGeneration.load();
}

bool Apps::unlockApp(const AppId_t appId, AppOwnershipInfo_t* info, const CSteamId& ownerId)
{
	//Changing the purchased field is enough, but just for nicety in the Steamclient UI we change the owner too
	info->owner = ownerId.accountId();
	info->realOwner = 0;
	info->familyShared = info->owner != g_currentSteamId.accountId();

	info->licensePermanent = !info->familyShared;
	info->retailLicense = false;
	info->licenseExpired = false;
	info->licensePending = false;
	info->licenseLocked = false;

	info->releaseState = k_EAppReleaseStateReleased;
	info->ownsLicense = true;

	info->lowViolence = false;
	info->regionRestricted = false;

	info->autoGrant = false;
	info->trialTime = 0;
	info->fromFreeWeekend = false;
	info->freeLicense = info->familyShared;
	info->siteLicense = false;

	LOG_ONCE("Unlocked %u\n", appId);
	return true;
}

bool Apps::unlockApp(const AppId_t appId, AppOwnershipInfo_t* info)
{
	return unlockApp(appId, info, g_currentSteamId);
}

void Apps::buildDepotDependency(CUtlVector<DepotInfo_t>* depots, CUtlVector<DepotInfo_t>* sharedDepots)
{
	LOG_DEBUG("Vec Alloc %u, Grow %u, Size %u\n", depots->mem.alloc, depots->mem.growSize, depots->size);

	const auto depotBlacklist = g_config.depotBlacklist.get();
	const auto manifestOverrides = g_config.manifestIds.get();

	for (unsigned int i = 0; i < depots->size; i++)
	{
		const auto depot = depots->at(i);

		if (depotBlacklist.contains(depot->depotId))
		{
			LOG_DEBUG("Removing %u with %llu\n", depot->depotId, depot->manifestId);
			depots->swap(i, depots->size - 1);
			depots->size--;
		}

		if (manifestOverrides.contains(depot->depotId))
		{
			const uint64_t oldId = depot->manifestId;
			depot->manifestId = manifestOverrides.at(depot->depotId);
			LOG_DEBUG("Overrode %u's manifest %llu with %llu\n", depot->depotId, oldId, depot->manifestId);
		}

		LOG_DEBUG("Depot %u for %u -> %llu\n", depot->depotId, depot->appId, depot->manifestId);
	}

	for (unsigned int i = 0; i < sharedDepots->size; i++)
	{
		const auto depot = sharedDepots->at(i);
		LOG_DEBUG("Shared Depot %u for %u -> %llu\n", depot->depotId, depot->appId, depot->manifestId);
	}
}

bool Apps::checkAppOwnership(AppId_t appId, AppOwnershipInfo_t* pInfo)
{
	//Wait Until GetSubscribedApps gets called once to let Steam request and populate legit data first.
	//Afterwards modifying should hopefully not affect false positives anymore
	if (!applistRequested || !pInfo || !g_currentSteamId.isSet())
	{
		return false;
	}

	const CSteamId denuvoOwner = g_config.getDenuvoGameOwner(appId);

	//Do not modify Denuvo enabled Games
	if (denuvoOwner.isSet() && denuvoOwner.steamId64 != g_currentSteamId.steamId64)
	{
		//Would love to log the SteamId, but for users anonymity I won't
		LOG_ONCE("Skipping %u because it's a Denuvo game from someone else\n", appId);
		return false;
	}

	const bool remoteApp = isRemoteApp(appId);

	if (!remoteApp && g_config.shouldExcludeAppId(appId))
	{
		return false;
	}

	if (pInfo->lowViolence)
	{
		pInfo->lowViolence = false;
		LOG_ONCE("Decensoring %u\n", appId);
	}
	if (pInfo->regionRestricted)
	{
		pInfo->regionRestricted = false;
		LOG_ONCE("Bypassing region restriction for %u\n", appId);
	}

	const auto times = g_config.subscriptionTimestamps.get();
	if (times.contains(appId))
	{
		pInfo->purchaseTime = times.at(appId);
	}

	if (!remoteApp && !g_config.isAddedAppId(appId))
	{
		return false;
	}

	unlockApp(appId, pInfo);

	return true;
}

void Apps::getLegacyCDKey(const AppId_t appId)
{
	const auto user = g_pSteamEngine->getUser();
	if (user->isSubscribed(appId))
	{
		return;
	}

	std::string newKey;

	const auto keys = g_config.cdKeys.get();
	if (keys.contains(appId))
	{
		newKey = keys.at(appId);
		LOG_DEBUG("Using key from config for %u\n", appId);
	}
	else
	{
		constexpr const char* CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		static const unsigned int CHARS_SIZE = strlen(CHARS);

		//Some games use 5 SEGMENT_CHARS/5 SEGMENT_NUM or a mix of both
		//4 * 4 is the most common one though
		constexpr unsigned int SEGMENT_CHARS = 4;
		constexpr unsigned int SEGMENT_NUM = 4;

		constexpr unsigned int SEGMENT_SIZE = SEGMENT_CHARS + 1; //AAAA-, BBBB-, etc
		constexpr unsigned int KEY_SIZE = SEGMENT_SIZE * SEGMENT_NUM - 1; //Do not end with -
		
		//Don't forget null terminator since we
		//do not pass a size argument to SetLegacyCDKey
		newKey.resize(KEY_SIZE);

		srand(g_currentSteamId.steamId.accountId + appId);

		for (unsigned int i = 0; i < KEY_SIZE; i++)
		{
			if ((i + 1) % SEGMENT_SIZE == 0)
			{
				newKey[i] = '-';
				continue;
			}

			const unsigned int num = rand() % CHARS_SIZE;
			newKey[i] = CHARS[num];
		}

		LOG_DEBUG("Generated random key %s for %u\n", newKey.c_str(), appId);
	}

	const auto clientUser = user->getClientUser();

	//Wrapper function for CUser::SetLegacyCDKey, which gets called
	//from CCMInterface when a legacy cd key packet arrives
	//Injecting them only in GetLegacyCDKey doesn't work right, so we set it instead
	if (!clientUser->setLegacyCDKey(appId, newKey.c_str()))
	{
		LOG_ERROR("Failed to set CDKey for %u\n", appId);
	}
}

void Apps::getSubscribedApps(AppId_t* appList, const size_t size, uint32_t& count)
{
	auto apps = g_config.addedAppIds.get();
	{
		const std::lock_guard lock(remoteAppsMutex);
		apps.insert(remoteApps.begin(), remoteApps.end());
	}

	//Valve calls this function twice, once with size of 0 then again
	if (!size || !appList)
	{
		count = count + apps.size();
		return;
	}

	for (const auto appId : apps)
	{
		if (count >= size)
		{
			LOG_WARN("GetSubscribedApps has no room for transient app %u\n", appId);
			break;
		}

		if (std::find(appList, appList + count, appId) != appList + count)
		{
			LOG_DEBUG("GetSubscribedApps skipped duplicate transient app %u\n", appId);
			continue;
		}

		appList[count++] = appId;
	}

	applistRequested = true;
}

void Apps::parseProductInfoFromResponse(CMsgClientPICSProductInfoResponse* msg)
{
	auto set = std::unordered_set<AppId_t>();
	{
		const std::lock_guard lock(pendingLicenseChangesMutex);

		for (const auto& app : msg->apps())
		{
			if (!pendingLicenseChanges.contains(app.appid()))
			{
				continue;
			}

			set.emplace(app.appid());
			pendingLicenseChanges.erase(app.appid());
		}
	}

	postAppLicensesChanged(set);

	for (const auto appId : set)
	{
		if (isRemoteApp(appId))
		{
			LOG_DEBUG("REMOTE READY app=%u metadata received\n", appId);
		}
	}

	for (const auto appId : set)
	{
		if (g_config.isAddedAppId(appId))
		{
			requestRemoteAppBroadcast();
			LOG_DEBUG("REMOTE BROADCAST QUEUED reason=metadata app=%u\n", appId);
			break;
		}
	}
}

void Apps::postAppLicensesChanged(const std::unordered_set<AppId_t>& apps, const bool appsAdded)
{
	if (!apps.size())
	{
		return;
	}

	const auto user = g_pSteamEngine->getUser(0);
	if (!user)
	{
		return;
	}

	AppLicensesChanged_t cb { };
	unsigned int totalPackets = std::floor(apps.size() / AppLicensesChanged_t::MAX_APPS_PER_CALLBACK);

	for (unsigned int i = 0; i < apps.size(); i++)
	{
		unsigned int idx = i % AppLicensesChanged_t::MAX_APPS_PER_CALLBACK;

		cb.apps[idx] = *std::next(apps.begin(), i);
		cb.count = idx + 1;
		if (appsAdded)
		{
			cb.appsAdded |= 1llu << idx;
		}
		cb.remainingPackets = totalPackets;

		LOG_DEBUG("AppLicensesChanged_t.apps[%u] -> %u (i -> %i, packets left -> %i, appsAdded %llu)\n", idx, cb.apps[idx], i, totalPackets, cb.appsAdded);

		if (idx + 1 >= AppLicensesChanged_t::MAX_APPS_PER_CALLBACK)
		{
			user->postCallback(ECallbackType::AppLicensesChanged_t, &cb, sizeof(cb));
			totalPackets--;
			memset(&cb, 0, sizeof(cb));
		}
	}

	if (cb.count)
	{
		user->postCallback(ECallbackType::AppLicensesChanged_t, &cb, sizeof(cb));
	}

	std::ostringstream appsLog;
	for (const auto& app : apps)
	{
		appsLog << (appsLog.str().size() ? ", " : "") << app;
	}

	LOG_API("AppLicensesChanged callback invoked for %s!\n", appsLog.str().c_str());
}

void Apps::runIPCFrame()
{
	if (!g_config.syncRemoteAdditionalApps.get())
	{
		updateRemoteApps({});
	}

	const auto usr = g_pSteamEngine->getUser();
	if (!usr)
	{
		return;
	}

	const std::lock_guard appsChanged(g_config.appsChangedMutex);
	const auto appInfo = usr->getClientApps();

	if (g_config.removedApps.size())
	{
		postAppLicensesChanged(g_config.removedApps, false);
		g_config.removedApps.clear();
	}

	const auto added = g_config.newApps;

	if (!added.size())
	{
		return;
	}

	const std::lock_guard pendingLicensesLock(pendingLicenseChangesMutex);

	//Max batch of 15, otherwise not all apps will get a response which means they won't get added
	constexpr unsigned int MAX_APPS_PER_REQUEST = 15;
	AppId_t apps[MAX_APPS_PER_REQUEST] { };

	unsigned int i = 0;
	for (; i < added.size(); i++)
	{
		const unsigned int idx = i % MAX_APPS_PER_REQUEST;
		const AppId_t appId = *std::next(added.begin(), i);

		apps[idx] = appId;

		LOG_DEBUG("AppInfoRequest %u -> %u from (%i)\n", idx, apps[idx], i);

		if (idx + 1 >= MAX_APPS_PER_REQUEST)
		{
			appInfo->requestAppInfoUpdate(apps, MAX_APPS_PER_REQUEST);
			memset(apps, 0, sizeof(apps));
		}

		pendingLicenseChanges.emplace(appId);
	}

	const unsigned int idx = i % MAX_APPS_PER_REQUEST;
	if (apps[0])
	{
		appInfo->requestAppInfoUpdate(apps, idx);
	}

	g_config.newApps.clear();
}

bool Apps::shouldDisableCloud(const AppId_t appId)
{
	if (!g_config.disableCloud.get())
	{
		return false;
	}

	return !g_pSteamEngine->getUser(0)->isSubscribed(appId);
}

bool Apps::shouldDisableCDKey(const AppId_t appId)
{
	return !g_pSteamEngine->getUser(0)->isSubscribed(appId);
}

bool Apps::shouldDisableUpdates(const AppId_t appId)
{
	if (!g_config.disableUpdates.get())
	{
		return false;
	}

	//Using AdditionalApps here aswell so users can manually block updates
	return isRemoteApp(appId) || g_config.isAddedAppId(appId) || !g_pSteamEngine->getUser(0)->isSubscribed(appId);
}

void Apps::sendAndRecvLastPlayedTimes(const char* name, CPlayer_GetLastPlayedTimes_Response* recv)
{
	if (strcmp(name, "Player.ClientGetLastPlayedTimes#1") != 0)
	{
		return;
	}

	const auto apps = g_config.addedAppIds.get();
	for (int i = recv->games_size() - 1; i >= 0; i--)
	{
		auto game = recv->mutable_games(i);
		if (!apps.contains(game->appid()))
		{
			continue;
		}

		LOG_DEBUG("Removed serverside PlayTime for %u\n", game->appid());
		recv->mutable_games()->DeleteSubrange(i, 1);
	}
}

void Apps::sendGamesPlayed(CNetPacket* pkt)
{
	const auto titles = g_config.gameTitles.get();
	const auto usr = g_pSteamEngine->getUser();
	const auto appInfo = usr->getClientApps();

	auto msg = pkt->deserializeBody<CMsgClientGamesPlayed>();

	for (int i = 0; i < msg.games_played_size(); i++)
	{
		auto game = msg.mutable_games_played(i);
		if (!game->game_id())
		{
			continue;
		}

		const uint64_t gameId = game->game_id();

		// Native non-Steam shortcut IDs use 0x2000000 in their low 32 bits.
		// Leave the original shortcut title and 64-bit ID untouched.
		if (gameId & GAME_TYPE_SHORTCUT)
		{
			LOG_DEBUG("Preserving non-Steam shortcut %llu\n", gameId);
			continue;
		}

		if (g_config.disableFamilyLock.get())
		{
			game->set_owner_id(1);
		}

		if (titles.contains(gameId))
		{
			game->set_game_extra_info(titles.at(gameId));
		}
		//This probably belongs into FakeAppIds, but the GameTitles does not so it stays here
		else if (FakeAppIds::getFakeAppId(gameId))
		{
			char name[256] {}; //No clue how long titles can get
			int len;

			if (privateApps.contains(gameId))
			{
				strcpy(name, "Redacted");
				len = strlen(name);
			}
			else
			{
				len = appInfo->getAppData(gameId, "common/name", name, sizeof(name));
			}

			if (len > 0)
			{
				LOG_DEBUG("AppName %s (%i)\n", name, len);
				game->set_game_extra_info(name);
			}
		}

		//msg->mutable_games_played(i)->ParseFromString(game.SerializeAsString());

		LOG_DEBUG("Playing game %llu with flags %u & pid %u\n", gameId, game->game_flags(), game->process_id());
	}

	if (msg.games_played_size() < 1)
	{
		const auto statusApp = g_config.idleStatus.get();
		if (statusApp.appId)
		{
			auto game = msg.add_games_played();
			game->set_game_id(statusApp.appId);
			game->set_game_extra_info(statusApp.title);
			game->set_game_flags(0);

			if (g_config.disableFamilyLock.get())
			{
				game->set_owner_id(1);
			}
			//game->set_game_flags(EGAMEFLAG_MULTIPLAYER);
		}
	}

	pkt->serialize(msg);
}

void Apps::sendPICSInfoRequest(CNetPacket* pkt)
{
	const auto tokens = g_config.appTokens.get();
	auto msg = pkt->deserializeBody<CMsgClientPICSProductInfoRequest>();

	for (int i = 0; i < msg.apps_size(); i++)
	{
		auto app = msg.mutable_apps(i);
		if (tokens.contains(app->appid()))
		{
			app->set_access_token(tokens.at(app->appid()));
			LOG_DEBUG("Used access token from config for %u\n", app->appid());
		}
	}

	pkt->serialize(msg);
}

void Apps::sendMsg(CNetPacket *pkt)
{
	switch(pkt->getProtoBufType())
	{
		case k_EMsgClientPICSProductInfoRequest:
			sendPICSInfoRequest(pkt);
			break;

		case k_EMsgClientGamesPlayed:
		case k_EMsgClientGamesPlayedNoDataBlob:
		case k_EMsgClientGamesPlayedWithDataBlob:
			sendGamesPlayed(pkt);
			break;

		default:
			break;
	}
}

void Apps::setConfigStoreString(const char* key, const char* value)
{
	if (!std::string(key).starts_with("WebStorage\\PrivateApps"))
	{
		return;
	}

	LOG_DEBUG("%s -> %s\n", key, value);

	auto str = std::string(value);
	if (str.size() < 3) //List is empty, nope out
	{
		return;
	}

	privateApps.clear();
	str = str.substr(1, str.size() - 2); //[730,240,440,etc]
	const auto split = Utils::strsplit(const_cast<char*>(str.c_str()), ",");

	for (const auto& s : split)
	{
		if (!Utils::isNumber(s.c_str()))
		{
			LOG_WARN("%s is not a number! Skipping\n", s.c_str());
		}

		const AppId_t appId = std::stoul(s);
		privateApps.emplace(appId);
		LOG_DEBUG("Added %u to privateApps\n", appId);
	}
}
