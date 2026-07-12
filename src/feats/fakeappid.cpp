#include "fakeappid.hpp"

#include "../config.hpp"

#include "../sdk/CProtoBufMsgBase.hpp"
#include "../sdk/CSteamEngine.hpp"
#include "../sdk/CSteamMatchmakingServers.hpp"
#include "../sdk/CUser.hpp"
#include "../sdk/IClientUtils.hpp"

uint32_t FakeAppIds::lastAppLaunched;

std::unordered_map<uint32_t, uint32_t> FakeAppIds::fakeAppIdMap = std::unordered_map<uint32_t, uint32_t>();
std::unordered_map<uint32_t, uint32_t> FakeAppIds::fakeAppIdMapServer = std::unordered_map<uint32_t, uint32_t>();
std::unordered_map<uint64_t, uint32_t> FakeAppIds::fakeAppIdMapPings = std::unordered_map<uint64_t, uint32_t>();

uint32_t FakeAppIds::getFakeAppId(uint32_t appId)
{
	auto fakeAppIds = g_config.fakeAppIds.get();

	if (fakeAppIds.contains(appId))
	{
		return fakeAppIds[appId];
	}
	else if (fakeAppIds.contains(0) && !g_pSteamEngine->getUser(0)->isSubscribed(appId))
	{
		return fakeAppIds[0];
	}

	return 0;
}

uint32_t FakeAppIds::getRealAppIdForCurrentPipe(bool fallback)
{
	if (!g_pClientUtils)
	{
		return 0;
	}

	uint32_t hPipe = *g_pClientUtils->getPipeIndex();
	if (fakeAppIdMap.contains(hPipe))
	{
		return fakeAppIdMap[hPipe];
	}

	if (fallback)
	{
		return g_pClientUtils->getAppId();
	}

	return 0;
}

void FakeAppIds::launchApp(uint32_t appId)
{
	lastAppLaunched = appId;
}

void FakeAppIds::setAppIdForCurrentPipe(uint32_t& appId)
{
	//Keep track of every AppId, for various reasons
	//fakeAppIdMap[*g_pClientUtils->getPipeIndex()] = appId;
	fakeAppIdMap[*g_pClientUtils->getPipeIndex()] = lastAppLaunched;

	g_pLog->debug("fakeAppIdMap[%p] = %u\n", *g_pClientUtils->getPipeIndex(), appId);

	//Do not change Steam Client itself (AppId 0)
	if (!appId)
	{
		return;
	}

	uint32_t newAppId = getFakeAppId(appId);
	if (newAppId)
	{
		g_pLog->once("Changing AppId of %u\n", appId);
		appId = newAppId;
	}
}

void FakeAppIds::runIPCFrame(bool post)
{
	uint32_t appId = getRealAppIdForCurrentPipe(false);
	uint32_t fakeAppId = getFakeAppId(appId);

	if (!appId || !fakeAppId || appId == fakeAppId)
	{
		return;
	}

	if (post)
	{
		appId = fakeAppId;
	}

	g_pLog->debug("Setting AppId to %u in pipe %p\n", appId, *g_pClientUtils->getPipeIndex());
	g_pSteamEngine->setAppIdForCurrentPipe(appId);
}

void FakeAppIds::getServerDetails(uint32_t handle, gameserverdetails_t& details)
{
	if (!fakeAppIdMapServer.contains(handle))
	{
		return;
	}

	const uint32_t realAppId = fakeAppIdMapServer[handle];
	fakeAppIdMapPings[*reinterpret_cast<uint64_t*>(&details.address)] = realAppId;
	details.appId = realAppId;

	g_pLog->debug("Changing appId back to %u\n", realAppId);
}

uint32_t FakeAppIds::requestInternetServerList(uint32_t appId)
{
	const uint32_t fake = getFakeAppId(appId);
	if (!fake)
	{
		return 0;
	}

	g_pLog->debug("Replacing %u with %u\n", appId, fake);
	return fake;
}

void FakeAppIds::pingResponse(gameserverdetails_t *details)
{
	if (!details)
	{
		return;
	}

	const uint64_t ip = *reinterpret_cast<uint64_t*>(&details->address);
	if (!fakeAppIdMapPings.contains(ip))
	{
		return;
	}

	details->appId = fakeAppIdMapPings[ip];
}


void FakeAppIds::sendMsg(CProtoBufMsgBase* msg)
{
	switch(msg->type)
	{
		case EMSG_GAMESPLAYED:
		case EMSG_GAMESPLAYED_NO_DATABLOB:
		case EMSG_GAMESPLAYED_WITH_DATABLOB:
			break;

		default:
			return;
	}

	const auto body = msg->getBody<CMsgClientGamesPlayed>();
	for(int i = 0; i < body->games_played_size(); i++)
	{
		const auto game = body->mutable_games_played(i);
		const uint64_t gameId = game->game_id();

		// Native non-Steam shortcut IDs use 0x02000000 in their low 32 bits.
		// Preserve the original 64-bit shortcut ID instead of applying a fake AppID.
		if ((gameId & 0xffffffffULL) == 0x02000000ULL)
		{
			continue;
		}

		const uint32_t fakeAppId = FakeAppIds::getFakeAppId(gameId);
		if (!fakeAppId)
		{
			continue;
		}

		g_pLog->debug("Setting %llu to %u\n", gameId, fakeAppId);
		game->set_game_id(fakeAppId);
	}
}
