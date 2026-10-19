#include "fakeappid.hpp"

#include "../config.hpp"
#include "../process.hpp"



std::unordered_map<HSteamPipe, AppId_t> FakeAppIds::fakeAppIdMap = std::unordered_map<HSteamPipe, AppId_t>();
std::unordered_map<HServerRequest, AppId_t> FakeAppIds::fakeAppIdMapServer = std::unordered_map<HServerRequest, AppId_t>();
std::unordered_map<uint64_t, AppId_t> FakeAppIds::fakeAppIdMapPings = std::unordered_map<uint64_t, AppId_t>();

AppId_t FakeAppIds::getFakeAppId(const AppId_t appId)
{
	const auto fakeAppIds = g_config.fakeAppIds.get();

	if (fakeAppIds->contains(appId))
	{
		return fakeAppIds->at(appId);
	}
	else if (fakeAppIds->contains(0) && !g_pSteamEngine->getUser(0)->isSubscribed(appId))
	{
		return fakeAppIds->at(0);
	}

	return 0;
}

AppId_t FakeAppIds::getRealAppIdForCurrentPipe(const bool fallback)
{
	const auto utils = g_pSteamEngine->getUtils();
	if (!utils)
	{
		return 0;
	}

	if (fakeAppIdMap.contains(utils->getCurrentSteamPipe()))
	{
		return fakeAppIdMap.at(utils->getCurrentSteamPipe());
	}

	if (fallback)
	{
		return utils->getAppId();
	}

	return 0;
}

bool FakeAppIds::shouldUseRealAppIdForInterface(const EIPCInterface type)
{
	switch(type)
	{
		//case EIPCInterface::User:
		//case EIPCInterface::GameServerInternal:
		//case EIPCInterface::Friends:
		case EIPCInterface::Utils:
		case EIPCInterface::Billing:
		//case EIPCInterface::Matchmaking:
		case EIPCInterface::Apps:
		case EIPCInterface::UserStats:
		//case EIPCInterface::Networking:
		case EIPCInterface::RemoteStorage:
		case EIPCInterface::DepotBuilder:
		case EIPCInterface::AppManager:
		case EIPCInterface::ConfigStore:
		//case EIPCInterface::GameCoordinator:
		//case EIPCInterface::GameServerStats:
		case EIPCInterface::GameStats:
		case EIPCInterface::HTTP:
		case EIPCInterface::Screenshots:
		case EIPCInterface::Audio:
		case EIPCInterface::UnifiedMessages:
		case EIPCInterface::StreamLauncher:
		case EIPCInterface::ParentalSettings:
		case EIPCInterface::NetworkDeviceManager:
		case EIPCInterface::Music:
		case EIPCInterface::RemoteClientManager:
		case EIPCInterface::UGC:
		case EIPCInterface::StreamClient:
		case EIPCInterface::ProductBuilder:
		case EIPCInterface::Shortcuts:
		case EIPCInterface::GameNotifications:
		case EIPCInterface::Video:
		case EIPCInterface::Inventory:
		case EIPCInterface::VR:
		case EIPCInterface::ControllerSerialized:
		case EIPCInterface::AppDisableUpdate:
		case EIPCInterface::SharedConnection:
		case EIPCInterface::Shader:
		//case EIPCInterface::NetworkingSocketsSerialized:
		case EIPCInterface::Compat:
		case EIPCInterface::Parties:
		//case EIPCInterface::NetworkingUtilsSerialized:
		case EIPCInterface::RemotePlay:
		//case EIPCInterface::GameServerPacketHandler:
		case EIPCInterface::SystemManager:
		case EIPCInterface::SystemPerfManager:
		case EIPCInterface::SystemDockManager:
		case EIPCInterface::SystemAudioManager:
		case EIPCInterface::SystemDisplayManager:
		case EIPCInterface::Timeline:
			return true;

		default:
			return false;
	}
}

void FakeAppIds::closePipe(const HSteamPipe pipe)
{
	if (fakeAppIdMap.contains(pipe))
	{
		fakeAppIdMap.erase(pipe);
	}
}

void FakeAppIds::setAppIdForCurrentPipe(AppId_t& appId)
{
	fakeAppIdMap[g_pSteamEngine->getUtils()->getCurrentSteamPipe()] = appId;

	//Do not change Steam Client itself (AppId 0)
	if (!appId)
	{
		return;
	}

	const AppId_t newAppId = getFakeAppId(appId);
	if (newAppId)
	{
		LOG_DEBUG("Changing AppId of %u\n", appId);
		//appId = newAppId;
	}
}

void FakeAppIds::runIPCFrame(const bool post, const EIPCInterface interface)
{
	if (!shouldUseRealAppIdForInterface(interface))
	{
		return;
	}

	AppId_t appId = getRealAppIdForCurrentPipe(false);
	const AppId_t fakeAppId = getFakeAppId(appId);

	if (!appId || !fakeAppId || appId == fakeAppId)
	{
		return;
	}

	if (post)
	{
		appId = fakeAppId;
	}

	if (g_config.extendedLogging.copy())
	{
		const auto utils = g_pSteamEngine->getUtils();
		LOG_DEBUG("Setting AppId to %u in pipe 0x%x\n", appId, utils ? utils->getCurrentSteamPipe() : 0);
	}

	g_pSteamEngine->setAppIdForCurrentPipe(appId);
}

void FakeAppIds::getServerDetails(const HServerRequest handle, gameserverdetails_t& details)
{
	if (!fakeAppIdMapServer.contains(handle))
	{
		return;
	}

	const AppId_t realAppId = fakeAppIdMapServer.at(handle);
	fakeAppIdMapPings[details.ip64] = realAppId;
	details.appId = realAppId;

	LOG_DEBUG("Changing appId back to %u\n", realAppId);
}

uint32_t FakeAppIds::requestInternetServerList(const AppId_t appId)
{
	const AppId_t fake = getFakeAppId(appId);
	if (!fake)
	{
		return 0;
	}

	LOG_DEBUG("Replacing %u with %u\n", appId, fake);
	return fake;
}

void FakeAppIds::pingResponse(gameserverdetails_t *details)
{
	if (!details)
	{
		return;
	}

	const uint64_t ip = details->ip64;
	if (!fakeAppIdMapPings.contains(ip))
	{
		return;
	}

	details->appId = fakeAppIdMapPings.at(ip);
}

void FakeAppIds::sendGamesPlayed(CNetPacket *pkt)
{
	auto msg = pkt->deserializeBody<CMsgClientGamesPlayed>();

	for (int i = 0; i < msg.games_played_size(); i++)
	{
		const auto game = msg.mutable_games_played(i);
		const uint64_t gameId = game->game_id();

		if (gameId & GAME_TYPE_SHORTCUT)
		{
			continue;
		}

		const AppId_t fakeAppId = FakeAppIds::getFakeAppId(gameId);
		if (!fakeAppId)
		{
			continue;
		}

		LOG_DEBUG("Setting %lu to %u\n", gameId, fakeAppId);
		game->set_game_id(fakeAppId);
	}

	pkt->serialize(msg);
}

void FakeAppIds::sendRichPresenceUpload(CNetPacket* pkt)
{
	auto header = pkt->deserializeHeader();
	LOG_DEBUG("Routing appId %u\n", header.routing_appid());

	const auto appId = getFakeAppId(header.routing_appid());

	if (!appId)
	{
		return;
	}

	//This won't fix localized rich presences, but it's better than nothing
	header.set_routing_appid(appId);

	auto msg = pkt->deserializeBody<CMsgClientRichPresenceUpload>();
	pkt->serialize(msg, &header);
}

void FakeAppIds::sendMsg(CNetPacket* pkt)
{
	switch(pkt->getProtoBufType())
	{
		case k_EMsgClientGamesPlayed:
		case k_EMsgClientGamesPlayedNoDataBlob:
		case k_EMsgClientGamesPlayedWithDataBlob:
			sendGamesPlayed(pkt);
			break;
		
		case k_EMsgClientRichPresenceUpload:
			sendRichPresenceUpload(pkt);
			break;

		default:
			break;
	}
}
