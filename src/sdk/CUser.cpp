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

bool CUser::checkAppOwnership(const AppId_t appId, AppOwnershipInfo_t* pInfo)
{
	return Hooks::CUser_CheckAppOwnership->tramp.fn(this, appId, pInfo);
}

bool CUser::isSubscribed(const AppId_t appId)
{
	AppOwnershipInfo_t info {};
	if (!checkAppOwnership(appId, &info))
	{
		return false;
	}

	return info.ownsLicense && !info.licenseExpired;
}

void CUser::postCallback(const ECallbackType type, void* pCallback, const uint32_t callbackSize)
{
	const static auto fn = reinterpret_cast<void(*)(void*, ECallbackType, void*, uint32_t, uint32_t)>(Patterns::CUser::PostCallback.address);
	fn(this, type, pCallback, callbackSize, 0);
}

void CUser::updateAppOwnershipTicket(const AppId_t appId, void* pTicket, const uint32_t len)
{
	const static auto fn = reinterpret_cast<void(*)(void*, uint32_t, void*, uint32_t)>(Patterns::CUser::UpdateAppOwnershipTicket.address);
	fn(this, appId, pTicket, len);

	//Dunno if this achieves anything, but the client does it so we do too
	AppOwnershipTicketReceived_t cb;
	cb.result = k_EResultOK;
	cb.appId = appId;
	postCallback(ECallbackType::AppOwnershipTicketReceived_t, &cb, sizeof(cb));
}
