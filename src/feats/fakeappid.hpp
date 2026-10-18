#pragma once

#include "../sdk/sdk.hpp"

#include <cstdint>
#include <unordered_map>


namespace FakeAppIds
{
	extern std::unordered_map<HSteamPipe, AppId_t> fakeAppIdMap;
	extern std::unordered_map<HServerRequest, AppId_t> fakeAppIdMapServer;
	extern std::unordered_map<uint64_t, AppId_t> fakeAppIdMapPings;

	AppId_t getFakeAppId(const AppId_t appId);
	AppId_t getRealAppIdFromEnv(const HSteamPipe pipe);
	AppId_t getRealAppIdForCurrentPipe(const bool fallback = true);
	bool shouldUseRealAppIdForInterface(const EIPCInterface type);

	//General functionality
	void closePipe(const HSteamPipe pipe);
	void setAppIdForCurrentPipe(AppId_t& appId);
	void runIPCFrame(const bool post, const EIPCInterface interface);

	//Serverbrowser
	void getServerDetails(const HServerRequest handle, gameserverdetails_t& details);
	uint32_t requestInternetServerList(const AppId_t appId);
	void pingResponse(gameserverdetails_t* details);

	void sendGamesPlayed(CNetPacket* pkt);
	void sendRichPresenceUpload(CNetPacket* pkt);
	void sendMsg(CNetPacket* pkt);
}
