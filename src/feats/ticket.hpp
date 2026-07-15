#pragma once

#include "../sdk/steam.hpp"

#include <cstdint>
#include <map>
#include <string>


class CMsgClientGetAppOwnershipTicketResponse;
class CMsgClientRequestEncryptedAppTicketResponse;
class CNetPacket;

namespace Ticket
{
	class SavedTicket
	{
public:
		uint32_t steamId;
		std::string ticket;
	};

	extern uint32_t oneTimeSteamIdSpoof;
	extern std::map<AppId_t, SavedTicket> ticketMap;
	extern std::map<AppId_t, SavedTicket> encryptedTicketMap;

	std::string getTicketDir();

	//TODO: Fill with error checks
	std::string getTicketPath(const AppId_t appId);
	SavedTicket getCachedTicket(const AppId_t appId);
	bool saveTicketToCache(const CMsgClientGetAppOwnershipTicketResponse& resp);

	void launchApp(const AppId_t appId);
	uint32_t getTicketOwnershipExtendedData
	(
		const AppId_t appId,
		void* ticket,
		const uint32_t ticketSize,
		uint32_t* offAppId,
		uint32_t* offSteamId,
		uint32_t* offSig,
		uint32_t* sigSize
	);

	std::string getEncryptedTicketPath(const AppId_t appId);
	SavedTicket getCachedEncryptedTicket(const AppId_t appId);
	bool saveEncryptedTicketToCache(const CMsgClientRequestEncryptedAppTicketResponse& resp);

	void recvEncryptedAppTicket(CNetPacket* pkt);
	void recvAppTicket(const CNetPacket* pkt);
	void recvMsg(CNetPacket* pkt);
}
