#pragma once

#include "../sdk/sdk.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace Achievements
{
	//Legacy schemas, used by games/steam_api
	constexpr const char* GET_PLAYER_STATS_SERVICE_NAME = "Player.GetUserStats#1";
	constexpr unsigned int COOLDOWN_MINUTES = 10;

	extern std::unordered_map<AppId_t, std::chrono::time_point<std::chrono::system_clock>> fetchCooldowns;
	extern std::unordered_map<AppId_t, uint64_t> preferredOwners;
	extern std::unordered_map<AppId_t, std::unordered_set<uint64_t>> ownerBlacklist;

	std::string getReviewUrl(const AppId_t appId);
	std::unordered_set<uint64_t> getReviewersForGame(const AppId_t appId);
	void setCooldown(const AppId_t appId);

	uint32_t tryGetPlayerStats
	(
		CClientUnifiedServiceTransport* serviceTransport,
		const char* serviceName,
		CPlayer_GetUserStats_Request* send,
		CPlayer_GetUserStats_Response* recv,
		const uint64_t steamId
	);
	//CPlayer_GetUserStats
	uint32_t sendAndRecvGetPlayerStats
	(
		CClientUnifiedServiceTransport* serviceTransport,
		const char* serviceName,
		CPlayer_GetUserStats_Request* send,
		CPlayer_GetUserStats_Response* recv
	);

	uint32_t tryGetUserStats(CAPIJob* job, CProtoBufMsgBase* send, const uint32_t timeOut, CProtoBufMsgBase* recv, const EMsg targetType, const uint64_t steamId);
	//GetUserStats
	uint32_t sendAndRecvGetUserStats(CAPIJob* job, CProtoBufMsgBase* send, const uint32_t timeOut, CProtoBufMsgBase* recv, const EMsg targetType);

	//New schemas, used by the Steam UI
	constexpr const char* GET_GAME_ACHIEVEMENTS_SERVICE_NAME = "Player.GetGameAchievements#1";
	constexpr const char* GET_USER_ACHIEVEMENTS_SERVICE_NAME = "Player.GetUserAchievements#1";

	extern std::unordered_map<long, AppId_t> jobToAppIdMap;
	extern std::unordered_map<AppId_t, CPlayer_GetGameAchievements_Response> gameAchievements;
	extern std::unordered_map<AppId_t, std::vector<uint8_t>> userAchievements;

	bool recvMsg(CNetPacket* pkt);
	void sendMsg(CNetPacket* pkt);
}
