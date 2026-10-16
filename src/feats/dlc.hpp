#pragma once

#include "../sdk/sdk.hpp"

#include <cstddef>
#include <cstdint>


namespace DLC
{
	bool shouldUnlockDlc(const AppId_t appId);

	bool checkAppOwnership(const AppId_t appId, AppOwnershipInfo_t* info);
	bool isDlcEnabled(const AppId_t appId);
	bool isAppDlcInstalled(const AppId_t appId);
	bool userSubscribedInTicket(const AppId_t appId);

	uint32_t getDlcCount(const AppId_t appId);
	bool getDlcDataByIndex(const AppId_t appId, const uint32_t index, AppId_t* dlcId, bool* available, char* dlcName, steamsize_t& dlcNameLen);
}
