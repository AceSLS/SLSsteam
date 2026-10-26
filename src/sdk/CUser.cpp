#include "CUser.hpp"

#include "CUser.hpp"
#include "IClientAppManager.hpp"
#include "IClientApps.hpp"
#include "IClientUser.hpp"

#include "../hooks.hpp"
#include "../patterns.hpp"

#include "libmem/libmem.h"


IClientAppManager* CUser::getAppManager()
{
	const static lm_address_t offset = MemHlp::searchOffsetByTypeName(this, "15CUserAppManager");
	return reinterpret_cast<IClientAppManager*>(this + offset);
}

IClientApps* CUser::getClientApps()
{
	const static lm_address_t offset = MemHlp::searchOffsetByTypeName(this, "12CUserAppInfo");
	return reinterpret_cast<IClientApps*>(this + offset);
}

IClientCompat* CUser::getClientCompat()
{
	const static lm_address_t offset = MemHlp::searchOffsetByTypeName(this, "14CCompatManager");
	return reinterpret_cast<IClientCompat*>(this + offset);
}

IClientUser* CUser::getClientUser()
{
	const static lm_address_t offset = MemHlp::searchOffsetByTypeName(this, "5CUser");
	return reinterpret_cast<IClientUser*>(this + offset);
}

IClientUserStats* CUser::getClientUserStats()
{
	const static lm_address_t offset = MemHlp::searchOffsetByTypeName(this, "10CUserStats");
	return reinterpret_cast<IClientUserStats*>(this + offset);
}

bool CUser::checkAppOwnership(const AppId_t appId, AppOwnershipInfo_t* pInfo)
{
	return Hooks::CUser_CheckAppOwnership->tramp.fn(this, appId, pInfo);
}

steamsize_t CUser::getSubscribedApps(AppId_t* pAppIds, steamsize_t numAppIds)
{
	return Hooks::CUser_GetSubscribedApps->tramp.fn(this, pAppIds, numAppIds, 1);
}

bool CUser::isSubscribed(const AppId_t appId)
{
	AppOwnershipInfo_t info {};
	if (!checkAppOwnership(appId, &info))
	{
		return false;
	}

	LOG_ONCE("AppOwnership for %u, owns %u, expired %u, num %u\n", appId, info.ownsLicense, info.licenseExpired, info.numLicenses);
	return info.ownsLicense && !info.licenseExpired && info.numLicenses;
}

void CUser::postCallback(const ECallbackType type, void* pCallback, const steamsize_t callbackSize)
{
	const static auto fn = reinterpret_cast<void(*)(void*, ECallbackType, void*, steamsize_t, uint32_t)>(Patterns::CUser::PostCallback.address);
	fn(this, type, pCallback, callbackSize, 0);
}

void CUser::updateAppOwnershipTicket(const AppId_t appId, void* pTicket, const steamsize_t len)
{
	const static auto fn = reinterpret_cast<void(*)(void*, uint32_t, void*, steamsize_t)>(Patterns::CUser::UpdateAppOwnershipTicket.address);
	fn(this, appId, pTicket, len);

	//Dunno if this achieves anything, but the client does it so we do too
	AppOwnershipTicketReceived_t cb;
	cb.result = k_EResultOK;
	cb.appId = appId;
	postCallback(ECallbackType::AppOwnershipTicketReceived_t, &cb, sizeof(cb));
}
