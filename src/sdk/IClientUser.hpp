#pragma once

#include "types.hpp"

#include <cstdint>


SDK_Class IClientUser
{
public:

	bool loggedOn();
	uint32_t getAppOwnershipTicketExtendeData
	(
		const AppId_t appId,
		void* pTicket,
		const uint32_t ticketSize,
		uint32_t* pOffAppId,
		uint32_t* pOffSteamId,
		uint32_t* pOffSig,
		uint32_t* pSigSize
	);

	CSteamId getSteamId();
	bool setLegacyCDKey(const AppId_t appId, const char* key);
};
