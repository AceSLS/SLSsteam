#include "stealth.hpp"


bool Stealth::recvMsg(CNetPacket* pkt)
{
	switch (pkt->getProtoBufType())
	{
		default:
			return false;
	}
}

uint32_t Stealth::sendAndRecvStoreUserStats2(CProtoBufMsgBase* send, CProtoBufMsgBase* recv, EMsg target)
{
	if (target != k_EMsgClientStoreUserStatsResponse)
	{
		return 0;
	}

	const auto sendBdy = send->getBody<CMsgClientStoreUserStats2>();
	if (g_pSteamEngine->getUser()->isSubscribed(sendBdy->game_id()))
	{
		return 0;
	}

	const auto recvBdy = recv->getBody<CMsgClientStoreUserStatsResponse>();

	recvBdy->set_crc_stats(sendBdy->crc_stats());
	recvBdy->set_eresult(k_EResultOK);
	recvBdy->set_game_id(sendBdy->game_id());
	recvBdy->set_stats_out_of_date(false);

	LOG_DEBUG("Fake successfull stat upload for %lu\n", sendBdy->game_id());

	return 769;
}

bool Stealth::sendMsg(CNetPacket* pkt)
{
	bool choke = false;

	switch (pkt->getProtoBufType())
	{
		case k_EMsgClientGamesPlayed:
		case k_EMsgClientGamesPlayedNoDataBlob:
		case k_EMsgClientGamesPlayedWithDataBlob:
		{
			auto msg = pkt->deserializeBody<CMsgClientGamesPlayed>();
			for (int i = msg.games_played_size() - 1; i >= 0; i--)
			{
				const auto game = msg.mutable_games_played(i);
				if (game->game_id() & GAME_TYPE_SHORTCUT)
				{
					continue;
				}

				if (g_pSteamEngine->getUser()->isSubscribed(game->game_id()))
				{
					continue;
				}

				LOG_DEBUG("Cleared %lu from CMsgClientGamesPlayed!\n", game->game_id());
				msg.mutable_games_played()->DeleteSubrange(i, 1);
			}

			break;
		}

		case k_EMsgClientGetAppOwnershipTicket:
		{
			const auto msg = pkt->deserializeBody<CMsgClientGetAppOwnershipTicket>();

			if (!g_pSteamEngine->getUser()->isSubscribed(msg.app_id()))
			{
				AppOwnershipTicketReceived_t cb { };
				cb.appId = msg.app_id();
				cb.result = k_EResultOK;
				g_pSteamEngine->getUser()->postCallback(ECallbackType::AppOwnershipTicketReceived_t, &cb, sizeof(cb));

				choke = true;
			}

			break;
		}

		default:
			break;
	}

	if (choke)
	{
		LOG_DEBUG("Stealth choking %s\n", pkt->getProtoBufTypeName().c_str());
	}

	return choke;
}
