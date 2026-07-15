#include "hooks.hpp"

#include "api.hpp"
#include "config.hpp"
#include "globals.hpp"
#include "log.hpp"
#include "memhlp.hpp"
#include "patterns.hpp"
#include "vftableinfo.hpp"

#include "sdk/CAPIJob.hpp"
#include "sdk/CClientUnifiedServiceTransport.hpp"
#include "sdk/CNetPacket.hpp"
#include "sdk/CPackageInfoCache.hpp"
#include "sdk/CProtoBufMsgBase.hpp"
#include "sdk/CSteamEngine.hpp"
#include "sdk/CSteamMatchmakingServers.hpp"
#include "sdk/CUser.hpp"
#include "sdk/CUtl.hpp"
#include "sdk/IClientAppManager.hpp"
#include "sdk/IClientApps.hpp"
#include "sdk/IClientUser.hpp"
#include "sdk/IClientUtils.hpp"
#include "sdk/steam.hpp"

#include "feats/achievements.hpp"
#include "feats/apps.hpp"
#include "feats/dlc.hpp"
#include "feats/license.hpp"
#include "feats/misc.hpp"
#include "feats/fakeappid.hpp"
#include "feats/inventory.hpp"
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
VFTHook<T>::VFTHook() : VFTHook<T>("")
{

}

template<typename T>
bool DetourHook<T>::setup(const Pattern_t& pattern, T hookFn)
{
	this->name = pattern.name;
	this->originalFn.address = pattern.address;
	this->hookFn.fn = hookFn;

	return true;
}

template<typename T>
bool DetourHook<T>::setup(const VFTableInfo_t& info, T hookFn)
{
	this->name = info.getPrintName();
	this->originalFn.address = info.address;
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
void VFTHook<T>::setup(std::shared_ptr<lm_vmt_t> vft, const VFTableInfo_t& info, T hookFn)
{
	this->vft = vft;
	this->index = info.index;
	this->name = info.getPrintName();

	this->originalFn.address = LM_VmtGetOriginal(this->vft.get(), this->index);
	this->hookFn.fn = hookFn;
}

static uint32_t hkGetManifest(void* arg0, uint32_t appId, uint32_t depotId, uint32_t arg3, uint32_t arg4, uint32_t arg5, uint32_t arg6)
{
	if (!g_pSteamEngine->getUser(0)->isSubscribed(appId) || g_config.addedAppIds.get().contains(appId))
	{
		depotId = 1;
	}

	return Hooks::GetManifest.tramp.fn(arg0, appId, depotId, arg3, arg4, arg5, arg6);
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

static uint32_t hkAPIJob_SendAndRecv(CAPIJob* pAPIJob, CProtoBufMsgBase* send, uint32_t a2, uint32_t timeOut, CProtoBufMsgBase* recv, EMsg targetType)
{
	uint32_t ret = Achievements::sendAndRecvGetUserStats(pAPIJob, send, timeOut, recv, targetType);

	if (!ret)
	{
		ret = Hooks::CAPIJob_SendAndRecv.tramp.fn(pAPIJob, send, a2, timeOut, recv, targetType);
	}

	g_pLog->debug
	(
		"%s(%p, %s, %u, %u, %s, %u) -> %u\n",

		Hooks::CAPIJob_SendAndRecv.name.c_str(),
		pAPIJob,
		MemHlp::getTypeName(send),
		a2,
		timeOut,
		MemHlp::getTypeName(recv),
		targetType,
		ret
	);

	return ret;
}

static uint32_t hkAppDataCache_BParseResponseFromMessage(void* pAppDataCache, CProtoBufMsgBase* pMsg)
{
	const uint32_t ret = Hooks::CAppDataCache_BParseResponseFromMessage.tramp.fn(pAppDataCache, pMsg);
	g_pLog->debug
	(
		"%s(%p, %p) -> %i\n",
		Patterns::CAppDataCache::BParseResponseMessage.name.c_str(),
		pAppDataCache,
		pMsg,
		ret
	);

	Apps::parseProductInfoFromResponse(pMsg->getBody<CMsgClientPICSProductInfoResponse>());

	return ret;
}

static uint32_t hkClientUnifiedServiceTransport_SendAndRecvMsg(CClientUnifiedServiceTransport* pUnifiedServiceTransport, const char* name, void* send, void* recv, void* arg4)
{
	uint32_t ret = Achievements::sendAndRecvGetPlayerStats
	(
		pUnifiedServiceTransport,
		name,
		reinterpret_cast<CPlayer_GetUserStats_Request*>(send),
		reinterpret_cast<CPlayer_GetUserStats_Response*>(recv)
	);

	if (ret == k_EResultNoResult)
	{
		ret = Hooks::CClientUnifiedServiceMethod_SendAndRecvMsg.tramp.fn(pUnifiedServiceTransport, name, send, recv, arg4);
	}

	Apps::sendAndRecvLastPlayedTimes(name, reinterpret_cast<CPlayer_GetLastPlayedTimes_Response*>(recv));

	g_pLog->debug
	(
		"%s(%p, %s, %s, %s, %p) -> %u\n",

		Hooks::CClientUnifiedServiceMethod_SendAndRecvMsg.name.c_str(),
		pUnifiedServiceTransport,
		name,
		MemHlp::getTypeName(send),
		MemHlp::getTypeName(recv),
		arg4,
		ret
	);

	return ret;
}

static CPackageInfo* hkPackageInfoCache_GetPackageInfo(CPackageInfoCache* pCache, uint32_t packageId, uint32_t a2, uint32_t a3)
{
	const auto pkg = Hooks::CPackageInfoCache_GetPackageInfo.tramp.fn(pCache, packageId, a2, a3);
	g_pLog->once
	(
		"%s(%p, %u, %u, %u) -> %p\n",

		Hooks::CPackageInfoCache_GetPackageInfo.name.c_str(),
		pCache,
		packageId,
		a2,
		a3,
		pkg
	);

	License::getPackageInfo(pkg);

	return pkg;
}

static void hkCMInterface_RecvPkt(void* pCMInterface, CNetPacket* pNetPacket)
{
	g_pLog->debug("RecvPkt %s -> %p\n", pNetPacket->getProtoBufTypeName().c_str(), pNetPacket->getType());

	if (pNetPacket->isValid() && pNetPacket->isProtoBuf())
	{
		const uint32_t type = pNetPacket->getProtoBufType();
		const auto header = pNetPacket->deserializeHeader();

		const bool disableFamilyShareLock = g_config.disableFamilyLock.get();

		if (disableFamilyShareLock && type == k_EMsgClientSharedLibraryStopPlaying)
		{
			pNetPacket->clearBody();
			g_pLog->debug("Chocked k_EMsgClientSharedLibraryStopPlaying\n");
		}

		if (disableFamilyShareLock && type == k_EMsgServiceMethod && header.target_job_name() == "FamilyGroupsClient.NotifyRunningApps#1")
		{
			pNetPacket->clearBody();
			g_pLog->debug("Chocked FamilyGroupsClient.NotifyRunningApps#1\n");
		}

		Misc::recvMsg(pNetPacket);
		Ticket::recvMsg(pNetPacket);
		License::recvMsg(pNetPacket);
	}

	Hooks::CCMInterface_RecvPkt.tramp.fn(pCMInterface, pNetPacket);
}

static uint32_t hkSteamEngine_RunInterface(void* pSteamEngine, CUtlBuffer* pBufInterfaceInfo, CUtlBuffer* a2)
{
	if (!g_pSteamEngine)
	{
		g_pSteamEngine = reinterpret_cast<CSteamEngine*>(pSteamEngine);
	}

	const EInterfaceType type = static_cast<EInterfaceType>(*reinterpret_cast<EInterfaceType*>(pBufInterfaceInfo->mem.base + pBufInterfaceInfo->get) & 0xff);
	const bool switchFakeAppIds = FakeAppIds::shouldUseRealAppIdForInterface(type);

	if (switchFakeAppIds)
	{
		FakeAppIds::runIPCFrame(false);
	}

	const uint32_t ret = Hooks::CSteamEngine_RunInterface.tramp.fn(pSteamEngine, pBufInterfaceInfo, a2);

	if (switchFakeAppIds)
	{
		FakeAppIds::runIPCFrame(true);
	}

	Apps::runIPCFrame();
	License::runIPCFrame();
	SLSAPI::runIPCFrame();

	if (g_config.extendedLogging.get())
	{
		g_pLog->debug
		(
			"%s(%p, %p, %p) -> %u with type %p for appId %u (%u)\n",

			Hooks::CSteamEngine_RunInterface.name.c_str(),
			pSteamEngine,
			pBufInterfaceInfo,
			a2,
			ret,
			type,
			FakeAppIds::getRealAppIdForCurrentPipe(),
			g_pClientUtils->getAppId()
		);
	}

	return ret;
}

static AppId_t hkSteamEngine_SetAppIdForCurrentPipe(void* pSteamEngine, AppId_t appId, bool a2)
{
	FakeAppIds::setAppIdForCurrentPipe(appId);

	const AppId_t ret = Hooks::CSteamEngine_SetAppIdForCurrentPipe.tramp.fn(pSteamEngine, appId, a2);

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

static bool hkWebSocketConnection_BBuildAndAsyncSendFrame(void* pWebSocketConnection, uint32_t type, void* pData, uint32_t dataSize)
{
	if (type == 2)
	{
		CNetPacket packet {};
		packet.body = reinterpret_cast<CNetPacketBody*>(pData);
		packet.size = dataSize;

		g_pLog->debug("SendPkt %s -> %p\n", packet.getProtoBufTypeName().c_str(), packet.getType());

		if (packet.isValid() && packet.isProtoBuf())
		{
			Apps::sendMsg(&packet);
			FakeAppIds::sendMsg(&packet);

			//Do not free ourself since Steam does so. We reuse our CNetPacket buffer
			pData = packet.body;
			dataSize = packet.size;
		}
	}

	return Hooks::CWebSocketConnection_BBuildAndAsyncSendFrame.tramp.fn(pWebSocketConnection, type, pData, dataSize);
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

static uint32_t hkSteamMatchmakingServers_RequestInternetServerList(void* pSteamMatchmakingServers, AppId_t appId, uint32_t a2, uint32_t a3, uint32_t a4)
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
static uint32_t hkUser_CheckAppOwnership(void* pClientUser, AppId_t appId, AppOwnershipInfo_t* pOwnershipInfo)
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

static uint32_t hkUser_GetSubscribedApps(void* pClientUser, AppId_t* pAppList, uint32_t size, uint8_t a3)
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

static bool hkUserAppManager_BuildDepotDependency
(
	void* a0,
	AppId_t appId,
	void* a2,
	CUtlVector<DepotInfo_t>* depots,
	CUtlVector<DepotInfo_t>* sharedDepots,
	void* a5,
	uint32_t* pBuildId,
	bool* a7
)
{
	const bool success = Hooks::CUserAppManager_BuildDepotDependency.tramp.fn(a0, appId, a2, depots, sharedDepots, a5, pBuildId, a7);
	g_pLog->debug("%s(%p, %u) -> %i\n", Hooks::CUserAppManager_BuildDepotDependency.name.c_str(), a0, appId, success);

	Apps::buildDepotDependency(appId, depots, sharedDepots);

	return success;
}


static bool hkClientAppManager_BCanRemotePlayTogether(void* pClientAppManager, AppId_t appId)
{
	const bool ret = Hooks::IClientAppManager_BCanRemotePlayTogether.originalFn.fn(pClientAppManager, appId);
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

static void* hkClientAppManager_LaunchApp(void* pClientAppManager, AppId_t* pAppId, void* a2, void* a3, void* a4)
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

static bool hkClientAppManager_IsAppDlcInstalled(void* pClientAppManager, AppId_t appId, AppId_t dlcId)
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

static bool hkClientAppManager_BIsDlcEnabled(void* pClientAppManager, AppId_t appId, AppId_t dlcId, void* a3)
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

static bool hkClientAppManager_GetUpdateInfo(void* pClientAppManager, AppId_t appId, uint32_t* a2)
{
	const bool success = Hooks::IClientAppManager_GetAppUpdateInfo.originalFn.fn(pClientAppManager, appId, a2);
	g_pLog->once("%s(%p, %u, %p) -> %i\n", Hooks::IClientAppManager_GetAppUpdateInfo.name.c_str(), pClientAppManager, appId, a2, success);

	if (Apps::shouldDisableUpdates(appId))
	{
		g_pLog->once("Disabled updates for %u\n", appId);
		return false;
	}

	return success;
}

__attribute__((hot))
static void hkClientAppManager_RunIPCFrame(void* pClientAppManager, void* a1, void* a2, void* a3)
{
	g_pClientAppManager = reinterpret_cast<IClientAppManager*>(pClientAppManager);

	std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
	LM_VmtNew(*reinterpret_cast<lm_address_t**>(pClientAppManager), vft.get());

	Hooks::IClientAppManager_BCanRemotePlayTogether.setup(vft, VFTIndexes::IClientAppManager::BCanRemotePlayTogether, hkClientAppManager_BCanRemotePlayTogether);
	Hooks::IClientAppManager_BIsDlcEnabled.setup(vft, VFTIndexes::IClientAppManager::BIsDlcEnabled, hkClientAppManager_BIsDlcEnabled);
	Hooks::IClientAppManager_GetAppUpdateInfo.setup(vft, VFTIndexes::IClientAppManager::GetUpdateInfo, hkClientAppManager_GetUpdateInfo);
	Hooks::IClientAppManager_LaunchApp.setup(vft, VFTIndexes::IClientAppManager::LaunchApp, hkClientAppManager_LaunchApp);
	Hooks::IClientAppManager_IsAppDlcInstalled.setup(vft, VFTIndexes::IClientAppManager::IsAppDlcInstalled, hkClientAppManager_IsAppDlcInstalled);

	Hooks::IClientAppManager_BCanRemotePlayTogether.place();
	Hooks::IClientAppManager_BIsDlcEnabled.place();
	Hooks::IClientAppManager_GetAppUpdateInfo.place();
	Hooks::IClientAppManager_LaunchApp.place();
	Hooks::IClientAppManager_IsAppDlcInstalled.place();

	g_pLog->debug("IClientAppManager->vft at %p\n", vft->vtable);

	Hooks::IClientAppManager_RunIPCFrame.remove();
	Hooks::IClientAppManager_RunIPCFrame.originalFn.fn(pClientAppManager, a1, a2, a3);
}

static unsigned int hkClientApps_GetDLCCount(void* pClientApps, AppId_t appId)
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

static bool hkClientApps_GetDLCDataByIndex(void* pClientApps, AppId_t appId, int dlcIndex, AppId_t* pDlcId, bool* pIsAvailable, char* pChDlcName, size_t dlcNameLen)
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
	g_pClientApps = reinterpret_cast<IClientApps*>(pClientApps);

	std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
	LM_VmtNew(*reinterpret_cast<lm_address_t**>(pClientApps), vft.get());

	Hooks::IClientApps_GetDLCDataByIndex.setup(vft, VFTIndexes::IClientApps::GetDLCDataByIndex, hkClientApps_GetDLCDataByIndex);
	Hooks::IClientApps_GetDLCCount.setup(vft, VFTIndexes::IClientApps::GetDLCCount, hkClientApps_GetDLCCount);

	Hooks::IClientApps_GetDLCDataByIndex.place();
	Hooks::IClientApps_GetDLCCount.place();

	g_pLog->debug("IClientApps->vft at %p\n", vft->vtable);

	Hooks::IClientApps_RunIPCFrame.remove();
	Hooks::IClientApps_RunIPCFrame.originalFn.fn(pClientApps, a1, a2, a3);
}

static bool hkClientInventory_GetResultItems(void* pClientInventory, uint32_t handle, SteamItemDetails_t* arrItems, uint32_t arrItemsSize, uint32_t* pItemsCount)
{
	const bool success = Hooks::IClientInventory_GetResultItems.originalFn.fn(pClientInventory, handle, arrItems, arrItemsSize, pItemsCount);

	Inventory::getResultItems(arrItems, arrItemsSize, pItemsCount);

	g_pLog->debug
		(
			"%s(%p, %u, %p, %u, %u) -> %i\n",

			Hooks::IClientInventory_GetResultItems.name.c_str(),
			pClientInventory,
			handle,
			arrItems,
			arrItemsSize,
			*pItemsCount,
			success
		);

	return success;
}

static bool hkClientInventory_GetItemDefinitionIDs(void* pClientInventory, uint32_t* arrItems, uint32_t arrItemsSize, uint32_t handle)
{
	const bool success = Hooks::IClientInventory_GetItemDefinitionIDs.originalFn.fn(pClientInventory, arrItems, arrItemsSize, handle);

	Inventory::getItemDefinitionIds(arrItems, arrItemsSize);

	return success;
}

static void hkClientInventory_RunIPCFrame(void* pClientInventory, void* a1, void* a2, void* a3)
{
	static bool hooked = false;
	if (!hooked)
	{
		std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
		LM_VmtNew(*reinterpret_cast<lm_address_t**>(pClientInventory), vft.get());

		Hooks::IClientInventory_GetItemDefinitionIDs.setup(vft, VFTIndexes::IClientInventory::GetItemDefinitionIDs, hkClientInventory_GetItemDefinitionIDs);
		Hooks::IClientInventory_GetResultItems.setup(vft, VFTIndexes::IClientInventory::GetResultItems, hkClientInventory_GetResultItems);

		Hooks::IClientInventory_GetItemDefinitionIDs.place();
		Hooks::IClientInventory_GetResultItems.place();

		hooked = true;
	}

	Hooks::IClientInventory_RunIPCFrame.tramp.fn(pClientInventory, a1, a2, a3);
}

static bool hkClientRemoteStorage_IsCloudEnabledForApp(void* pClientRemoteStorage, AppId_t appId)
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
	std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
	LM_VmtNew(*reinterpret_cast<lm_address_t**>(pClientRemoteStorage), vft.get());

	Hooks::IClientRemoteStorage_IsCloudEnabledForApp.setup(vft, VFTIndexes::IClientRemoteStorage::IsCloudEnabledForApp, hkClientRemoteStorage_IsCloudEnabledForApp);
	Hooks::IClientRemoteStorage_IsCloudEnabledForApp.place();

	g_pLog->debug("IClientRemoteStorage->vft at %p\n", vft->vtable);
	

	Hooks::IClientRemoteStorage_RunIPCFrame.remove();
	Hooks::IClientRemoteStorage_RunIPCFrame.originalFn.fn(pClientRemoteStorage, a1, a2, a3);
}

static AppId_t hkClientUtils_GetAppId(void* pClientUtils)
{
	AppId_t appId = Hooks::IClientUtils_GetAppId.originalFn.fn(pClientUtils);

	g_pLog->debug
	(
		"%s(%p) -> %u\n",

		Hooks::IClientUtils_GetAppId.name.c_str(),
		pClientUtils,
		appId
	);

	const AppId_t real = FakeAppIds::getRealAppIdForCurrentPipe(false);
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
	g_pClientUtils = reinterpret_cast<IClientUtils*>(pClientUtils);

	std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
	LM_VmtNew(*reinterpret_cast<lm_address_t**>(pClientUtils), vft.get());

	Hooks::IClientUtils_GetAppId.setup(vft, VFTIndexes::IClientUtils::GetAppId, hkClientUtils_GetAppId);
	Hooks::IClientUtils_GetOfflineMode.setup(vft, VFTIndexes::IClientUtils::GetOfflineMode, hkClientUtils_GetOfflineMode);

	Hooks::IClientUtils_GetAppId.place();
	Hooks::IClientUtils_GetOfflineMode.place();

	g_pLog->debug("IClientUtils->vft at %p\n", vft->vtable);


	Hooks::IClientUtils_RunIPCFrame.remove();
	Hooks::IClientUtils_RunIPCFrame.originalFn.fn(pClientUtils, a1, a2, a3);
}

static bool hkClientUser_BLoggedOn(void* pClientUser)
{
	const bool ret = Hooks::IClientUser_BLoggedOn.originalFn.fn(pClientUser);
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

static uint32_t hkClientUser_BUpdateOwnershipTicket(void* pClientUser, AppId_t appId, bool staleOnly)
{
	const auto cached = Ticket::getCachedTicket(appId);
	if (g_pSteamEngine->getUser(0)->isSubscribed(appId) && !cached.steamId)
	{
		staleOnly = false;
		g_pLog->debug("Force re-requesting OwnershipInfo for %u\n", appId);
	}

	const uint32_t ret = Hooks::IClientUser_BUpdateAppOwnershipTicket.originalFn.fn(pClientUser, appId, staleOnly);

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

static uint32_t hkClientUser_GetAppOwnershipTicketExtendedData
(
	void* pClientUser,
	AppId_t appId,
	void* pTicket,
	uint32_t ticketSize,
	uint32_t* pOffAppId,
	uint32_t* pOffSteamId,
	uint32_t* pOffSig,
	uint32_t* pSigSize
)
{
	uint32_t ret = Ticket::getTicketOwnershipExtendedData(appId, pTicket, ticketSize, pOffAppId, pOffSteamId, pOffSig, pSigSize);

	if (ret == 0)
	{
		ret = Hooks::IClientUser_GetAppOwnershipTicketExtendedData.tramp.fn
		(
			pClientUser,
			appId,
			pTicket,
			ticketSize,
			pOffAppId,
			pOffSteamId,
			pOffSig,
			pSigSize
		);
	}

	g_pLog->once("%s(%p, %u)->%u\n", Hooks::IClientUser_GetAppOwnershipTicketExtendedData.name.c_str(), pClientUser, appId, ret);

	return ret;
}

static uint8_t hkClientUser_IsUserSubscribedAppInTicket(void* pClientUser, uint32_t steamId, uint32_t a2, uint32_t a3, AppId_t appId)
{
	const uint8_t ticketState = Hooks::IClientUser_IsUserSubscribedAppInTicket.originalFn.fn(pClientUser, steamId, a2, a3, appId);
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

static bool hkClientUser_RequiresLegacyCDKey(void* pClientUser, AppId_t appId, uint32_t* a2)
{
	const bool requiresKey = Hooks::IClientUser_RequiresLegacyCDKey.originalFn.fn(pClientUser, appId, a2);
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
	g_pClientUser = reinterpret_cast<IClientUser*>(pClientUser);

	std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
	LM_VmtNew(*reinterpret_cast<lm_address_t**>(pClientUser), vft.get());

	Hooks::IClientUser_BLoggedOn.setup(vft, VFTIndexes::IClientUser::BLoggedOn, &hkClientUser_BLoggedOn);
	Hooks::IClientUser_BUpdateAppOwnershipTicket.setup(vft, VFTIndexes::IClientUser::BUpdateAppOwnershipTicket, hkClientUser_BUpdateOwnershipTicket);
	Hooks::IClientUser_GetAppOwnershipTicketExtendedData.setup(vft, VFTIndexes::IClientUser::GetAppOwnershipTicketExtendedData, hkClientUser_GetAppOwnershipTicketExtendedData);
	Hooks::IClientUser_IsUserSubscribedAppInTicket.setup(vft, VFTIndexes::IClientUser::IsUserSubscribedAppInTicket, &hkClientUser_IsUserSubscribedAppInTicket);
	Hooks::IClientUser_RequiresLegacyCDKey.setup(vft, VFTIndexes::IClientUser::RequiresLegacyCDKey, hkClientUser_RequiresLegacyCDKey);

	Hooks::IClientUser_BLoggedOn.place();
	Hooks::IClientUser_BUpdateAppOwnershipTicket.place();
	Hooks::IClientUser_GetAppOwnershipTicketExtendedData.place();
	Hooks::IClientUser_IsUserSubscribedAppInTicket.place();
	Hooks::IClientUser_RequiresLegacyCDKey.place();

	g_pLog->debug("IClientUser->vft at %p\n", vft->vtable);

	//We can hook from here since this gets called before CheckAppOwnership
	//Hooks::IClientUser_GetSteamId = vft->vtable[VFTIndexes::IClientUser::GetSteamID.index];
	//Hooks::createAndPlaceSteamIdHook();


	Hooks::IClientUser_RunIPCFrame.remove();
	Hooks::IClientUser_RunIPCFrame.originalFn.fn(pClientUser, a1, a2, a3);
}

static void hkSteamMatchmakingPingResponse_ServerResponded(void* pSteamMatchingPingResponse, gameserverdetails_t* details)
{
	FakeAppIds::pingResponse(details);
	Hooks::ISteamMatchmakingPingResponse_ServerResponded.tramp.fn(pSteamMatchingPingResponse, details);
}

lm_address_t Hooks::hkNakedGetSteamId;
bool Hooks::createAndPlaceSteamIdHook()
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
	DetourHook<GetManifest_t> GetManifest;
	DetourHook<TraceIPC_t> TraceIPC;

	DetourHook<IClientAppManager_RunIPCFrame_t> IClientAppManager_RunIPCFrame;
	DetourHook<IClientApps_RunIPCFrame_t> IClientApps_RunIPCFrame;
	DetourHook<IClientInventory_RunIPCFrame_t> IClientInventory_RunIPCFrame;
	DetourHook<IClientRemoteStorage_RunIPCFrame_t> IClientRemoteStorage_RunIPCFrame;
	DetourHook<IClientUtils_RunIPCFrame_t> IClientUtils_RunIPCFrame;
	DetourHook<IClientUser_RunIPCFrame_t> IClientUser_RunIPCFrame;

	DetourHook<CAPIJob_SendAndRecv_t> CAPIJob_SendAndRecv;

	DetourHook<CAppDataCache_BParseResponseFromMessage_t> CAppDataCache_BParseResponseFromMessage;

	DetourHook<CClientUnifiedServiceMethod_SendAndRecvMsg_t> CClientUnifiedServiceMethod_SendAndRecvMsg;

	DetourHook<CCMInterface_RecvPkt_t> CCMInterface_RecvPkt;

	DetourHook<CPackageInfoCache_GetPackageInfo_t> CPackageInfoCache_GetPackageInfo;

	DetourHook<CSteamMatchmakingServers_GetServerDetails_t> CSteamMatchmakingServers_GetServerDetails;
	DetourHook<CSteamMatchmakingServers_RequestInternetServerList_t> CSteamMatchmakingServers_RequestInternetServerList;

	DetourHook<CSteamEngine_RunInterface_t> CSteamEngine_RunInterface;
	DetourHook<CSteamEngine_SetAppIdForCurrentPipe_t> CSteamEngine_SetAppIdForCurrentPipe;

	DetourHook<CUser_CheckAppOwnership_t> CUser_CheckAppOwnership;
	DetourHook<CUser_GetSubscribedApps_t> CUser_GetSubscribedApps;

	DetourHook<CUserAppManager_BuildDepotDependency_t> CUserAppManager_BuildDepotDependency;

	DetourHook<CWebSocketConnection_BBuildAndAsyncSendFrame_t> CWebSocketConnection_BBuildAndAsyncSendFrame;

	VFTHook<IClientAppManager_BCanRemotePlayTogether_t> IClientAppManager_BCanRemotePlayTogether;
	VFTHook<IClientAppManager_BIsDlcEnabled_t> IClientAppManager_BIsDlcEnabled;
	VFTHook<IClientAppManager_GetAppUpdateInfo_t> IClientAppManager_GetAppUpdateInfo;
	VFTHook<IClientAppManager_LaunchApp_t> IClientAppManager_LaunchApp;
	VFTHook<IClientAppManager_IsAppDlcInstalled_t> IClientAppManager_IsAppDlcInstalled;

	VFTHook<IClientApps_GetDLCDataByIndex_t> IClientApps_GetDLCDataByIndex;
	VFTHook<IClientApps_GetDLCCount_t> IClientApps_GetDLCCount;

	VFTHook<IClientInventory_GetResultItems_t> IClientInventory_GetResultItems("IClientInventory::GetResultItems");
	VFTHook<IClientInventory_GetItemDefinitionIDs_t> IClientInventory_GetItemDefinitionIDs("IClientInventory::GetItemDefinitionsIDs");

	VFTHook<IClientRemoteStorage_IsCloudEnabledForApp_t> IClientRemoteStorage_IsCloudEnabledForApp;

	VFTHook<IClientUtils_GetAppId_t> IClientUtils_GetAppId;
	VFTHook<IClientUtils_GetOfflineMode_t> IClientUtils_GetOfflineMode;

	VFTHook<IClientUser_BLoggedOn_t> IClientUser_BLoggedOn;
	VFTHook<IClientUser_BUpdateAppOwnershipTicket_t> IClientUser_BUpdateAppOwnershipTicket;
	VFTHook<IClientUser_GetAppOwnershipTicketExtendedData_t> IClientUser_GetAppOwnershipTicketExtendedData;
	VFTHook<IClientUser_IsUserSubscribedAppInTicket_t> IClientUser_IsUserSubscribedAppInTicket;
	VFTHook<IClientUser_RequiresLegacyCDKey_t> IClientUser_RequiresLegacyCDKey;

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
		GetManifest.setup(Patterns::GetManifest, &hkGetManifest)
		&& TraceIPC.setup(Patterns::TraceIPC, &hkTraceIPC)

		&& IClientApps_RunIPCFrame.setup(Patterns::IClientApps::RunIPCFrame, hkClientApps_RunIPCFrame)
		&& IClientAppManager_RunIPCFrame.setup(Patterns::IClientAppManager::RunIPCFrame, hkClientAppManager_RunIPCFrame)
		&& IClientInventory_RunIPCFrame.setup(Patterns::IClientInventory::RunIPCFrame, hkClientInventory_RunIPCFrame)
		&& IClientRemoteStorage_RunIPCFrame.setup(Patterns::IClientRemoteStorage::RunIPCFrame, hkClientRemoteStorage_RunIPCFrame)
		&& IClientUtils_RunIPCFrame.setup(Patterns::IClientUtils::RunIPCFrame, hkClientUtils_RunIPCFrame)
		&& IClientUser_RunIPCFrame.setup(Patterns::IClientUser::RunIPCFrame, hkClientUser_RunIPCFrame)

		&& CAPIJob_SendAndRecv.setup(Patterns::CAPIJob::SendAndRecv, &hkAPIJob_SendAndRecv)

		&& CAppDataCache_BParseResponseFromMessage.setup(Patterns::CAppDataCache::BParseResponseMessage, &hkAppDataCache_BParseResponseFromMessage)

		//We detour hook this virtual function out of respect for my friend Selectively11. His amazing project
		//CloudRedirect hooks the same function using a VFT hook already
		&& CClientUnifiedServiceMethod_SendAndRecvMsg.setup(VFTIndexes::CClientUnifiedServiceTransport::SendAndRecvMsg, &hkClientUnifiedServiceTransport_SendAndRecvMsg)

		//To lazy to move this for now. Doesn't really matter wheter we detour or vft hook
		&& CCMInterface_RecvPkt.setup(VFTIndexes::CCMInterface::RecvPkt, &hkCMInterface_RecvPkt)

		&& CCMInterface_RecvPkt.setup(Patterns::CCMInterface::RecvPkt, &hkCMInterface_RecvPkt)

		&& CPackageInfoCache_GetPackageInfo.setup(Patterns::CPackageInfoCache::GetPackageInfo, &hkPackageInfoCache_GetPackageInfo)

		&& CSteamMatchmakingServers_GetServerDetails.setup(VFTIndexes::CSteamMatchmakingServers::GetServerDetails, &hkSteamMatchmakingServers_GetServerDetails)
		&& CSteamMatchmakingServers_RequestInternetServerList.setup(VFTIndexes::CSteamMatchmakingServers::RequestInternetServerList, &hkSteamMatchmakingServers_RequestInternetServerList)

		&& CUser_CheckAppOwnership.setup(Patterns::CUser::CheckAppOwnership, &hkUser_CheckAppOwnership)
		&& CUser_GetSubscribedApps.setup(Patterns::CUser::GetSubscribedApps, &hkUser_GetSubscribedApps)

		&& CUserAppManager_BuildDepotDependency.setup(Patterns::CUserAppManager::BuildDepotDependency, hkUserAppManager_BuildDepotDependency)

		&& CSteamEngine_RunInterface.setup(Patterns::CSteamEngine::RunInterface, &hkSteamEngine_RunInterface)
		&& CSteamEngine_SetAppIdForCurrentPipe.setup(Patterns::CSteamEngine::SetAppIdForCurrentPipe, &hkSteamEngine_SetAppIdForCurrentPipe)

		&& CWebSocketConnection_BBuildAndAsyncSendFrame.setup(Patterns::CWebSocketConnection::BBuildAndAsyncSendFrame, &hkWebSocketConnection_BBuildAndAsyncSendFrame)

		&& ISteamMatchmakingPingResponse_ServerResponded.setup(Patterns::ISteamMatchmakingPingResponse::ServerResponded, hkSteamMatchmakingPingResponse_ServerResponded);

	Hooks::place();
	//This is unnecessary but I'll keep this for now in case I wanna improve error checks
	return succeeded;
}

void Hooks::place()
{
	createAndPlaceSteamIdHook();

	//Detours
	GetManifest.place();
	TraceIPC.place();

	IClientApps_RunIPCFrame.place();
	IClientAppManager_RunIPCFrame.place();
	IClientInventory_RunIPCFrame.place();
	IClientRemoteStorage_RunIPCFrame.place();
	IClientUtils_RunIPCFrame.place();
	IClientUser_RunIPCFrame.place();

	CAPIJob_SendAndRecv.place();

	CAppDataCache_BParseResponseFromMessage.place();

	CClientUnifiedServiceMethod_SendAndRecvMsg.place();

	CCMInterface_RecvPkt.place();

	CPackageInfoCache_GetPackageInfo.place();

	CSteamEngine_RunInterface.place();
	CSteamEngine_SetAppIdForCurrentPipe.place();

	CSteamMatchmakingServers_GetServerDetails.place();
	CSteamMatchmakingServers_RequestInternetServerList.place();

	CUser_CheckAppOwnership.place();
	CUser_GetSubscribedApps.place();

	CUserAppManager_BuildDepotDependency.place();

	CWebSocketConnection_BBuildAndAsyncSendFrame.place();

	ISteamMatchmakingPingResponse_ServerResponded.place();
}

void Hooks::remove()
{
	//Detours
	GetManifest.remove();
	TraceIPC.remove();

	IClientApps_RunIPCFrame.remove();
	IClientAppManager_RunIPCFrame.remove();
	IClientInventory_RunIPCFrame.remove();
	IClientRemoteStorage_RunIPCFrame.remove();
	IClientUtils_RunIPCFrame.remove();
	IClientUser_RunIPCFrame.remove();

	CAPIJob_SendAndRecv.remove();

	CAppDataCache_BParseResponseFromMessage.remove();

	CClientUnifiedServiceMethod_SendAndRecvMsg.remove();

	CCMInterface_RecvPkt.remove();

	CPackageInfoCache_GetPackageInfo.remove();

	CSteamEngine_RunInterface.remove();
	CSteamEngine_SetAppIdForCurrentPipe.remove();

	CSteamMatchmakingServers_GetServerDetails.remove();
	CSteamMatchmakingServers_RequestInternetServerList.remove();

	CUser_CheckAppOwnership.remove();
	CUser_GetSubscribedApps.remove();

	CUserAppManager_BuildDepotDependency.remove();

	CWebSocketConnection_BBuildAndAsyncSendFrame.remove();

	ISteamMatchmakingPingResponse_ServerResponded.remove();

	//VFT Hooks
	IClientAppManager_BCanRemotePlayTogether.remove();
	IClientAppManager_BIsDlcEnabled.remove();
	IClientAppManager_GetAppUpdateInfo.remove();
	IClientAppManager_LaunchApp.remove();
	IClientAppManager_IsAppDlcInstalled.remove();

	IClientApps_GetDLCDataByIndex.remove();
	IClientApps_GetDLCCount.remove();

	IClientInventory_GetItemDefinitionIDs.remove();
	IClientInventory_GetResultItems.remove();

	IClientRemoteStorage_IsCloudEnabledForApp.remove();

	IClientUtils_GetAppId.remove();
	IClientUtils_GetOfflineMode.remove();

	IClientUser_BLoggedOn.remove();
	IClientUser_BUpdateAppOwnershipTicket.remove();
	IClientUser_GetAppOwnershipTicketExtendedData.remove();
	IClientUser_IsUserSubscribedAppInTicket.remove();
	IClientUser_RequiresLegacyCDKey.remove();

	
	//TODO: Remove jmp
	if (hkNakedGetSteamId != LM_ADDRESS_BAD)
	{
		LM_FreeMemory(hkNakedGetSteamId, 0);
	}
}
