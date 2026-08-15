#pragma once

#include "../sdk/sdk.hpp"

#include <cstdint>
#include <mutex>
#include <unordered_set>


namespace Apps
{
	extern bool applistRequested;
	extern std::unordered_set<AppId_t> privateApps;

	extern std::mutex pendingLicenseChangesMutex;
	extern std::unordered_set<AppId_t> pendingLicenseChanges;

	bool updateRemoteApps(const std::unordered_set<AppId_t>& appIds);
	bool isRemoteApp(const AppId_t appId);
	void requestRemoteAppBroadcast();
	uint64_t getRemoteAppBroadcastGeneration();

	bool unlockApp(const AppId_t appId, AppOwnershipInfo_t* info, const CSteamId& ownerId);
	bool unlockApp(const AppId_t appId, AppOwnershipInfo_t* info);

	void buildDepotDependency(CUtlVector<DepotInfo_t>* depots, CUtlVector<DepotInfo_t>* sharedDepots);
	bool checkAppOwnership(const AppId_t appId, AppOwnershipInfo_t* info);
	void getLegacyCDKey(const AppId_t appId);
	void getSubscribedApps(AppId_t* appList, const uint32_t size, uint32_t& count);
	void parseProductInfoFromResponse(CMsgClientPICSProductInfoResponse* msg);
	void runIPCFrame();

	void postAppLicensesChanged(const std::unordered_set<AppId_t>& apps, const bool appsAdded = true);

	bool shouldDisableCloud(const AppId_t appId);
	bool shouldDisableCDKey(const AppId_t appId);
	bool shouldDisableUpdates(const AppId_t appId);

	void sendAndRecvLastPlayedTimes(const char* name, CPlayer_GetLastPlayedTimes_Response* recv);
	void sendGamesPlayed(CNetPacket* pkt);
	void sendPICSInfoRequest(CNetPacket* pkt);
	void sendMsg(CNetPacket* pkt);

	void setConfigStoreString(const char* key, const char* value);
};
