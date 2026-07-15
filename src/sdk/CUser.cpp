#include "CUser.hpp"

#include "CUser.hpp"

#include "../config.hpp"
#include "../hooks.hpp"
#include "../patterns.hpp"


bool CUser::checkAppOwnership(const AppId_t appId, AppOwnershipInfo_t* pInfo)
{
	return Hooks::CUser_CheckAppOwnership.tramp.fn(this, appId, pInfo);
}

bool CUser::isSubscribed(const AppId_t appId)
{
	AppOwnershipInfo_t info {};
	if (!checkAppOwnership(appId, &info))
	{
		return false;
	}

	if (g_config.addedPackageIds.get().contains(info.subId))
	{
		return false;
	}

	return info.ownsLicense && !info.licenseExpired;
}

bool CUser::processLicenseList(const CMsgClientLicenseList* licenses)
{
	const static auto fn = reinterpret_cast<bool(*)(void*, const CMsgClientLicenseList*)>(Patterns::CUser::ProcessLicenseList.address);
	return fn(this, licenses);
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
