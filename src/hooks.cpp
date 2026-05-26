#include "hooks.hpp"

#include "config.hpp"
#include "globals.hpp"
#include "api.hpp"
#include "log.hpp"
#include "memhlp.hpp"
#include "patterns.hpp"
#include "vftableinfo.hpp"

#include "sdk/CAppOwnershipInfo.hpp"
#include "sdk/CProtoBufMsgBase.hpp"
#include "sdk/CSteamEngine.hpp"
#include "sdk/CSteamMatchmakingServers.hpp"
#include "sdk/CUser.hpp"
#include "sdk/EResult.hpp"
#include "sdk/IClientAppManager.hpp"
#include "sdk/IClientApps.hpp"
#include "sdk/IClientUtils.hpp"

#include "feats/achievements.hpp"
#include "feats/apps.hpp"
#include "feats/dlc.hpp"
#include "feats/misc.hpp"
#include "feats/fakeappid.hpp"
#include "feats/ticket.hpp"

#include "libmem/libmem.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <pthread.h>
#include <strings.h>
#include <unistd.h>
#include <vector>


template<typename T>
Hook<T>::Hook(const char* name)
{
	this->name = std::string(name);
}

template<typename T>
DetourHook<T>::DetourHook(const char* name) : Hook<T>::Hook(name)
{
	this->size = 0;
}

//TODO: Fix this ungodly mess
template<typename T>
DetourHook<T>::DetourHook() : DetourHook<T>("")
{

}

template<typename T>
VFTHook<T>::VFTHook(const char* name) : Hook<T>::Hook(name)
{
	this->hooked = false;
}

template<typename T>
bool DetourHook<T>::setup(Pattern_t pattern, T hookFn)
{
	if (pattern.address == LM_ADDRESS_BAD)
	{
		return false;
	}

	this->name = pattern.name;
	this->originalFn.address = pattern.address;
	this->hookFn.fn = hookFn;

	return true;
}

template<typename T>
void DetourHook<T>::place()
{
	this->size = LM_HookCode(this->originalFn.address, this->hookFn.address, &this->tramp.address);
	MemHlp::fixPICThunkCall(this->name.c_str(), this->originalFn.address, this->tramp.address);

	g_pLog->debug
	(
		"Detour hooked %s (%p) with hook at %p and tramp at %p\n",
		this->name.c_str(),
		this->originalFn.address,
		this->hookFn.address,
		this->tramp.address
	);
}

template<typename T>
void DetourHook<T>::remove()
{
	if (!this->size)
	{
		return;
	}

	LM_UnhookCode(this->originalFn.address, this->tramp.address, this->size);
	this->size = 0;

	g_pLog->debug("Unhooked %s\n", this->name.c_str());
}

template<typename T>
void VFTHook<T>::place()
{
	LM_VmtHook(this->vft.get(), this->index, this->hookFn.address);
	this->hooked = true;

	g_pLog->debug
	(
		"VFT hooked %s (%p) with hook at %p\n",
		this->name.c_str(),
		this->originalFn.address,
		this->hookFn.address
	);
}

template<typename T>
void VFTHook<T>::remove()
{
	//No clue how libmem reacts when unhooking a non existent hook
	//so we do this
	if (!this->hooked)
	{
		return;
	}

	LM_VmtUnhook(this->vft.get(), this->index);
	this->hooked = false;

	g_pLog->debug("Unhooked %s!\n", this->name.c_str());
}

template<typename T>
void VFTHook<T>::setup(std::shared_ptr<lm_vmt_t> vft, unsigned int index, T hookFn)
{
	this->vft = vft;
	this->index = index;

	this->originalFn.address = LM_VmtGetOriginal(this->vft.get(), this->index);
	this->hookFn.fn = hookFn;
}

__attribute__((hot))
static void hkTraceIPC(const char* iface, const char* fn)
{
	Hooks::TraceIPC.tramp.fn(iface, fn);

	if (g_config.extendedLogging.get())
	{
		g_pLog->debug
		(
			"%s(%s, %s)\n",

			Hooks::TraceIPC.name.c_str(),
			iface,
			fn
		);
	}
}

static uint32_t hkCAPIJob_GetPlayerStats(void* pAPIJob)
{
	uint32_t res = Hooks::CAPIJob_GetPlayerStats.tramp.fn(pAPIJob);

	g_pLog->debug
	(
		"%s(%p) -> %i\n",
		Hooks::CAPIJob_GetPlayerStats.name.c_str(),
		pAPIJob,
		res
	);

	Achievements::getPlayerStats(res);

	return res;
}

static void hkProtoBufMsgBase_InitFromPacket(CProtoBufMsgBase* pMsg, void* pSrc)
{
	Hooks::CProtoBufMsgBase_InitFromPacket.tramp.fn(pMsg, pSrc);

	//Safety first
	if (!pSrc)
	{
		return;
	}

	g_pLog->debug("Received ProtoBufMsg of type %u with type %s\n", pMsg->type, MemHlp::getTypeName(pMsg));

	Achievements::recvMessage(pMsg);
	Misc::recvMsg(pMsg);
	Ticket::recvMsg(pMsg);
}

static uint32_t hkProtoBufMsgBase_Send(CProtoBufMsgBase* pMsg)
{
	Apps::sendMsg(pMsg);
	FakeAppIds::sendMsg(pMsg);

	const uint32_t ret = Hooks::CProtoBufMsgBase_Send.tramp.fn(pMsg);
	g_pLog->debug("Sending ProtoBufMsg of type %u with type %s\n", pMsg->type, MemHlp::getTypeName(pMsg));

	return ret;
}

static void hkSteamEngine_Init(void* pSteamEngine)
{
	Hooks::CSteamEngine_Init.tramp.fn(pSteamEngine);

	g_pSteamEngine = reinterpret_cast<CSteamEngine*>(pSteamEngine);
	g_pLog->once("g_pSteamEngine at %p\n", pSteamEngine);
}

static uint32_t hkSteamEngine_SetAppIdForCurrentPipe(void* pSteamEngine, uint32_t appId, bool a2)
{
	FakeAppIds::setAppIdForCurrentPipe(appId);

	const uint32_t ret = Hooks::CSteamEngine_SetAppIdForCurrentPipe.tramp.fn(pSteamEngine, appId, a2);

	g_pLog->debug
	(
		"%s(%p, %u, %i) -> %i\n",

		Hooks::CSteamEngine_SetAppIdForCurrentPipe.name.c_str(),
		pSteamEngine,
		appId,
		a2,
		ret
	);

	return ret;
}

static gameserverdetails_t* hkSteamMatchmakingServers_GetServerDetails(void* pSteamMatchmakingServers, uint32_t handle, uint32_t serverIdx)
{
	gameserverdetails_t* ret = Hooks::CSteamMatchmakingServers_GetServerDetails.tramp.fn(pSteamMatchmakingServers, handle, serverIdx);

	g_pLog->debug
	(
		"%s(%p, %p, %u) -> %p\n",

		Hooks::CSteamMatchmakingServers_GetServerDetails.name.c_str(),
		pSteamMatchmakingServers,
		handle,
		serverIdx,
		ret
	);

	if(ret)
	{
		FakeAppIds::getServerDetails(handle, *ret);
	}

	return ret;
}

static uint32_t hkSteamMatchmakingServers_RequestInternetServerList(void* pSteamMatchmakingServers, uint32_t appId, uint32_t a2, uint32_t a3, uint32_t a4)
{
	const uint32_t fake = FakeAppIds::requestInternetServerList(appId);

	uint32_t handle = Hooks::CSteamMatchmakingServers_RequestInternetServerList.tramp.fn(pSteamMatchmakingServers, fake ? fake : appId, a2, a3, a4);

	g_pLog->debug
	(
		"%s(%p, %u, %p, %p, %p)->%p\n",

		Hooks::CSteamMatchmakingServers_RequestInternetServerList.name.c_str(),
		pSteamMatchmakingServers,
		appId,
		a2,
		a3,
		a4,
		handle
	);

	FakeAppIds::fakeAppIdMapServer[handle] = appId;

	return handle;
}

__attribute__((hot))
static uint32_t hkUser_CheckAppOwnership(void* pClientUser, uint32_t appId, CAppOwnershipInfo* pOwnershipInfo)
{
	const uint32_t ret = Hooks::CUser_CheckAppOwnership.tramp.fn(pClientUser, appId, pOwnershipInfo);

	//Do not log pOwnershipInfo because it gets deleted very quickly, so it's pretty much useless in the logs
	g_pLog->once
	(
		"%s(%p, %u) -> %i\n",

		Hooks::CUser_CheckAppOwnership.name.c_str(),
		pClientUser,
		appId,
		ret
	);

	if (Apps::checkAppOwnership(appId, pOwnershipInfo) || DLC::checkAppOwnership(appId, pOwnershipInfo))
	{
		return true;
	}

	return ret;
}

static uint32_t hkUser_GetSubscribedApps(void* pClientUser, uint32_t* pAppList, uint32_t size, uint8_t a3)
{
	uint32_t count = Hooks::CUser_GetSubscribedApps.tramp.fn(pClientUser, pAppList, size, a3);

	Apps::getSubscribedApps(pAppList, size, count);

	g_pLog->debug
	(
		"%s(%p, %p, %i, %i) -> %i\n",

		Hooks::CUser_GetSubscribedApps.name.c_str(),
		pClientUser,
		pAppList,
		size,
		a3,
		count
	);

	return count;
}

static bool hkClientAppManager_BCanRemotePlayTogether(void* pClientAppManager, uint32_t appId)
{
	const bool ret = Hooks::IClientAppManager_BCanRemotePlayTogether.tramp.fn(pClientAppManager, appId);
	g_pLog->debug
	(
		"%s(%p, %u) -> %u\n",
		Hooks::IClientAppManager_BCanRemotePlayTogether.name.c_str(),
		pClientAppManager,
		appId,
		ret
	);

	return true;
}

static void* hkClientAppManager_LaunchApp(void* pClientAppManager, uint32_t* pAppId, void* a2, void* a3, void* a4)
{
	if (pAppId)
	{
		g_pLog->once
		(
			"%s(%p, %u, %p, %p, %p)\n",

			Hooks::IClientAppManager_LaunchApp.name.c_str(),
			pClientAppManager,
			*pAppId,
			a2,
			a3,
			a4
		);

		FakeAppIds::launchApp(*pAppId);
		Ticket::launchApp(*pAppId);
	}

	//Do not do anything in post! Otherwise App launching will break
	return Hooks::IClientAppManager_LaunchApp.originalFn.fn(pClientAppManager, pAppId, a2, a3, a4);
}

static bool hkClientAppManager_IsAppDlcInstalled(void* pClientAppManager, uint32_t appId, uint32_t dlcId)
{
	const bool ret = Hooks::IClientAppManager_IsAppDlcInstalled.originalFn.fn(pClientAppManager, appId, dlcId);
	g_pLog->once
	(
		"%s(%p, %u, %u) -> %i\n",

		Hooks::IClientAppManager_IsAppDlcInstalled.name.c_str(),
		pClientAppManager,
		appId,
		dlcId,
		ret
	);

	if (DLC::isAppDlcInstalled(dlcId))
	{
		return true;
	}

	return ret;
}

static bool hkClientAppManager_BIsDlcEnabled(void* pClientAppManager, uint32_t appId, uint32_t dlcId, void* a3)
{
	const bool ret = Hooks::IClientAppManager_BIsDlcEnabled.originalFn.fn(pClientAppManager, appId, dlcId, a3);
	g_pLog->once
	(
		"%s(%p, %u, %u, %p) -> %i\n",

		Hooks::IClientAppManager_BIsDlcEnabled.name.c_str(),
		pClientAppManager,
		appId,
		dlcId,
		a3,
		ret
	);

	
	if (DLC::isDlcEnabled(appId))
	{
		return true;
	}

	return ret;
}

static bool hkClientAppManager_GetUpdateInfo(void* pClientAppManager, uint32_t appId, uint32_t* a2)
{
	const bool success = Hooks::IClientAppManager_GetAppUpdateInfo.originalFn.fn(pClientAppManager, appId, a2);
	g_pLog->once("IClientAppManager::GetUpdateInfo(%p, %u, %p) -> %i\n", pClientAppManager, appId, a2, success);

	if (Apps::shouldDisableUpdates(appId))
	{
		g_pLog->once("Disabled updates for %u\n", appId);
		return false;
	}

	return success;
}

static bool hkClientAppManager_GetAppOwnershipInfo(void* pClientAppManager, uint32_t appId, uint32_t* pInfo)
{
	const bool ret = Hooks::IClientAppManager_GetAppOwnershipInfo.originalFn.fn(pClientAppManager, appId, pInfo);
	constexpr uint32_t invalidPlatformFlag = 0x10;

	if (ret && pInfo && SLSAPI::currentInstallAppId() == appId && (pInfo[1] & invalidPlatformFlag))
	{
		g_pLog->info("API Clearing invalid-platform ownership flag for %u during InstallApp\n", appId);
		pInfo[1] &= ~invalidPlatformFlag;
	}

	return ret;
}

__attribute__((hot))
static void hkClientAppManager_RunIPCFrame(void* pClientAppManager, void* a1, void* a2, void* a3)
{
	g_pClientAppManager = reinterpret_cast<IClientAppManager*>(pClientAppManager);

	static bool vftHooksInitialized = false;
	if (!vftHooksInitialized)
	{
		std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
		LM_VmtNew(*reinterpret_cast<lm_address_t**>(pClientAppManager), vft.get());

		Hooks::IClientAppManager_BIsDlcEnabled.setup(vft, VFTIndexes::IClientAppManager::BIsDlcEnabled, hkClientAppManager_BIsDlcEnabled);
		Hooks::IClientAppManager_GetAppUpdateInfo.setup(vft, VFTIndexes::IClientAppManager::GetUpdateInfo, hkClientAppManager_GetUpdateInfo);
		Hooks::IClientAppManager_LaunchApp.setup(vft, VFTIndexes::IClientAppManager::LaunchApp, hkClientAppManager_LaunchApp);
		Hooks::IClientAppManager_IsAppDlcInstalled.setup(vft, VFTIndexes::IClientAppManager::IsAppDlcInstalled, hkClientAppManager_IsAppDlcInstalled);
		Hooks::IClientAppManager_GetAppOwnershipInfo.setup(vft, VFTIndexes::IClientAppManager::GetAppOwnershipInfo, hkClientAppManager_GetAppOwnershipInfo);

		Hooks::IClientAppManager_BIsDlcEnabled.place();
		Hooks::IClientAppManager_GetAppUpdateInfo.place();
		Hooks::IClientAppManager_LaunchApp.place();
		Hooks::IClientAppManager_IsAppDlcInstalled.place();
		Hooks::IClientAppManager_GetAppOwnershipInfo.place();

		g_pLog->debug("IClientAppManager->vft at %p\n", vft->vtable);
		vftHooksInitialized = true;
	}

	Hooks::IClientAppManager_RunIPCFrame.tramp.fn(pClientAppManager, a1, a2, a3);
	SLSAPI::runPendingRequests();
}

static unsigned int hkClientApps_GetDLCCount(void* pClientApps, uint32_t appId)
{
	uint32_t count = Hooks::IClientApps_GetDLCCount.originalFn.fn(pClientApps, appId);
	g_pLog->once
	(
		"%s(%p, %u) -> %u\n",

		Hooks::IClientApps_GetDLCCount.name.c_str(),
		pClientApps,
		appId,
		count
	);
	
	appId = FakeAppIds::getRealAppIdForCurrentPipe();

	const uint32_t override = DLC::getDlcCount(appId);
	if (override)
	{
		return override;
	}

	return count;
}

static bool hkClientApps_GetDLCDataByIndex(void* pClientApps, uint32_t appId, int dlcIndex, uint32_t* pDlcId, bool* pIsAvailable, char* pChDlcName, size_t dlcNameLen)
{
	appId = FakeAppIds::getRealAppIdForCurrentPipe();

	//Preserve original call to populate stuff
	const bool ret = DLC::getDlcDataByIndex(appId, dlcIndex, pDlcId, pIsAvailable, pChDlcName, dlcNameLen)
		|| Hooks::IClientApps_GetDLCDataByIndex.originalFn.fn(pClientApps, appId, dlcIndex, pDlcId, pIsAvailable, pChDlcName, dlcNameLen);


	g_pLog->once
	(
		"%s(%p, %u, %i, %p, %p, %s, %i) -> %i\n",

		Hooks::IClientApps_GetDLCDataByIndex.name.c_str(),
		pClientApps,
		appId,
		dlcIndex,
		pDlcId,
		pIsAvailable,
		pChDlcName,
		dlcNameLen,
		ret
	);

	return ret;
}

__attribute__((hot))
static void hkClientApps_RunIPCFrame(void* pClientApps, void* a1, void* a2, void* a3)
{
	static bool hooked = false;
	if (!hooked)
	{
		g_pClientApps = reinterpret_cast<IClientApps*>(pClientApps);

		std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
		LM_VmtNew(*reinterpret_cast<lm_address_t**>(pClientApps), vft.get());

		Hooks::IClientApps_GetDLCDataByIndex.setup(vft, VFTIndexes::IClientApps::GetDLCDataByIndex, hkClientApps_GetDLCDataByIndex);
		Hooks::IClientApps_GetDLCCount.setup(vft, VFTIndexes::IClientApps::GetDLCCount, hkClientApps_GetDLCCount);

		Hooks::IClientApps_GetDLCDataByIndex.place();
		Hooks::IClientApps_GetDLCCount.place();

		g_pLog->debug("IClientApps->vft at %p\n", vft->vtable);

		hooked = true;
	}

	Hooks::IClientApps_RunIPCFrame.tramp.fn(pClientApps, a1, a2, a3);
	SLSAPI::runPendingRequests();
}

static bool hkClientRemoteStorage_IsCloudEnabledForApp(void* pClientRemoteStorage, uint32_t appId)
{
	const bool enabled = Hooks::IClientRemoteStorage_IsCloudEnabledForApp.originalFn.fn(pClientRemoteStorage, appId);
	g_pLog->once
	(
		"%s(%p, %u) -> %i\n",

		Hooks::IClientRemoteStorage_IsCloudEnabledForApp.name.c_str(),
		pClientRemoteStorage,
		appId,
		enabled
	);

	if (Apps::shouldDisableCloud(appId))
	{
		g_pLog->once("Disabled cloud for %u\n", appId);
		return false;
	}

	return enabled;
}

static void hkClientRemoteStorage_RunIPCFrame(void* pClientRemoteStorage, void* a1, void* a2, void* a3)
{

	static bool hooked = false;
	if (!hooked)
	{
		std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
		LM_VmtNew(*reinterpret_cast<lm_address_t**>(pClientRemoteStorage), vft.get());

		Hooks::IClientRemoteStorage_IsCloudEnabledForApp.setup(vft, VFTIndexes::IClientRemoteStorage::IsCloudEnabledForApp, hkClientRemoteStorage_IsCloudEnabledForApp);
		Hooks::IClientRemoteStorage_IsCloudEnabledForApp.place();

		g_pLog->debug("IClientRemoteStorage->vft at %p\n", vft->vtable);

		hooked = true;
	}
	
	//Cloud & Workshop
	FakeAppIds::runIPCFrame(false);
	Hooks::IClientRemoteStorage_RunIPCFrame.tramp.fn(pClientRemoteStorage, a1, a2, a3);
	FakeAppIds::runIPCFrame(true);
	SLSAPI::runPendingRequests();
}

static void hkClientUGC_RunIPCFrame(void* pClientUGC, void* a1, void* a2, void* a3)
{
	//Workshop
	FakeAppIds::runIPCFrame(false);
	Hooks::IClientUGC_RunIPCFrame.tramp.fn(pClientUGC, a1, a2, a3);
	FakeAppIds::runIPCFrame(true);
	SLSAPI::runPendingRequests();
}

static uint32_t hkClientUtils_GetAppId(void* pClientUtils)
{
	uint32_t appId = Hooks::IClientUtils_GetAppId.originalFn.fn(pClientUtils);

	g_pLog->debug
	(
		"%s(%p) -> %u\n",

		Hooks::IClientUtils_GetAppId.name.c_str(),
		pClientUtils,
		appId
	);

	const uint32_t real = FakeAppIds::getRealAppIdForCurrentPipe(false);
	if(real)
	{
		g_pLog->debug("Overwriting appId with %u\n", real);
		return real;
	}

	return appId;
}

static bool hkClientUtils_GetOfflineMode(void* pClientUtils)
{
	const bool ret = Hooks::IClientUtils_GetOfflineMode.originalFn.fn(pClientUtils);

	if (Misc::shouldFakeOffline())
	{
		return true;
	}

	return ret;
}

static void hkClientUtils_RunIPCFrame(void* pClientUtils, void* a1, void* a2, void* a3)
{
	static bool hooked = false;
	if (!hooked)
	{
		g_pClientUtils = reinterpret_cast<IClientUtils*>(pClientUtils);

		std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
		LM_VmtNew(*reinterpret_cast<lm_address_t**>(pClientUtils), vft.get());

		Hooks::IClientUtils_GetAppId.setup(vft, VFTIndexes::IClientUtils::GetAppId, hkClientUtils_GetAppId);
		Hooks::IClientUtils_GetOfflineMode.setup(vft, VFTIndexes::IClientUtils::GetOfflineMode, hkClientUtils_GetOfflineMode);

		Hooks::IClientUtils_GetAppId.place();
		Hooks::IClientUtils_GetOfflineMode.place();

		g_pLog->debug("IClientUtils->vft at %p\n", vft->vtable);

		hooked = true;
	}

	Hooks::IClientUtils_RunIPCFrame.tramp.fn(pClientUtils, a1, a2, a3);
	SLSAPI::runPendingRequests();
}

static bool hkClientUser_BLoggedOn(void* pClientUser)
{
	const bool ret = Hooks::IClientUser_BLoggedOn.tramp.fn(pClientUser);
	//Useless logging
	//g_pLog->debug
	//(
	//	"%s(%p) -> %i\n",
	//	Hooks::IClientUser_BLoggedOn.name.c_str(),
	//	pClientUser,
	//	ret
	//);
	
	if (Misc::shouldFakeOffline())
	{
		return false;
	}

	return ret;
}

static uint32_t hkClientUser_BUpdateOwnershipTicket(void* pClientUser, uint32_t appId, bool staleOnly)
{
	const auto cached = Ticket::getCachedTicket(appId);
	if (g_pSteamEngine->getUser(0)->isSubscribed(appId) && !cached.steamId)
	{
		staleOnly = false;
		g_pLog->debug("Force re-requesting OwnershipInfo for %u\n", appId);
	}

	const uint32_t ret = Hooks::IClientUser_BUpdateAppOwnershipTicket.tramp.fn(pClientUser, appId, staleOnly);

	g_pLog->debug
	(
		"%s(%p, %u, %i) -> %u\n",

		Hooks::IClientUser_BUpdateAppOwnershipTicket.name.c_str(),
		pClientUser,
		appId,
		staleOnly,
		ret
	);

	return ret;
}

static uint32_t hkClientUser_GetAppOwnershipTicketExtendedData(
	void* pClientUser,
	uint32_t appId,
	void* pTicket,
	uint32_t ticketSize,
	uint32_t* a4,
	uint32_t* a5,
	uint32_t* a6,
	uint32_t* a7)

{
	const uint32_t ret = Hooks::IClientUser_GetAppOwnershipTicketExtendedData.tramp.fn(pClientUser, appId, pTicket, ticketSize, a4, a5, a6, a7);
	g_pLog->once("%s(%u)->%u\n", Hooks::IClientUser_GetAppOwnershipTicketExtendedData.name.c_str(), appId, ret);

	Ticket::getTicketOwnershipExtendedData(appId);

	return ret;
}

static uint8_t hkClientUser_IsUserSubscribedAppInTicket(void* pClientUser, uint32_t steamId, uint32_t a2, uint32_t a3, uint32_t appId)
{
	const uint8_t ticketState = Hooks::IClientUser_IsUserSubscribedAppInTicket.tramp.fn(pClientUser, steamId, a2, a3, appId);
	//g_pLog->once("IClientUser::IsUserSubscribedAppInTicket(%p, %u, %u, %u, %u) -> %i\n", pClientUser, steamId, a2, a3, appId, ticketState);
	//Don't log the steamId, protect users from themselves and stuff
	g_pLog->once
	(
		"%s(%p, %u, %u, %u) -> %i\n",

		Hooks::IClientUser_IsUserSubscribedAppInTicket.name.c_str(),
		pClientUser,
		a2,
		a3,
		appId,
		ticketState
	);
	
	if (DLC::userSubscribedInTicket(appId))
	{
		//Owned and subscribed hehe :)
		return 0;
	}

	return ticketState;
}

__attribute__((stdcall))
static uint32_t hkClientUser_GetSteamId(uint32_t steamId)
{
	if (!g_currentSteamId)
	{
		g_currentSteamId = steamId;
	}

	Ticket::SavedTicket ticket = Ticket::getCachedEncryptedTicket(FakeAppIds::getRealAppIdForCurrentPipe());

	if (ticket.steamId)
	{
		steamId = ticket.steamId;
	}
	else if (Ticket::oneTimeSteamIdSpoof)
	{
		//One time spoof should be enough for this type
		steamId = Ticket::oneTimeSteamIdSpoof;
		Ticket::oneTimeSteamIdSpoof = 0;
	}

	return steamId;
}

static bool hkClientUser_RequiresLegacyCDKey(void* pClientUser, uint32_t appId, uint32_t* a2)
{
	const bool requiresKey = Hooks::IClientUser_RequiresLegacyCDKey.tramp.fn(pClientUser, appId, a2);
	g_pLog->once
	(
		"%s(%p, %u, %u) -> %i\n",

		Hooks::IClientUser_RequiresLegacyCDKey.name.c_str(),
		pClientUser,
		appId,
		a2,
		requiresKey
	);

	if (Apps::shouldDisableCDKey(appId))
	{
		g_pLog->once("Disable CD Key for %u\n", appId);
		return false;
	}

	return requiresKey;
}

static void hkClientUser_RunIPCFrame(void* pClientUser, void* a1, void* a2, void* a3)
{
	//g_pClientUser = reinterpret_cast<IClientUser*>(pClientUser);

	//std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
	//LM_VmtNew(*reinterpret_cast<lm_address_t**>(pClientUser), vft.get());

	//g_pLog->debug("IClientUser->vft at %p\n", vft->vtable);

	//Hooks::IClientUser_RunIPCFrame.remove();
	//Hooks::IClientUser_RunIPCFrame.originalFn.fn(pClientUser, a1, a2, a3);
	
	//FakeAppIds::pipeLoop(false);
	Hooks::IClientUser_RunIPCFrame.tramp.fn(pClientUser, a1, a2, a3);
	SLSAPI::runPendingRequests();
	//FakeAppIds::pipeLoop(true);
}

static void hkClientUserStats_RunIPCFrame(void* pClientUserStats, void* a1, void* a2, void* a3)
{
	//Achievements
	FakeAppIds::runIPCFrame(false);
	Hooks::IClientUserStats_RunIPCFrame.tramp.fn(pClientUserStats, a1, a2, a3);
	FakeAppIds::runIPCFrame(true);
	SLSAPI::runPendingRequests();
}

static void hkSteamMatchmakingPingResponse_ServerResponded(void* pSteamMatchingPingResponse, gameserverdetails_t* details)
{
	FakeAppIds::pingResponse(details);
	Hooks::ISteamMatchmakingPingResponse_ServerResponded.tramp.fn(pSteamMatchingPingResponse, details);
}

static void patchRetn(lm_address_t address)
{
	constexpr lm_byte_t retn = 0xC3;

	lm_prot_t oldProt;
	LM_ProtMemory(address, 1, LM_PROT_XRW, &oldProt); //LM_PROT_W Should be enough, but just in case something tries to execute it inbetween us setting the prot and writing to it
	LM_WriteMemory(address, &retn, 1);
	LM_ProtMemory(address, 1, oldProt, LM_NULL);
}

static lm_address_t hkNakedGetSteamId;
static bool createAndPlaceSteamIdHook()
{
	hkNakedGetSteamId = LM_AllocMemory(0, LM_PROT_XRW);
	if (hkNakedGetSteamId == LM_ADDRESS_BAD)
	{
		g_pLog->debug("Failed to allocate memory for GetSteamId!\n");
		return false;
	}

	g_pLog->debug("Allocated memory for GetSteamId hook at %p\n", hkNakedGetSteamId);

	auto insts = std::vector<lm_inst_t>();
	lm_address_t readAddr = Hooks::IClientUser_GetSteamId;
	for(;;)
	{
		lm_inst_t inst;
		if (!LM_Disassemble(readAddr, &inst))
		{
			g_pLog->debug("Failed to disassemble function at %p!\n", readAddr);
			return false;
		}

		insts.emplace_back(inst);
		readAddr = inst.address + inst.size;

		if (strcmp(inst.mnemonic, "ret") == 0)
		{
			break;
		}
	}

	const unsigned int retIdx = insts.size() - 1;

	g_pLog->debug("Ret is instruction number %u\n", retIdx);
	//TODO: Create InlineHook class for this
	size_t totalBytes = 0;
	unsigned int instsToOverwrite = 0;
	for(int i = retIdx; i >= 0; i--)
	{
		lm_inst_t inst = insts.at(i);
		totalBytes += inst.size;
		instsToOverwrite++;

		//Need only 5 bytes to place relative jmp
		if (totalBytes >= 5)
		{
			break;
		}
	}

	static uint32_t steamId;

	lm_address_t writeAddr = hkNakedGetSteamId;
	//I really didn't want to use pushad and popad since it's just lazy
	//But I'm bad at this so this has to do
	MemHlp::assembleCodeAt(writeAddr, "mov [%p], ecx", &steamId);
	MemHlp::assembleCodeAt(writeAddr, "pushad", nullptr);
	MemHlp::assembleCodeAt(writeAddr, "pushfd", nullptr);
	//MemHlp::assembleCodeAt(writeAddr, "pushfq", nullptr);

	MemHlp::assembleCodeAt(writeAddr, "mov eax, %p", &hkClientUser_GetSteamId);
	MemHlp::assembleCodeAt(writeAddr, "mov ebx, [%p]", &steamId);
	MemHlp::assembleCodeAt(writeAddr, "push ebx", steamId);
	MemHlp::assembleCodeAt(writeAddr, "call eax", nullptr);
	MemHlp::assembleCodeAt(writeAddr, "mov [%p], eax", &steamId);

	//MemHlp::assembleCodeAt(writeAddr, "popfq", nullptr);
	MemHlp::assembleCodeAt(writeAddr, "popfd", nullptr);
	MemHlp::assembleCodeAt(writeAddr, "popad", nullptr);
	MemHlp::assembleCodeAt(writeAddr, "mov ecx, [%p]", &steamId);
	
	//TODO: Dynamically resolve register which holds SteamId
	//MemHlp::assembleCodeAt(writeAddr, "mov [%p], ecx", &g_currentSteamId);

	//MemHlp::assembleCodeAt(writeAddr, "push eax", nullptr);

	//MemHlp::assembleCodeAt(writeAddr, "mov eax, [%p]", &Ticket::steamIdSpoof);
	//MemHlp::assembleCodeAt(writeAddr, "test eax, eax", nullptr);
	//MemHlp::assembleCodeAt(writeAddr, "je %p", 4); //2 bytes
	//MemHlp::assembleCodeAt(writeAddr, "mov ecx, eax", nullptr); //2 bytes
	//MemHlp::assembleCodeAt(writeAddr, "mov eax, 0", nullptr); //5 bytes
	//MemHlp::assembleCodeAt(writeAddr, "mov [%p], eax", &Ticket::steamIdSpoof); //5 bytes
	//
	//MemHlp::assembleCodeAt(writeAddr, "pop eax", nullptr);

	//Write the overwritten instructions after our hook code
	for (unsigned int i = 0; i < instsToOverwrite; i++)
	{
		lm_inst_t inst = insts.at(insts.size() - instsToOverwrite + i);
		memcpy(reinterpret_cast<void*>(writeAddr), inst.bytes, inst.size);

		writeAddr += inst.size;
		g_pLog->debug("Copied %s %s to tramp\n", inst.mnemonic, inst.op_str);
	}

	lm_address_t jmpAddr = insts.at(insts.size() - instsToOverwrite).address;
	g_pLog->debug("Placing jmp at %p\n", jmpAddr);

	//Might be worth to convert to LM_AssembleEx, but whatever
	lm_prot_t oldProt;
	LM_ProtMemory(jmpAddr, 5, LM_PROT_XRW, &oldProt);
	*reinterpret_cast<lm_byte_t*>(jmpAddr) = 0xE9;
	*reinterpret_cast<lm_address_t*>(jmpAddr + 1) = hkNakedGetSteamId - jmpAddr - 5;
	LM_ProtMemory(jmpAddr, 5, oldProt, nullptr);

	return true;
}

namespace Hooks
{
	//TODO: Lazily intialize in a different way, or preload glibc
	DetourHook<TraceIPC_t> TraceIPC;

	DetourHook<IClientAppManager_RunIPCFrame_t> IClientAppManager_RunIPCFrame;
	DetourHook<IClientApps_RunIPCFrame_t> IClientApps_RunIPCFrame;
	DetourHook<IClientRemoteStorage_RunIPCFrame_t> IClientRemoteStorage_RunIPCFrame;
	DetourHook<IClientUGC_RunIPCFrame_t> IClientUGC_RunIPCFrame;
	DetourHook<IClientUtils_RunIPCFrame_t> IClientUtils_RunIPCFrame;
	DetourHook<IClientUser_RunIPCFrame_t> IClientUser_RunIPCFrame;
	DetourHook<IClientUserStats_RunIPCFrame_t> IClientUserStats_RunIPCFrame;

	DetourHook<CAPIJob_GetPlayerStats_t> CAPIJob_GetPlayerStats;

	DetourHook<CProtoBufMsgBase_InitFromPacket_t> CProtoBufMsgBase_InitFromPacket;
	DetourHook<CProtoBufMsgBase_Send_t> CProtoBufMsgBase_Send;

	DetourHook<CSteamMatchmakingServers_GetServerDetails_t> CSteamMatchmakingServers_GetServerDetails;
	DetourHook<CSteamMatchmakingServers_RequestInternetServerList_t> CSteamMatchmakingServers_RequestInternetServerList;

	DetourHook<CSteamEngine_Init_t> CSteamEngine_Init;
	DetourHook<CSteamEngine_SetAppIdForCurrentPipe_t> CSteamEngine_SetAppIdForCurrentPipe;

	DetourHook<CUser_CheckAppOwnership_t> CUser_CheckAppOwnership;
	DetourHook<CUser_GetSubscribedApps_t> CUser_GetSubscribedApps;

	DetourHook<IClientAppManager_BCanRemotePlayTogether_t> IClientAppManager_BCanRemotePlayTogether;

	DetourHook<IClientUser_BLoggedOn_t> IClientUser_BLoggedOn;
	DetourHook<IClientUser_BUpdateAppOwnershipTicket_t> IClientUser_BUpdateAppOwnershipTicket;
	DetourHook<IClientUser_GetAppOwnershipTicketExtendedData_t> IClientUser_GetAppOwnershipTicketExtendedData;
	DetourHook<IClientUser_IsUserSubscribedAppInTicket_t> IClientUser_IsUserSubscribedAppInTicket;
	DetourHook<IClientUser_RequiresLegacyCDKey_t> IClientUser_RequiresLegacyCDKey;

	VFTHook<IClientAppManager_BIsDlcEnabled_t> IClientAppManager_BIsDlcEnabled("IClientAppManager::BIsDlcEnabled");
	VFTHook<IClientAppManager_GetAppUpdateInfo_t> IClientAppManager_GetAppUpdateInfo("IClientAppManager::GetAppUpdateInfo");
	VFTHook<IClientAppManager_LaunchApp_t> IClientAppManager_LaunchApp("IClientAppManager::LaunchApp");
	VFTHook<IClientAppManager_IsAppDlcInstalled_t> IClientAppManager_IsAppDlcInstalled("IClientAppManager::IsAppDlcInstalled");
	VFTHook<IClientAppManager_GetAppOwnershipInfo_t> IClientAppManager_GetAppOwnershipInfo("IClientAppManager::GetAppOwnershipInfo");

	VFTHook<IClientApps_GetDLCDataByIndex_t> IClientApps_GetDLCDataByIndex("IClientApps::GetDLCDataByIndex");
	VFTHook<IClientApps_GetDLCCount_t> IClientApps_GetDLCCount("IClientApps::GetDLCCount");

	VFTHook<IClientRemoteStorage_IsCloudEnabledForApp_t> IClientRemoteStorage_IsCloudEnabledForApp("IClientRemoteStorage::IsCloudEnabledForApp");

	VFTHook<IClientUtils_GetAppId_t> IClientUtils_GetAppId("IClientUtils::GetAppId");
	VFTHook<IClientUtils_GetOfflineMode_t> IClientUtils_GetOfflineMode("IClientUtils::GetOfflineMode");


	//steamui.so
	DetourHook<ISteamMatchmakingPingResponse_ServerResponded_t> ISteamMatchmakingPingResponse_ServerResponded;


	//Naked
	lm_address_t IClientUser_GetSteamId;
}

bool Hooks::setup()
{
	g_pLog->debug("Hooks::setup()\n");

	IClientUser_GetSteamId = Patterns::IClientUser::GetSteamId.address;

	bool succeeded =
		TraceIPC.setup(Patterns::TraceIPC, &hkTraceIPC)

		&& CAPIJob_GetPlayerStats.setup(Patterns::CAPIJob::GetPlayerStats, &hkCAPIJob_GetPlayerStats)

		&& CProtoBufMsgBase_InitFromPacket.setup(Patterns::CProtoBufMsgBase::InitFromPacket, &hkProtoBufMsgBase_InitFromPacket)
		&& CProtoBufMsgBase_Send.setup(Patterns::CProtoBufMsgBase::Send, &hkProtoBufMsgBase_Send)

		&& CSteamMatchmakingServers_GetServerDetails.setup(Patterns::CSteamMatchmakingServers::GetServerDetails, &hkSteamMatchmakingServers_GetServerDetails)
		&& CSteamMatchmakingServers_RequestInternetServerList.setup(Patterns::CSteamMatchmakingServers::RequestInternetServerList, &hkSteamMatchmakingServers_RequestInternetServerList)

		&& CUser_CheckAppOwnership.setup(Patterns::CUser::CheckAppOwnership, &hkUser_CheckAppOwnership)
		&& CUser_GetSubscribedApps.setup(Patterns::CUser::GetSubscribedApps, &hkUser_GetSubscribedApps)

		&& CSteamEngine_Init.setup(Patterns::CSteamEngine::Init, &hkSteamEngine_Init)
		&& CSteamEngine_SetAppIdForCurrentPipe.setup(Patterns::CSteamEngine::SetAppIdForCurrentPipe, &hkSteamEngine_SetAppIdForCurrentPipe)

		&& IClientAppManager_BCanRemotePlayTogether.setup(Patterns::IClientAppManager::BCanRemotePlayTogether, hkClientAppManager_BCanRemotePlayTogether)

		&& IClientApps_RunIPCFrame.setup(Patterns::IClientApps::RunIPCFrame, hkClientApps_RunIPCFrame)
		&& IClientAppManager_RunIPCFrame.setup(Patterns::IClientAppManager::RunIPCFrame, hkClientAppManager_RunIPCFrame)
		&& IClientRemoteStorage_RunIPCFrame.setup(Patterns::IClientRemoteStorage::RunIPCFrame, hkClientRemoteStorage_RunIPCFrame)
		&& IClientUGC_RunIPCFrame.setup(Patterns::IClientUGC::RunIPCFrame, hkClientUGC_RunIPCFrame)
		&& IClientUtils_RunIPCFrame.setup(Patterns::IClientUtils::RunIPCFrame, hkClientUtils_RunIPCFrame)
		&& IClientUser_RunIPCFrame.setup(Patterns::IClientUser::RunIPCFrame, hkClientUser_RunIPCFrame)
		&& IClientUserStats_RunIPCFrame.setup(Patterns::IClientUserStats::RunIPCFrame, hkClientUserStats_RunIPCFrame)

		&& IClientUser_BLoggedOn.setup(Patterns::IClientUser::BLoggedOn, &hkClientUser_BLoggedOn)
		&& IClientUser_BUpdateAppOwnershipTicket.setup(Patterns::IClientUser::BUpdateAppOwnershipTicket, hkClientUser_BUpdateOwnershipTicket)
		&& IClientUser_GetAppOwnershipTicketExtendedData.setup(Patterns::IClientUser::GetAppOwnershipTicketExtendedData, hkClientUser_GetAppOwnershipTicketExtendedData)
		&& IClientUser_IsUserSubscribedAppInTicket.setup(Patterns::IClientUser::IsUserSubscribedAppInTicket, &hkClientUser_IsUserSubscribedAppInTicket)
		&& IClientUser_RequiresLegacyCDKey.setup(Patterns::IClientUser::RequiresLegacyCDKey, hkClientUser_RequiresLegacyCDKey)

		&& ISteamMatchmakingPingResponse_ServerResponded.setup(Patterns::ISteamMatchmakingPingResponse::ServerResponded, hkSteamMatchmakingPingResponse_ServerResponded);

	Hooks::place();
	//This is unnecessary but I'll keep this for now in case I wanna improve error checks
	return succeeded;
}

void Hooks::place()
{
	if (g_config.disableFamilyLock.get())
	{
		patchRetn(Patterns::FamilyGroupRunningApp.address);
		patchRetn(Patterns::StopPlayingBorrowedApp.address);
	}

	//Detours
	TraceIPC.place();

	CAPIJob_GetPlayerStats.place();

	CProtoBufMsgBase_InitFromPacket.place();
	CProtoBufMsgBase_Send.place();

	CSteamEngine_Init.place();
	CSteamEngine_SetAppIdForCurrentPipe.place();

	CSteamMatchmakingServers_GetServerDetails.place();
	CSteamMatchmakingServers_RequestInternetServerList.place();

	CUser_CheckAppOwnership.place();
	CUser_GetSubscribedApps.place();

	IClientAppManager_BCanRemotePlayTogether.place();

	IClientApps_RunIPCFrame.place();
	IClientAppManager_RunIPCFrame.place();
	IClientRemoteStorage_RunIPCFrame.place();
	IClientUGC_RunIPCFrame.place();
	IClientUtils_RunIPCFrame.place();
	IClientUser_RunIPCFrame.place();
	IClientUserStats_RunIPCFrame.place();

	IClientUser_BLoggedOn.place();
	IClientUser_BUpdateAppOwnershipTicket.place();
	IClientUser_GetAppOwnershipTicketExtendedData.place();
	IClientUser_IsUserSubscribedAppInTicket.place();
	IClientUser_RequiresLegacyCDKey.place();

	ISteamMatchmakingPingResponse_ServerResponded.place();

	createAndPlaceSteamIdHook();
}

void Hooks::remove()
{
	//Detours
	TraceIPC.remove();

	CAPIJob_GetPlayerStats.remove();

	CProtoBufMsgBase_InitFromPacket.remove();
	CProtoBufMsgBase_Send.remove();

	CSteamEngine_Init.remove();
	CSteamEngine_SetAppIdForCurrentPipe.remove();

	CSteamMatchmakingServers_GetServerDetails.remove();
	CSteamMatchmakingServers_RequestInternetServerList.remove();

	CUser_CheckAppOwnership.remove();
	CUser_GetSubscribedApps.remove();

	IClientAppManager_BCanRemotePlayTogether.remove();

	IClientApps_RunIPCFrame.remove();
	IClientAppManager_RunIPCFrame.remove();
	IClientRemoteStorage_RunIPCFrame.remove();
	IClientUGC_RunIPCFrame.remove();
	IClientUtils_RunIPCFrame.remove();
	IClientUser_RunIPCFrame.remove();
	IClientUserStats_RunIPCFrame.remove();

	IClientUser_BLoggedOn.remove();
	IClientUser_BUpdateAppOwnershipTicket.remove();
	IClientUser_GetAppOwnershipTicketExtendedData.remove();
	IClientUser_IsUserSubscribedAppInTicket.remove();
	IClientUser_RequiresLegacyCDKey.remove();

	ISteamMatchmakingPingResponse_ServerResponded.remove();

	//VFT Hooks
	IClientAppManager_BIsDlcEnabled.remove();
	IClientAppManager_GetAppUpdateInfo.remove();
	IClientAppManager_LaunchApp.remove();
	IClientAppManager_IsAppDlcInstalled.remove();
	IClientAppManager_GetAppOwnershipInfo.remove();

	IClientApps_GetDLCDataByIndex.remove();
	IClientApps_GetDLCCount.remove();

	IClientRemoteStorage_IsCloudEnabledForApp.remove();

	IClientUtils_GetAppId.remove();
	
	//TODO: Remove jmp
	if (hkNakedGetSteamId != LM_ADDRESS_BAD)
	{
		LM_FreeMemory(hkNakedGetSteamId, 0);
	}
}
