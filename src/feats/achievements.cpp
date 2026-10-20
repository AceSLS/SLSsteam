#include "achievements.hpp"

#include "../curl.hpp"
#include "../config.hpp"
#include "../log.hpp"

#include <cstdint>
#include <cstring>
#include <regex>
#include <sstream>
#include <string>


std::unordered_map<AppId_t, std::chrono::time_point<std::chrono::system_clock>> Achievements::fetchCooldowns = std::unordered_map<AppId_t, std::chrono::time_point<std::chrono::system_clock>>();
std::unordered_map<AppId_t, uint64_t> Achievements::preferredOwners = std::unordered_map<AppId_t, uint64_t>();
std::unordered_map<AppId_t, std::unordered_set<uint64_t>> Achievements::ownerBlacklist = std::unordered_map<AppId_t, std::unordered_set<uint64_t>>();

std::string Achievements::getReviewUrl(const AppId_t appId)
{
	std::ostringstream url;

	url << "https://api.steampowered.com/IUserReviewsService/GetAppReviews/v1/"
		<< "?appid=" << appId
		<< "&num_per_page=" << g_config.maxSchemaTries.copy()
		<< "&filter=1" //k_EUserReviewsAppReviewsFilter_Recent
		<< "&languages\\[0\\]=all" //Escape brackets, otherwise curl exits with "URL malformed. The syntax was not correct"
		<< "&day_range=0"
		<< "&review_type=0" //k_EUserReviewsReviewType_All
		<< "&purchase_type=1" //k_EUserReviewsPurchaseType_All
		<< "&filter_offtopic_activity=false"
		<< "&use_review_quality=false";

	return url.str();
}

std::unordered_set<uint64_t> Achievements::getReviewersForGame(const AppId_t appId)
{
	auto list = std::unordered_set<uint64_t>();
	if (!g_config.maxSchemaTries.copy())
	{
		return list;
	}

	if (fetchCooldowns.contains(appId))
	{
		const auto now = std::chrono::system_clock::now();
		if (now < fetchCooldowns.at(appId))
		{
			return list;
		}

		fetchCooldowns.erase(appId);
	}

	auto url = getReviewUrl(appId);
	std::string reviews;

	if (Curl::downloadString(url.c_str(), reviews))
	{
		LOG_WARN("Failed to get reviewer list for %u!\n", appId);
		return list;
	}

	if (!ownerBlacklist.contains(appId))
	{
		ownerBlacklist[appId] = std::unordered_set<uint64_t>();
	}

	//LOG_DEBUG("Downloaded reviewers %s\n", reviews.c_str());

	std::regex steamIdFieldsRe("\"steamid\":\"[0-9]+\"");

	auto begin = std::sregex_iterator(reviews.begin(), reviews.end(), steamIdFieldsRe);
	auto end = std::sregex_iterator();

	for (auto i = begin; i != end; ++i)
	{
		std::smatch steamIdMatch = *i;
		std::string steamIdFieldStr = steamIdMatch.str();

		//LOG_DEBUG("SteamId match %s\n", match.str().c_str());

		std::regex idRe("[0-9]+");
		if (!std::regex_search(steamIdFieldStr, steamIdMatch, idRe))
		{
			continue;
		}

		LOG_DEBUG("Extracted SteamId %s\n", steamIdMatch.str().c_str());

		uint64_t steamId = std::stoull(steamIdMatch.str().c_str());
		if (ownerBlacklist[appId].contains(steamId))
		{
			LOG_DEBUG("Skipping %lu for %u because it has failed before\n", steamId, appId);
			continue;
		}

		list.emplace(steamId);
	}

	return list;
}

void Achievements::setCooldown(const AppId_t appId)
{
	if (fetchCooldowns.contains(appId))
	{
		return;
	}

	fetchCooldowns[appId] = std::chrono::system_clock::now() + std::chrono::minutes(COOLDOWN_MINUTES);
	LOG_DEBUG("Set cooldown for %u for %u minutes\n", appId, COOLDOWN_MINUTES);
}

bool Achievements::tryGetPlayerStats
(
	CClientUnifiedServiceTransport* serviceTransport,
	const char* serviceName,
	CPlayer_GetUserStats_Request* send,
	CPlayer_GetUserStats_Response* recv,
	const uint64_t steamId
)
{
	LOG_DEBUG("CPlayer_GetUserStats_Request->set_steamid(%lu)\n", steamId);
	send->set_steamid(steamId);
	const bool res = serviceTransport->sendAndRecvMsg(serviceName, send, recv);

	if (!res)
	{
		return false;
	}

	recv->clear_crc_stats();
	recv->clear_stats();

	const AppId_t appId = send->appid();

	preferredOwners[appId] = steamId;
	LOG_DEBUG("Using steamId %lu for %u\n", steamId, appId);
	return true;
}

bool Achievements::sendAndRecvGetPlayerStats
(
	CClientUnifiedServiceTransport* serviceTransport,
	const char* serviceName,
	CPlayer_GetUserStats_Request* send,
	CPlayer_GetUserStats_Response* recv
)
{
	if (strcmp(serviceName, GET_PLAYER_STATS_SERVICE_NAME) != 0)
	{
		return false;
	}

	//Don't do anything for legit apps
	if (g_pSteamEngine->getUser(0)->isSubscribed(send->appid()))
	{
		return false;
	}

	const AppId_t appId = send->appid();
	send->clear_crc_stats();

	//Prefer last successfull owner to skip review fetch. Fixes stutters caused
	//by review fetching in games that spam stat requests
	if (preferredOwners.contains(appId))
	{
		const bool res = tryGetPlayerStats(serviceTransport, serviceName, send, recv, preferredOwners.at(appId));
		if (res)
		{
			return true;
		}

		preferredOwners.erase(send->appid());
	}

	const auto reviewers = getReviewersForGame(send->appid());

	for (const auto& id : reviewers)
	{
		const bool res = tryGetPlayerStats(serviceTransport, serviceName, send, recv, id);
		if (res)
		{
			return true;
		}
	}

	setCooldown(appId);
	LOG_DEBUG("No schemas for %u found! Falling back to offline cache\n", appId);
	return true;
}

uint32_t Achievements::tryGetUserStats(CAPIJob* job, CProtoBufMsgBase* send, const uint32_t timeOut, CProtoBufMsgBase* recv, const EMsg targetType, const uint64_t steamId)
{
	const auto sendBdy = send->getBody<CMsgClientGetUserStats>();

	LOG_DEBUG("CMsgClientGetUserStats->set_steam_id_for_user(%lu)\n", steamId);
	sendBdy->set_steam_id_for_user(steamId);

	const uint32_t ret = job->sendAndRecv(send, timeOut, recv, targetType);
	if (!ret)
	{
		return 0;
	}

	const AppId_t appId = sendBdy->game_id();
	const auto recvBdy = recv->getBody<CMsgClientGetUserStatsResponse>();

	if (recvBdy->eresult() == k_EResultFailure)
	{
		ownerBlacklist[appId].emplace(steamId);
		return 0;
	}

	if (recvBdy->eresult() != k_EResultOK)
	{
		return 0;
	}

	recvBdy->clear_achievement_blocks();
	recvBdy->clear_crc_stats();
	recvBdy->clear_stats();

	LOG_DEBUG("Using steamId %lu for %u\n", steamId, appId);
	preferredOwners[appId] = steamId;
	return ret;
}

uint32_t Achievements::sendAndRecvGetUserStats(CAPIJob* job, CProtoBufMsgBase* send, const uint32_t timeOut, CProtoBufMsgBase* recv, const EMsg targetType)
{
	if (targetType != k_EMsgClientGetUserStatsResponse)
	{
		return 0;
	}

	const auto sendBdy = send->getBody<CMsgClientGetUserStats>();

	if (g_pSteamEngine->getUser(0)->isSubscribed(sendBdy->game_id()))
	{
		return 0;
	}

	const AppId_t appId = sendBdy->game_id();
	sendBdy->clear_crc_stats();

	if (preferredOwners.contains(appId))
	{
		uint32_t ret = tryGetUserStats(job, send, timeOut, recv, targetType, preferredOwners.at(appId));
		if (ret)
		{
			return ret;
		}

		//Prefered owner failed, erase then fallthrough to trying reviewers
		preferredOwners.erase(appId);
	}

	const auto reviewers = getReviewersForGame(sendBdy->game_id());

	for (const auto& id : reviewers)
	{
		const uint32_t ret = tryGetUserStats(job, send, timeOut, recv, targetType, id);
		if (ret)
		{
			return ret;
		}
	}

	const auto recvBdy = recv->getBody<CMsgClientGetUserStatsResponse>();
	recvBdy->set_eresult(k_EResultNoConnection);
	setCooldown(appId);

	LOG_DEBUG("No schemas for %u found! Falling back to offline cache\n", appId);
	return 1;
}

std::unordered_map<long, AppId_t> Achievements::jobToAppIdMap = std::unordered_map<long, AppId_t>();
std::unordered_map<AppId_t, CPlayer_GetGameAchievements_Response> Achievements::gameAchievements = std::unordered_map<AppId_t, CPlayer_GetGameAchievements_Response>();
std::unordered_map<AppId_t, std::vector<uint8_t>> Achievements::userAchievements = std::unordered_map<AppId_t, std::vector<uint8_t>>();

bool Achievements::recvMsg(CNetPacket* pkt)
{
	if (pkt->getProtoBufType() != k_EMsgServiceMethodResponse)
	{
		return false;
	}

	const auto hdr = pkt->deserializeHeader();
	if (!hdr.has_target_job_name() || !hdr.has_jobid_target())
	{
		return false;
	}

	const long jobId = hdr.jobid_target();
	LOG_DEBUG("Received %s %lu\n", hdr.target_job_name().c_str(), hdr.jobid_target());

	if (!jobToAppIdMap.contains(jobId))
	{
		return false;
	}

	const AppId_t appId = jobToAppIdMap.at(jobId);

	if (hdr.target_job_name() == GET_GAME_ACHIEVEMENTS_SERVICE_NAME)
	{
		gameAchievements[appId] = pkt->deserializeBody<CPlayer_GetGameAchievements_Response>();

		//Queued user achievements, inject now
		if (userAchievements.contains(appId))
		{
			const auto& bytes = userAchievements.at(appId);

			CNetPacket statPkt { };
			statPkt.refs = 2;
			statPkt.body = reinterpret_cast<CNetPacketBody*>(Steam::Plat_Alloc(bytes.size()));
			statPkt.originalBody = statPkt.body;
			statPkt.size = bytes.size();

			if (!statPkt.body)
			{
				LOG_ERROR("Failed to allocate fake packet for stat injection for %u!\n", appId);
				return false;
			}

			memcpy(statPkt.body, bytes.data(), bytes.size());
			g_pCMInterface->recvPktHk(&statPkt);

			statPkt.free();
			LOG_DEBUG("Injected fake %s for %u\n", GET_USER_ACHIEVEMENTS_SERVICE_NAME, appId);
		}
	}
	else if (hdr.target_job_name() == GET_USER_ACHIEVEMENTS_SERVICE_NAME)
	{
		//We can't fill in data we don't have yet. Choke the packet & reinject later
		if (!gameAchievements.contains(appId))
		{
			auto& bytes = userAchievements[appId];
			bytes.resize(pkt->size);
			memcpy(bytes.data(), pkt->body, pkt->size);

			LOG_DEBUG("Failed to fill achievements for %u! Missing Game achievements. Queueing...\n", appId);

			return true; //Choke packet
		}

		auto msg = pkt->deserializeBody<CPlayer_GetUserAchievements_Response>();

		const auto& gameAch = gameAchievements.at(appId);

		msg.clear_achievements();
		msg.clear_groups();

		const auto stats = g_pSteamEngine->getUser()->getClientUserStats();

		for (const auto& ach : gameAch.achievements())
		{
			auto newAch = msg.add_achievements();
			newAch->set_internal_key(ach.internal_key());

			bool unlocked;
			uint32_t time;
			stats->getAchievement(appId, ach.internal_name().c_str(), &unlocked, &time);

			float minProgress;
			float curProgress;
			float maxProgress;
			stats->getAchievementProgress(appId, ach.internal_name().c_str(), &minProgress, &curProgress, &maxProgress);

			LOG_DEBUG("Achievement %s %s, progress %f %f %f\n", ach.internal_name().c_str(), unlocked ? "unlocked" : "locked", minProgress, curProgress, maxProgress);

			if (unlocked)
			{
				newAch->set_unlocked(unlocked);
				newAch->set_unlock_time(time);
			}

			newAch->set_progress_float(curProgress);
		}

		for (const auto& grp : gameAch.groups())
		{
			auto newGrp = msg.add_groups();
			newGrp->set_groupid(grp.groupid());
			newGrp->set_is_achievable(true);
		}

		pkt->serialize(msg);
		LOG_DEBUG("Injected achievements for %u\n", appId);
	}

	return false;
}

void Achievements::sendMsg(CNetPacket* pkt)
{
	if (pkt->getProtoBufType() != k_EMsgServiceMethodCallFromClient)
	{
		return;
	}

	const auto hdr = pkt->deserializeHeader();
	if (!hdr.has_target_job_name() || !hdr.has_jobid_source())
	{
		return;
	}

	const long jobId = hdr.jobid_source();

	LOG_DEBUG("Sending %s (%lu)\n", hdr.target_job_name().c_str(), hdr.jobid_source());

	//Store appId for later use
	if (hdr.target_job_name() == GET_GAME_ACHIEVEMENTS_SERVICE_NAME)
	{
		const auto msg = pkt->deserializeBody<CPlayer_GetGameAchievements_Request>();
		jobToAppIdMap[jobId] = msg.appid();
	}
	else if (hdr.target_job_name() == GET_USER_ACHIEVEMENTS_SERVICE_NAME)
	{
		const auto msg = pkt->deserializeBody<CPlayer_GetUserAchievements_Request>();
		const AppId_t appId = msg.appid();

		//Only modify unowned games
		if (!g_pSteamEngine->getUser()->isSubscribed(appId))
		{
			jobToAppIdMap[jobId] = msg.appid();
			LOG_DEBUG("AppId %u is unowned! Injecting achievements\n", appId);
		}
	}
}
