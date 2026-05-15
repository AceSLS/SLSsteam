#include "apps.hpp"

#include "../sdk/CAppOwnershipInfo.hpp"
#include "../sdk/CProtoBufMsgBase.hpp"
#include "../sdk/CSteamEngine.hpp"
#include "../sdk/CUser.hpp"
#include "../sdk/EReleaseState.hpp"
#include "../sdk/IClientApps.hpp"

#include "../config.hpp"
#include "../globals.hpp"

#include "fakeappid.hpp"

#include <cstdio>
#include <string>
#include <unordered_map>

bool Apps::applistRequested;
std::map<uint32_t, int> Apps::appIdOwnerOverride;

namespace
{
	constexpr uint32_t SHORTCUT_APP_ID_HIGH_BIT = 0x80000000;
	constexpr uint64_t SHORTCUT_GAME_ID_LOW_BITS = 0x02000000;
	constexpr uint64_t SHORTCUT_GAME_ID_LOW_MASK = 0xffffffff;

	uint32_t makeShortcutAppId(uint32_t appId)
	{
		return SHORTCUT_APP_ID_HIGH_BIT | appId;
	}

	uint64_t makeShortcutGameId(uint32_t appId)
	{
		return (static_cast<uint64_t>(makeShortcutAppId(appId)) << 32) | SHORTCUT_GAME_ID_LOW_BITS;
	}

	std::string getPresenceTitle(uint32_t appId, const std::unordered_map<uint32_t, std::string>& titles, const std::string& existingTitle = "")
	{
		if (titles.contains(appId))
		{
			return titles.at(appId);
		}

		if (g_pClientApps)
		{
			char name[256] {};
			g_pClientApps->getAppData(appId, "common/name", name, sizeof(name));
			if (name[0])
			{
				g_pLog->debug("AppName %s\n", name);
				return name;
			}
		}

		if (!existingTitle.empty())
		{
			return existingTitle;
		}

		char fallback[32] {};
		std::snprintf(fallback, sizeof(fallback), "App %u", appId);
		return fallback;
	}
}

bool Apps::isShortcutGameId(uint64_t gameId)
{
	return (gameId >> 32) && ((gameId & SHORTCUT_GAME_ID_LOW_MASK) == SHORTCUT_GAME_ID_LOW_BITS);
}

bool Apps::unlockApp(uint32_t appId, CAppOwnershipInfo* info, uint32_t ownerId)
{
	//Changing the purchased field is enough, but just for nicety in the Steamclient UI we change the owner too
	info->owner = ownerId;
	info->realOwner = 0;
	info->familyShared = ownerId != g_currentSteamId;

	info->licensePermanent = !info->familyShared;
	info->retailLicense = false;
	info->licenseExpired = false;
	info->licensePending = false;
	info->licenseLocked = false;

	info->releaseState = ERELEASESTATE_RELEASED;
	info->ownsLicense = true;

	info->lowViolence = false;
	info->regionRestricted = false;

	info->autoGrant = false;
	info->trialTime = 0;
	info->fromFreeWeekend = false;
	info->freeLicense = info->familyShared;
	info->siteLicense = false;

	g_pLog->once("Unlocked %u\n", appId);
	return true;
}

bool Apps::unlockApp(uint32_t appId, CAppOwnershipInfo* info)
{
	return unlockApp(appId, info, g_currentSteamId);
}

bool Apps::checkAppOwnership(uint32_t appId, CAppOwnershipInfo* pInfo)
{
	//Wait Until GetSubscribedApps gets called once to let Steam request and populate legit data first.
	//Afterwards modifying should hopefully not affect false positives anymore
	if (!applistRequested || !pInfo || !g_currentSteamId)
	{
		return false;
	}

	const uint32_t denuvoOwner = g_config.getDenuvoGameOwner(appId);

	//Do not modify Denuvo enabled Games
	if (denuvoOwner && denuvoOwner != g_currentSteamId)
	{
		//Would love to log the SteamId, but for users anonymity I won't
		g_pLog->once("Skipping %u because it's a Denuvo game from someone else\n", appId);
		return false;
	}

	if (g_config.shouldExcludeAppId(appId))
	{
		return false;
	}

	if (pInfo->lowViolence)
	{
		pInfo->lowViolence = false;
		g_pLog->once("Decensoring %u\n", appId);
	}
	if (pInfo->regionRestricted)
	{
		pInfo->regionRestricted = false;
		g_pLog->once("Bypassing region restriction for %u\n", appId);
	}

	const auto times = g_config.subscriptionTimestamps.get();
	if (times.contains(appId))
	{
		pInfo->purchaseTime = times.at(appId);
	}

	const bool manualUnlock = g_config.isAddedAppId(appId);
	if (!manualUnlock && (!g_config.playNotOwnedGames.get() || pInfo->ownsLicense))
	{
		return false;
	}

	if (!manualUnlock && g_config.automaticFilter.get())
	{
		//Returning false after we modify data shouldn't cause any problems because it should just get discarded
		if (!g_pClientApps)
		{
			return false;
		}

		auto type = g_pClientApps->getAppType(appId);
		if (type == APPTYPE_DLC) //Don't touch DLC here, otherwise downloads might break. Hopefully this won't decrease compatibility
		{
			return false;
		}

		switch(type)
		{
			case APPTYPE_APPLICATION:
			case APPTYPE_GAME:
				break;

			default:
				return false;
		}
	}

	unlockApp(appId, pInfo);

	return true;
}

void Apps::getSubscribedApps(uint32_t* appList, size_t size, uint32_t& count)
{
	//Valve calls this function twice, once with size of 0 then again
	if (!size || !appList)
	{
		count = count + g_config.addedAppIds.get().size();
		return;
	}

	//TODO: Maybe Add check if AppId already in list before blindly appending
	for(auto& appId : g_config.addedAppIds.get())
	{
		appList[count++] = appId;
	}

	applistRequested = true;
}

bool Apps::shouldDisableCloud(uint32_t appId)
{
	if (!g_config.disableCloud.get())
		return false;

	return !g_pSteamEngine->getUser(0)->isSubscribed(appId);
}

bool Apps::shouldDisableCDKey(uint32_t appId)
{
	return !g_pSteamEngine->getUser(0)->isSubscribed(appId);
}

bool Apps::shouldDisableUpdates(uint32_t appId)
{
	//Using AdditionalApps here aswell so users can manually block updates
	return g_config.isAddedAppId(appId) || !g_pSteamEngine->getUser(0)->isSubscribed(appId);
}

void Apps::sendGamesPlayed(CMsgClientGamesPlayed* msg)
{
	auto titles = g_config.gameTitles.get();
	bool owned = false;
	const auto addedAppIds = g_config.addedAppIds.get();

	for(int i = 0; i < msg->games_played_size(); i++)
	{
		auto game = CMsgClientGamesPlayed_GamePlayed(msg->games_played(i));
		const uint64_t originalGameId = game.game_id();

		if (!originalGameId)
		{
			continue;
		}

		const uint32_t gameAppId = static_cast<uint32_t>(originalGameId);
		const uint32_t launchedAppId = FakeAppIds::getRealAppIdForCurrentPipe(false);
		const uint32_t additionalAppId = launchedAppId ? launchedAppId : gameAppId;
		const bool additionalApp = addedAppIds.contains(additionalAppId);

		if(!owned && !additionalApp && g_pSteamEngine->getUser(0)->isSubscribed(gameAppId))
		{
			owned = true;
		}

		if (g_config.disableFamilyLock.get())
		{
			game.set_owner_id(1);
		}

		if (additionalApp)
		{
			const auto title = getPresenceTitle(additionalAppId, titles, game.game_extra_info());
			const uint64_t shortcutGameId = makeShortcutGameId(additionalAppId);

			game.set_game_id(shortcutGameId);
			game.set_game_extra_info(title);

			g_pLog->debug("Setting AdditionalApp %u presence to shortcut %llu (%s); local game_id was %llu\n", additionalAppId, shortcutGameId, title.c_str(), originalGameId);
		}
		else if (titles.contains(gameAppId))
		{
			game.set_game_extra_info(titles[gameAppId]);
		}
		else if (!owned || FakeAppIds::getFakeAppId(gameAppId))
		{
			game.set_game_extra_info(getPresenceTitle(gameAppId, titles));
		}

		msg->mutable_games_played(i)->ParseFromString(game.SerializeAsString());

		g_pLog->debug("Playing game %llu with flags %u & pid %u\n", game.game_id(), game.game_flags(), game.process_id());
	}

	if (owned || msg->games_played_size() > 0)
	{
		return;
	}

	const auto statusApp = g_config.idleStatus.get();
	if (statusApp.appId)
	{
		auto game = msg->add_games_played();
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

void Apps::sendPICSInfoRequest(CMsgClientPICSProductInfoRequest* msg)
{
	const auto tokens = g_config.appTokens.get();

	for(int i = 0; i < msg->apps_size(); i++)
	{
		auto app = msg->mutable_apps(i);
		if (tokens.contains(app->appid()))
		{
			app->set_access_token(tokens.at(app->appid()));
			g_pLog->debug("Used access token from config for %u\n", app->appid());
		}
	}
}

void Apps::sendMsg(CProtoBufMsgBase *msg)
{
	switch(msg->type)
	{
		case EMSG_PICS_PRODUCTINFO_REQUEST:
			sendPICSInfoRequest(msg->getBody<CMsgClientPICSProductInfoRequest>());
			break;

		case EMSG_GAMESPLAYED:
		case EMSG_GAMESPLAYED_NO_DATABLOB:
		case EMSG_GAMESPLAYED_WITH_DATABLOB:
			sendGamesPlayed(msg->getBody<CMsgClientGamesPlayed>());
			break;
	}
}
