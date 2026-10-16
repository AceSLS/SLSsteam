#include "dlc.hpp"

#include "../config.hpp"

#include "apps.hpp"


bool DLC::shouldUnlockDlc(const AppId_t appId)
{
	//Don't unlock inside the SteamClient (AppId 0)
	if (!g_pSteamEngine->getUtils()->getAppId())
	{
		return false;
	}

	if (g_pSteamEngine->getUser(0)->isSubscribed(appId))
	{
		return false;
	}

	if (g_config.shouldExcludeAppId(appId))
	{
		return false;
	}

	return true;
}

bool DLC::checkAppOwnership(const AppId_t appId, AppOwnershipInfo_t *info)
{
	if (!shouldUnlockDlc(appId))
	{
		return false;
	}

	Apps::unlockApp(appId, info);

	return true;
}

bool DLC::isDlcEnabled(const AppId_t appId)
{
	return shouldUnlockDlc(appId);
}

bool DLC::isAppDlcInstalled(const AppId_t appId)
{
	return shouldUnlockDlc(appId);
}

bool DLC::userSubscribedInTicket(const AppId_t appId)
{
	//Might want to compare the steamId param to the g_currentSteamId in the future
	//Although not doing that might also work for Dedicated servers?
	return shouldUnlockDlc(appId);
}

uint32_t DLC::getDlcCount(const AppId_t appId)
{
	const auto dlcData = g_config.dlcData.get();
	if (dlcData->contains(appId))
	{
		return dlcData->at(appId).dlcIds.size();
	}

	return 0;
}

bool DLC::getDlcDataByIndex(const AppId_t appId, const uint32_t index, AppId_t* dlcId, bool* available, char* dlcName, steamsize_t& dlcNameLen)
{
	if (!dlcId || !available || !dlcName)
	{
		return false;
	}

	const auto dlcData = g_config.dlcData.get();
	if (dlcData->contains(appId))
	{
		const auto& data = dlcData->at(appId);
		const auto dlc = std::next(data.dlcIds.begin(), index);

		*dlcId = dlc->first;
		*available = true;

		//No clue if we have to check for errors during printf since the devs hopefully didn't fuck
		//up the dlcNameLen. Who knows though
		snprintf(dlcName, dlcNameLen, "%s", dlc->second.c_str());

		return true;
	}
	else if (!g_config.shouldExcludeAppId(*dlcId))
	{
		*available = true;
	}

	return false;
}
