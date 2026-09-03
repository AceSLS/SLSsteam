#include "misc.hpp"

#include "../config.hpp"
#include "../globals.hpp"

#include "fakeappid.hpp"


bool Misc::shouldFakeOffline()
{
	const AppId_t appId = FakeAppIds::getRealAppIdForCurrentPipe();
	if (!appId || !g_config.fakeOffline.get()->contains(appId))
	{
		return false;
	}

	LOG_ONCE("Faking offline mode for %u\n", appId);
	return true;
}


void Misc::recvMsg(CNetPacket *pkt)
{
	switch(pkt->getProtoBufType())
	{
		case k_EMsgClientPersonaState:
		{
			const auto name = g_config.fakeName.get();
			if (name->size() < 1)
			{
				return;
			}

			auto msg = pkt->deserializeBody<CMsgClientPersonaState>();

			for (int i = 0; i < msg.friends_size(); i++)
			{
				auto frnd = msg.mutable_friends(i);

				if (frnd->friendid() != g_currentSteamId.steamId64)
				{
					continue;
				}

				frnd->set_player_name(*name);
				LOG_DEBUG("Faked self persona\n");

				pkt->serialize(msg);
				break;
			}

			break;
		}

		case k_EMsgClientEmailAddrInfo:
		{
			const auto email = g_config.fakeEmail.get();
			if (email->size() < 1)
			{
				return;
			}

			auto msg = pkt->deserializeBody<CMsgClientEmailAddrInfo>();
			msg.set_email_address(*email);
			msg.set_email_is_validated(true);

			pkt->serialize(msg);
			break;
		}

		case k_EMsgClientWalletInfoUpdate:
		{
			const int32_t amount = g_config.fakeWalletBalance.copy();
			if (!amount)
			{
				return;
			}

			auto msg = pkt->deserializeBody<CMsgClientWalletInfoUpdate>();
			msg.set_has_wallet(true);
			msg.set_balance(amount);
			msg.set_balance64(amount);

			pkt->serialize(msg);
			break;
		}

		case k_EMsgClientLogOnResponse:
		{
			auto msg = pkt->deserializeBody<CMsgClientLogonResponse>();
			if (msg.has_parental_setting_signature())
			{
				msg.clear_parental_setting_signature();
				LOG_DEBUG("Cleared parental setting signature!\n");
			}
			if (msg.has_parental_settings())
			{
				msg.clear_parental_settings();
				LOG_DEBUG("Cleared parental settings!\n");
			}

			pkt->serialize(msg);
			break;
		}

		default:
			break;
	}
}
