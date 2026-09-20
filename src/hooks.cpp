#include "hooks.hpp"

#include "feats/achievements.hpp"
#include "feats/apps.hpp"
#include "feats/dlc.hpp"
#include "feats/misc.hpp"
#include "feats/fakeappid.hpp"
#include "feats/ticket.hpp"

#include "api.hpp"
#include "config.hpp"
#include "decompiler.hpp"
#include "globals.hpp"
#include "log.hpp"
#include "lua.hpp"
#include "memhlp.hpp"
#include "patterns.hpp"
#include "process.hpp"
#include "vftableinfo.hpp"

#include "libmem/libmem.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <pthread.h>
#include <strings.h>
#include <unistd.h>


template<typename T>
FunctionUnion_t<T>::FunctionUnion_t() { }

template<typename T>
FunctionUnion_t<T>::FunctionUnion_t(const lm_address_t address) : address(address) { }

template<typename T>
FunctionUnion_t<T>::FunctionUnion_t(const T fn) : fn(fn) { }

std::unordered_set<IHook*> IHook::hooks = std::unordered_set<IHook*>();

IHook::IHook()
{
	hooks.emplace(this);
}

IHook::~IHook()
{
	if (hooks.contains(this))
	{
		hooks.erase(this);
	}
}

template<typename T>
Hook<T>::Hook(const std::string& name, const lm_address_t targetAddress, const T hookFn)
: targetAddress(targetAddress), name(name), hookFn(hookFn) { }

template<typename T>
Hook<T>::Hook(const std::string& name, lm_address_t* targetAddressPtr, const T hookFn)
: targetAddressPtr(targetAddressPtr), name(name), hookFn(hookFn) { }

template<typename T>
void Hook<T>::setup()
{
	if (setupRan)
	{
		return;
	}

	//Only one of the target* members will have a value
	if (targetAddress != LM_ADDRESS_BAD)
	{
		originalFn.address = targetAddress;
	}
	else if (targetAddressPtr)
	{
		originalFn.address = *targetAddressPtr;
	}
	else
	{
		LOG_ERROR("No address supplied to hook %s!\n", name.c_str());
		return;
	}

	setupRan = true;
}

template<typename T>
DetourHook<T>::DetourHook(const std::string& name, lm_address_t targetAddress, const T hookFn)
: Hook<T>(name, targetAddress, hookFn) { }

template<typename T>
DetourHook<T>::DetourHook(Pattern_t& targetPattern, const T hookFn)
: Hook<T>(targetPattern.name, &targetPattern.address, hookFn) { }

template<typename T>
DetourHook<T>::DetourHook(VFTableInfo_t& targetVFTInfo, const T hookFn)
: Hook<T>(targetVFTInfo.getPrintName(), &targetVFTInfo.address, hookFn) { }

template<typename T>
void DetourHook<T>::place()
{
	if (!this->setupRan || isHooked())
	{
		return;
	}

	this->size = LM_HookCode(this->originalFn.address, this->hookFn.address, &this->tramp.address);
	MemHlp::fixPICThunkCall(this->name.c_str(), this->originalFn.address, this->tramp.address);

	LOG_DEBUG
	(
		"Detour hooked %s (%p) with hook at 0x%x and tramp at 0x%x\n",
		this->name.c_str(),
		reinterpret_cast<void*>(this->originalFn.address),
		this->hookFn.address,
		this->tramp.address
	);
}

template<typename T>
void DetourHook<T>::remove()
{
	if (!isHooked())
	{
		return;
	}

	LM_UnhookCode(this->originalFn.address, this->tramp.address, this->size);
	this->size = 0;

	LOG_DEBUG("Unhooked %s\n", this->name.c_str());
}

template<typename T>
VFTHook<T>::VFTHook(const std::shared_ptr<lm_vmt_t>& vft, const VFTableInfo_t& mapVFTInfo, const T hookFn)
: Hook<T>(mapVFTInfo.getPrintName(), vft->vtable[mapVFTInfo.index], hookFn), vft(vft), index(mapVFTInfo.index) { }

template<typename T>
void VFTHook<T>::place()
{
	if (!this->setupRan || isHooked())
	{
		return;
	}

	LM_VmtHook(vft.get(), index, this->hookFn.address);

	this->hooked = true;

	LOG_DEBUG
	(
		"VFT hooked %s (0x%x) with hook at 0x%x\n",
		this->name.c_str(),
		this->originalFn.address,
		this->hookFn.address
	);
}

template<typename T>
void VFTHook<T>::remove()
{
	if (!isHooked())
	{
		return;
	}

	LM_VmtUnhook(vft.get(), index);
	this->hooked = false;

	LOG_DEBUG("Unhooked %s!\n", this->name.c_str());
}

std::unordered_map<int, LuaHook*> LuaHook::hooks = std::unordered_map<int, LuaHook*>();

LuaHook::LuaHook(const char* name, const Lua::Ptr_t targetFn)
	:
	name(name),
	fn(reinterpret_cast<lm_address_t>(targetFn)),
	tramp(0),
	size(0)
{
	for (int i = 0; ; i++)
	{
		if (!hooks.contains(i))
		{
			idx = i;
			break;
		}
	}

	hooks[idx] = this;
	LOG_DEBUG("Created LuaHook %s (%i) from %p\n", name, idx, targetFn);
}

LuaHook::~LuaHook()
{
	remove();
	hooks.erase(idx);
}

Lua::Ptr_t LuaHook::place()
{
	if (fn == LM_ADDRESS_BAD || hookFn == LM_ADDRESS_BAD)
	{
		LOG_ERROR("Failed to place LuaHook %s, targetFn is LM_ADDRESS_BAD\n", name.c_str());
		return nullptr;
	}

	size = LM_HookCode(fn, hookFn, &tramp);
	if (!size)
	{
		return nullptr;
	}

	MemHlp::fixPICThunkCall(name.c_str(), fn, tramp);
	LOG_DEBUG("Placed LuaHook %s from 0x%x to 0x%x\n", name.c_str(), fn, hookFn);
	return reinterpret_cast<void*>(tramp);
}

bool LuaHook::remove()
{
	if (!size)
	{
		return false;
	}

	LM_UnhookCode(fn, tramp, size);
	size = 0;
	LOG_DEBUG("Removed LuaHook %s\n", name.c_str());
	return true;
}

__attribute__((hot))
static void hkTraceIPC(const char* iface, const char* fn)
{
	if (g_config.extendedLogging.copy())
	{
		LOG_DEBUG
		(
			"%s(%s, %s)\n",

			Hooks::TraceIPC->name.c_str(),
			iface,
			fn
		);
	}

	LOG_TRACE("Calling tramp\n");
	Hooks::TraceIPC->tramp.fn(iface, fn);
}

static uint32_t hkAPIJob_SendAndRecv(CAPIJob* pAPIJob, CProtoBufMsgBase* send, uint32_t a2, uint32_t timeOut, CProtoBufMsgBase* recv, EMsg targetType)
{
	uint32_t ret = Achievements::sendAndRecvGetUserStats(pAPIJob, send, timeOut, recv, targetType);

	if (!ret)
	{
		LOG_TRACE("Calling tramp\n");
		ret = Hooks::CAPIJob_SendAndRecv->tramp.fn(pAPIJob, send, a2, timeOut, recv, targetType);
	}

	LOG_DEBUG
	(
		"%s(%p, %s, %u, %u, %s, %u) -> %u\n",

		Hooks::CAPIJob_SendAndRecv->name.c_str(),
		reinterpret_cast<void*>(pAPIJob),
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
	LOG_TRACE("Calling tramp\n");
	const uint32_t ret = Hooks::CAppDataCache_BParseResponseFromMessage->tramp.fn(pAppDataCache, pMsg);

	LOG_DEBUG
	(
		"%s(%p, %p) -> %i\n",
		Hooks::CAppDataCache_BParseResponseFromMessage->name.c_str(),
		reinterpret_cast<void*>(pAppDataCache),
		reinterpret_cast<void*>(pMsg),
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
		LOG_TRACE("Calling tramp\n");
		ret = Hooks::CClientUnifiedServiceMethod_SendAndRecvMsg->tramp.fn(pUnifiedServiceTransport, name, send, recv, arg4);
	}

	Apps::sendAndRecvLastPlayedTimes(name, reinterpret_cast<CPlayer_GetLastPlayedTimes_Response*>(recv));

	LOG_DEBUG
	(
		"%s(%p, %s, %s, %s, %p) -> %u\n",

		Hooks::CClientUnifiedServiceMethod_SendAndRecvMsg->name.c_str(),
		reinterpret_cast<void*>(pUnifiedServiceTransport),
		name,
		MemHlp::getTypeName(send),
		MemHlp::getTypeName(recv),
		arg4,
		ret
	);

	return ret;
}

static void hkCMInterface_RecvPkt(CCMInterface* pCMInterface, CNetPacket* pNetPacket)
{
	LOG_DEBUG
	(
		"RecvPkt with CMInterface at %p %s -> 0x%x (refs %u, body %p, originalBody %p)\n",
		reinterpret_cast<void*>(pCMInterface),
		pNetPacket->getProtoBufTypeName().c_str(),
		pNetPacket->getType(),
		pNetPacket->refs,
		reinterpret_cast<void*>(pNetPacket->body),
		reinterpret_cast<void*>(pNetPacket->originalBody)
	);

	if (pNetPacket->isValid() && pNetPacket->isProtoBuf())
	{
		const uint32_t type = pNetPacket->getProtoBufType();

		//Short observation reveals only one CMInterface, but just to be sure
		if (!g_pCMInterface && type == k_EMsgClientLogOnResponse)
		{
			g_pCMInterface = reinterpret_cast<CCMInterface*>(pCMInterface);
		}

		const auto header = pNetPacket->deserializeHeader();
		const bool disableFamilyShareLock = g_config.disableFamilyLock.copy();

		if (disableFamilyShareLock && type == k_EMsgClientSharedLibraryStopPlaying)
		{
			LOG_DEBUG("Choking %s\n", pNetPacket->getProtoBufTypeName().c_str());
			pNetPacket->free();
			return;
		}

		if
		(
			disableFamilyShareLock
			&& type == k_EMsgServiceMethod
			&& header.has_target_job_name() //Do not modify header by blindly requesting the target_job_name
			&& header.target_job_name() == "FamilyGroupsClient.NotifyRunningApps#1"
		)
		{
			LOG_DEBUG("Choking %s\n", header.target_job_name().c_str());
			pNetPacket->free();
			return;
		}

		Lua::fireCallback(Lua::Callbacks::Network_RecvPkt, pNetPacket);

		Misc::recvMsg(pNetPacket);
		Ticket::recvMsg(pNetPacket);
	}

	LOG_TRACE("Calling tramp\n");
	Hooks::CCMInterface_RecvPkt->tramp.fn(pCMInterface, pNetPacket);
}

//I don't like forward declerations, but with the current style & hooks layout it's a necessity
static CSteamId hkClientUser_GetSteamId(const CSteamId& steamId);

static uint32_t hkSteamEngine_ProcessIPCFrame(CSteamEngine* pSteamEngine, HSteamPipe hPipe, CUtlBuffer* pBufIn, CUtlBuffer* pBufOut)
{
	if (!g_pSteamEngine)
	{
		g_pSteamEngine = reinterpret_cast<CSteamEngine*>(pSteamEngine);
		LOG_DEBUG("g_pSteamEngine at %p\n", reinterpret_cast<void*>(g_pSteamEngine));
	}

	uint32_t ret;
	const bool log = g_config.extendedLogging.copy();

	//pBufIn
	//mem + 0 : 1 = IPCCommand

	const EIPCCmd cmd = *reinterpret_cast<EIPCCmd*>(pBufIn->mem.base + 0);

	if (log)
	{
		LOG_DEBUG("ProcessIPCFrame 0x%x -> %s\n", hPipe, EIPCCmd_ToString(cmd).c_str());
	}

	if (cmd == EIPCCmd::RunInterface)
	{
		//We do not initialize with the CSteamEngine because first run CUser is null
		Hooks::placeVFTHooks();

		//While hooking this function to replace the other hooks might seem attractive
		//we do not do so. Many calls straight up bypass the IPC layer and go
		//straight for the original VFT implementations (IClientAppManager comes to mind)
		//Although it's a great spot to quickly test things

		//pBufIn
		//mem + 1 : 1 = interfaceType
		//mem + 2 : 4 = hSteamUser
		//mem + 6 : 4 = function Id
		//arguments follow
		//then fencepost?
		
		const EIPCInterface interface = *reinterpret_cast<EIPCInterface*>(pBufIn->mem.base + 1);
		const uint32_t function = *reinterpret_cast<uint32_t*>(pBufIn->mem.base + 6);

		if (log)
		{
			const auto utils = g_pSteamEngine->getUtils();
			LOG_DEBUG
			(
				"RunInterface %s 0x%x for %u (%u)\n",

				EIPCInterface_ToString(interface).c_str(),
				function,
				FakeAppIds::getRealAppIdForCurrentPipe(),
				utils ? utils->getAppId() : 0
			);
		}

		FakeAppIds::runIPCFrame(false, interface);

		LOG_TRACE("Calling tramp\n");
		ret = Hooks::CSteamEngine_ProcessIPCFrame->tramp.fn(pSteamEngine, hPipe, pBufIn, pBufOut);

		//LOG_DEBUG("In\n%s\n", MemHlp::hexdump(pBufIn->mem.base, pBufIn->offset).c_str());

		FakeAppIds::runIPCFrame(true, interface);

		//pBufOut
		//mem + 0 : 1 = EIPCExitCode
		//return values follow
		const EIPCExitCode exitCode = *reinterpret_cast<EIPCExitCode*>(pBufOut->mem.base + 0);

		//IClientUser::GetSteamID has been optimized to hell and back
		//So to hook it we need a naked function hook that requires quite the
		//complex logic to get the full steamId. So I made an exception for this function,
		//since it seems to always get called from RunInterface anyway
		//53                                      push    ebx
		//8B 54 24 0C                             mov     edx, [esp+4+arg_4]
		//8B 44 24 08                             mov     eax, [esp+4+arg_0]
		//8B 9A B2 E8 FF FF                       mov     ebx, [edx-174Eh] //SteamId low
		//8B 8A AE E8 FF FF                       mov     ecx, [edx-1752h] //SteamId high
		//89 58 04                                mov     [eax+4], ebx
		//89 08                                   mov     [eax], ecx
		//                                        //Optimally inject here, grab eax, copy into g_currentSteamId
		//5B                                      pop     ebx
		//C2 04 00                                retn    4
		if (interface == EIPCInterface::User && exitCode == EIPCExitCode::Success && function == 0xD6FC3200)
		{
			//Universe always set, steamId gets filled in after login
			CSteamId* id = reinterpret_cast<CSteamId*>(pBufOut->mem.base + 1);

			if (!g_currentSteamId.isSet() && id->isSet())
			{
				g_currentSteamId = CSteamId(id->steamId64);
			}

			*id = hkClientUser_GetSteamId(g_currentSteamId);
		}

		//LOG_DEBUG("Out\n%s\n", MemHlp::hexdump(pBufOut->mem.base, pBufOut->offset).c_str());

		Apps::runIPCFrame();
		SLSAPI::runIPCFrame();
	}
	else
	{
		LOG_TRACE("Calling tramp\n");
		ret = Hooks::CSteamEngine_ProcessIPCFrame->tramp.fn(pSteamEngine, hPipe, pBufIn, pBufOut);
	}

	if (cmd == EIPCCmd::ConnectPipe)
	{
		const auto serverPipe = g_pSteamEngine->getServerPipe(hPipe);
		auto& proc = g_processMap[serverPipe->pipeHandle];

		if (proc.init(serverPipe->pid, serverPipe->pipeHandle))
		{
			Ticket::connectPipe(hPipe);
		}
	}

	if (cmd == EIPCCmd::ClosePipe)
	{
		if (g_processMap.contains(hPipe))
		{
			LOG_DEBUG("Deleting g_processMap mapping %u for 0x%x\n", g_processMap.at(hPipe).appId, hPipe);
			g_processMap.erase(hPipe);
		}
	}

	//LOG_DEBUG("In\n%s\n", MemHlp::hexdump(pBufIn->mem.base, pBufIn->offset).c_str());
	//LOG_DEBUG("Out\n%s\n", MemHlp::hexdump(pBufOut->mem.base, pBufOut->offset).c_str());

	return ret;
}

static AppId_t hkSteamEngine_SetAppIdForCurrentPipe(CSteamEngine* pSteamEngine, AppId_t appId, bool a2)
{
	FakeAppIds::setAppIdForCurrentPipe(appId);

	LOG_TRACE("Calling tramp\n");
	const AppId_t ret = Hooks::CSteamEngine_SetAppIdForCurrentPipe->tramp.fn(pSteamEngine, appId, a2);

	LOG_DEBUG
	(
		"%s(%p, %u, %i) -> %i\n",

		Hooks::CSteamEngine_SetAppIdForCurrentPipe->name.c_str(),
		reinterpret_cast<void*>(pSteamEngine),
		appId,
		a2,
		ret
	);

	return ret;
}

static bool hkWebSocketConnection_BBuildAndAsyncSendFrame(CWebSocketConnection* pWebSocketConnection, EWebSocketConnectionSendType type, void* pData, uint32_t dataSize)
{
	if (type == k_EWebSocketConnectionSendRaw)
	{
		//Freeing pData royally fucks up memory since
		//it gets freed sometime after the function returns
		//(going up the callstack around 5 times)
		CNetPacket packet {};
		packet.body = reinterpret_cast<CNetPacketBody*>(Steam::Plat_Alloc(dataSize));

		if (packet.body)
		{
			memcpy(packet.body, pData, dataSize);
			packet.size = dataSize;

			LOG_DEBUG
			(
				"SendPkt with CWebSocketConnection at %p %s -> 0x%x\n",
				reinterpret_cast<void*>(pWebSocketConnection),
				packet.getProtoBufTypeName().c_str(),
				packet.getType()
			);

			if (packet.isValid() && packet.isProtoBuf())
			{
				//Multiple sockets in use, we grab the main one
				if (!g_pWebSocketConnection && packet.getProtoBufType() == k_EMsgClientLogon)
				{
					g_pWebSocketConnection = reinterpret_cast<CWebSocketConnection*>(pWebSocketConnection);
				}

				Lua::fireCallback(Lua::Callbacks::Network_SendPkt, &packet);

				Apps::sendMsg(&packet);
				FakeAppIds::sendMsg(&packet);
			}

			LOG_TRACE("Calling tramp\n");
			const bool success = Hooks::CWebSocketConnection_BBuildAndAsyncSendFrame->tramp.fn(pWebSocketConnection, type, packet.body, packet.size);
			packet.free();
			return success;
		}
	}

	LOG_TRACE("Calling tramp\n");
	return Hooks::CWebSocketConnection_BBuildAndAsyncSendFrame->tramp.fn(pWebSocketConnection, type, pData, dataSize);
}

static gameserverdetails_t* hkSteamMatchmakingServers_GetServerDetails(void* pSteamMatchmakingServers, uint32_t handle, uint32_t serverIdx)
{
	LOG_TRACE("Calling tramp\n");
	gameserverdetails_t* ret = Hooks::CSteamMatchmakingServers_GetServerDetails->tramp.fn(pSteamMatchmakingServers, handle, serverIdx);

	LOG_DEBUG
	(
		"%s(%p, 0x%x, %u) -> %p\n",

		Hooks::CSteamMatchmakingServers_GetServerDetails->name.c_str(),
		pSteamMatchmakingServers,
		handle,
		serverIdx,
		reinterpret_cast<void*>(ret)
	);

	if (ret)
	{
		FakeAppIds::getServerDetails(handle, *ret);
	}

	return ret;
}

static uint32_t hkSteamMatchmakingServers_RequestInternetServerList(void* pSteamMatchmakingServers, AppId_t appId, uint32_t a2, uint32_t a3, uint32_t a4)
{
	const uint32_t fake = FakeAppIds::requestInternetServerList(appId);

	LOG_TRACE("Calling tramp\n");
	uint32_t handle = Hooks::CSteamMatchmakingServers_RequestInternetServerList->tramp.fn(pSteamMatchmakingServers, fake ? fake : appId, a2, a3, a4);

	LOG_DEBUG
	(
		"%s(%p, %u, 0x%x, 0x%x, 0x%x)->0x%x\n",

		Hooks::CSteamMatchmakingServers_RequestInternetServerList->name.c_str(),
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
static uint32_t hkUser_CheckAppOwnership(CUser* pUser, AppId_t appId, AppOwnershipInfo_t* pOwnershipInfo)
{
	LOG_TRACE("Calling tramp\n");
	const uint32_t ret = Hooks::CUser_CheckAppOwnership->tramp.fn(pUser, appId, pOwnershipInfo);

	//Do not log pOwnershipInfo because it gets deleted very quickly, so it's pretty much useless in the logs
	LOG_ONCE
	(
		"%s(%p, %u) -> %i\n",

		Hooks::CUser_CheckAppOwnership->name.c_str(),
		reinterpret_cast<void*>(pUser),
		appId,
		ret
	);

	if (Apps::checkAppOwnership(appId, pOwnershipInfo) || DLC::checkAppOwnership(appId, pOwnershipInfo))
	{
		return true;
	}

	return ret;
}

static uint32_t hkUser_GetSubscribedApps(CUser* pUser, AppId_t* pAppList, uint32_t size, uint8_t a3)
{
	LOG_TRACE("Calling tramp\n");
	uint32_t count = Hooks::CUser_GetSubscribedApps->tramp.fn(pUser, pAppList, size, a3);

	Apps::getSubscribedApps(pAppList, size, count);

	LOG_DEBUG
	(
		"%s(%p, %p, %i, %i) -> %i\n",

		Hooks::CUser_GetSubscribedApps->name.c_str(),
		reinterpret_cast<void*>(pUser),
		reinterpret_cast<void*>(pAppList),
		size,
		a3,
		count
	);

	return count;
}

static uint32_t hkUser_PostCallbackToAppId(CUser* pUser, AppId_t appId, uint32_t type, void* pCallback, uint32_t callbackSize)
{
	const AppId_t fakeAppId = FakeAppIds::getFakeAppId(appId);
	if (fakeAppId)
	{
		LOG_DEBUG("Rerouting callback from %u to %u\n", appId, fakeAppId);
		appId = fakeAppId;
	}

	LOG_TRACE("Calling tramp\n");
	const uint32_t ret = Hooks::CUser_PostCallbackToAppId->tramp.fn(pUser, appId, type, pCallback, callbackSize);

	LOG_DEBUG
	(
		"%s(%p, %u, %u, %p, %u) -> %u\n",

		Hooks::CUser_PostCallbackToAppId->name.c_str(),
		reinterpret_cast<void*>(pUser),
		appId,
		type,
		pCallback,
		callbackSize,
		ret
	);

	return ret;
}

static uint32_t hkUser_SpawnGameId
(
	void* pUser,
	const char* exe,
	const char* cmd,
	const char* workingDir,
	GameId_t* pGameId,
	const char* title,
	int32_t a6,
	int32_t a7,
	int32_t a8,
	int32_t a9,
	void* a10
)
{
	std::string newExe = exe;
	std::string newCmd = cmd;

	Apps::spawnGame(pGameId, newExe, newCmd);

	LOG_DEBUG("Launching gameId %llu with cmd %s as %s\n", *pGameId, newExe.c_str(), newCmd.c_str());

	const uint32_t ret = Hooks::CUser_SpawnGameId->tramp.fn
	(
		pUser,
		newExe.c_str(),
		newCmd.c_str(),
		workingDir,
		pGameId,
		title,
		a6,
		a7,
		a8,
		a9,
		a10
	);

	return ret;
}

static bool hkUserAppManager_BuildDepotDependency
(
	IClientAppManager* pAppManager,
	AppId_t appId,
	void* a2,
	CUtlVector<DepotInfo_t>* depots,
	CUtlVector<DepotInfo_t>* sharedDepots,
	void* a5,
	uint32_t* pBuildId,
	bool* a7
)
{
	LOG_TRACE("Calling tramp\n");
	const bool success = Hooks::CUserAppManager_BuildDepotDependency->tramp.fn(pAppManager, appId, a2, depots, sharedDepots, a5, pBuildId, a7);

	LOG_DEBUG
	(
		"%s(%p, %u) -> %i\n",
		Hooks::CUserAppManager_BuildDepotDependency->name.c_str(),
		reinterpret_cast<void*>(pAppManager),
		appId,
		success
	);

	Apps::buildDepotDependency(depots, sharedDepots);

	return success;
}

static bool hkClientAppManager_BCanRemotePlayTogether(IClientAppManager* pAppManager, AppId_t appId)
{
	LOG_TRACE("Calling original\n");
	const bool ret = Hooks::IClientAppManager_BCanRemotePlayTogether->originalFn.fn(pAppManager, appId);

	LOG_DEBUG
	(
		"%s(%p, %u) -> %u\n",
		Hooks::IClientAppManager_BCanRemotePlayTogether->name.c_str(),
		reinterpret_cast<void*>(pAppManager),
		appId,
		ret
	);

	return true;
}

static void* hkClientAppManager_LaunchApp(IClientAppManager* pAppManager, AppId_t* pAppId, void* a2, void* a3, void* a4)
{
	if (pAppId)
	{
		LOG_ONCE
		(
			"%s(%p, %u, %p, %p, %p)\n",

			Hooks::IClientAppManager_LaunchApp->name.c_str(),
			reinterpret_cast<void*>(pAppManager),
			*pAppId,
			a2,
			a3,
			a4
		);

		Ticket::launchApp(*pAppId);
	}

	LOG_TRACE("Calling original\n");
	//Do not do anything in post! Otherwise App launching will break
	return Hooks::IClientAppManager_LaunchApp->originalFn.fn(pAppManager, pAppId, a2, a3, a4);
}

static bool hkClientAppManager_IsAppDlcInstalled(IClientAppManager* pAppManager, AppId_t appId, AppId_t dlcId)
{
	LOG_TRACE("Calling original\n");
	const bool ret = Hooks::IClientAppManager_IsAppDlcInstalled->originalFn.fn(pAppManager, appId, dlcId);

	LOG_ONCE
	(
		"%s(%p, %u, %u) -> %i\n",

		Hooks::IClientAppManager_IsAppDlcInstalled->name.c_str(),
		reinterpret_cast<void*>(pAppManager),
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

static bool hkClientAppManager_BIsDlcEnabled(IClientAppManager* pAppManager, AppId_t appId, AppId_t dlcId, void* a3)
{
	LOG_TRACE("Calling original\n");

	const bool ret = Hooks::IClientAppManager_BIsDlcEnabled->originalFn.fn(pAppManager, appId, dlcId, a3);
	LOG_ONCE
	(
		"%s(%p, %u, %u, %p) -> %i\n",

		Hooks::IClientAppManager_BIsDlcEnabled->name.c_str(),
		reinterpret_cast<void*>(pAppManager),
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

static bool hkClientAppManager_GetUpdateInfo(IClientAppManager* pAppManager, AppId_t appId, uint32_t* a2)
{
	LOG_TRACE("Calling original\n");

	const bool success = Hooks::IClientAppManager_GetAppUpdateInfo->originalFn.fn(pAppManager, appId, a2);
	LOG_ONCE
	(
		"%s(%p, %u, %p) -> %i\n",
		Hooks::IClientAppManager_GetAppUpdateInfo->name.c_str(),
		reinterpret_cast<void*>(pAppManager),
		appId,
		reinterpret_cast<void*>(a2),
		success
	);

	if (Apps::shouldDisableUpdates(appId))
	{
		LOG_ONCE("Disabled updates for %u\n", appId);
		return false;
	}

	return success;
}

static bool hkClientAppManager_GetAppStateInfo(IClientAppManager* pAppManager, AppId_t appId, AppStateInfo_t* pInfo)
{
	const bool success = Hooks::IClientAppManager_GetAppStateInfo->originalFn.fn(pAppManager, appId, pInfo);

	if (success)
	{
		Apps::getAppStateInfo(appId, pInfo);
	}

	return success;
}

static unsigned int hkClientApps_GetDLCCount(IClientApps* pClientApps, AppId_t appId)
{
	LOG_TRACE("Calling original\n");

	uint32_t count = Hooks::IClientApps_GetDLCCount->originalFn.fn(pClientApps, appId);
	LOG_ONCE
	(
		"%s(%p, %u) -> %u\n",

		Hooks::IClientApps_GetDLCCount->name.c_str(),
		reinterpret_cast<void*>(pClientApps),
		appId,
		count
	);
	
	const uint32_t override = DLC::getDlcCount(appId);
	if (override)
	{
		return override;
	}

	return count;
}

static bool hkClientApps_GetDLCDataByIndex(IClientApps* pClientApps, AppId_t appId, int dlcIndex, AppId_t* pDlcId, bool* pIsAvailable, char* pChDlcName, size_t dlcNameLen)
{
	LOG_TRACE("Calling original\n");
	//Preserve original call to populate stuff
	const bool ret = DLC::getDlcDataByIndex(appId, dlcIndex, pDlcId, pIsAvailable, pChDlcName, dlcNameLen)
		|| Hooks::IClientApps_GetDLCDataByIndex->originalFn.fn(pClientApps, appId, dlcIndex, pDlcId, pIsAvailable, pChDlcName, dlcNameLen);


	LOG_ONCE
	(
		"%s(%p, %u, %i, %p, %p, %s, %i) -> %i\n",

		Hooks::IClientApps_GetDLCDataByIndex->name.c_str(),
		reinterpret_cast<void*>(pClientApps),
		appId,
		dlcIndex,
		reinterpret_cast<void*>(pDlcId),
		reinterpret_cast<void*>(pIsAvailable),
		pChDlcName,
		dlcNameLen,
		ret
	);

	return ret;
}

static uint32_t hkClientFriends_GetFriendGamePlayed(void* pClientFriends, uint64_t steamId, GamePlayed_t* gamePlayed)
{
	LOG_TRACE("Calling original\n");
	const uint32_t ret = Hooks::IClientFriends_GetFriendGamePlayed->tramp.fn(pClientFriends, steamId, gamePlayed);

	const AppId_t realAppId = FakeAppIds::getRealAppIdForCurrentPipe();
	const AppId_t fakeAppId = FakeAppIds::getFakeAppId(realAppId);

	if (fakeAppId && fakeAppId == gamePlayed->appId)
	{
		LOG_DEBUG("Set friend GamePlayed from %u to %u\n", gamePlayed->appId, realAppId);
		gamePlayed->appId = realAppId;
	}

	//We do not log this function, it's basically useless since we don't want any SteamIds in the logs
	
	return ret;
}

static bool hkClientRemoteStorage_IsCloudEnabledForApp(void* pClientRemoteStorage, AppId_t appId)
{
	LOG_TRACE("Calling tramp\n");
	const bool enabled = Hooks::IClientRemoteStorage_IsCloudEnabledForApp->tramp.fn(pClientRemoteStorage, appId);
	LOG_ONCE
	(
		"%s(%p, %u) -> %i\n",

		Hooks::IClientRemoteStorage_IsCloudEnabledForApp->name.c_str(),
		pClientRemoteStorage,
		appId,
		enabled
	);

	if (Apps::shouldDisableCloud(appId))
	{
		LOG_ONCE("Disabled cloud for %u\n", appId);
		return false;
	}

	return enabled;
}

static bool hkClientUser_BLoggedOn(IClientUser* pClientUser)
{
	LOG_TRACE("Calling original\n");
	const bool ret = Hooks::IClientUser_BLoggedOn->originalFn.fn(pClientUser);
	//Useless logging
	//LOG_DEBUG
	//(
	//	"%s(0x%x) -> %i\n",
	//	Hooks::IClientUser_BLoggedOn->name.c_str(),
	//	pClientUser,
	//	ret
	//);
	
	if (Misc::shouldFakeOffline())
	{
		return false;
	}

	return ret;
}

static uint32_t hkClientUser_BUpdateAppOwnershipTicket(IClientUser* pClientUser, AppId_t appId, bool staleOnly)
{
	const auto cached = Ticket::getCachedTicket(appId);
	if (g_pSteamEngine->getUser(0)->isSubscribed(appId) && !cached)
	{
		staleOnly = false;
		LOG_DEBUG("Force re-requesting OwnershipInfo for %u\n", appId);
	}

	LOG_TRACE("Calling original\n");
	const uint32_t ret = Hooks::IClientUser_BUpdateAppOwnershipTicket->originalFn.fn(pClientUser, appId, staleOnly);

	LOG_DEBUG
	(
		"%s(%p, %u, %i) -> %u\n",

		Hooks::IClientUser_BUpdateAppOwnershipTicket->name.c_str(),
		reinterpret_cast<void*>(pClientUser),
		appId,
		staleOnly,
		ret
	);

	return ret;
}

static uint32_t hkClientUser_GetAppOwnershipTicketExtendedData
(
	IClientUser* pClientUser,
	AppId_t appId,
	void* pTicket,
	uint32_t ticketSize,
	uint32_t* pOffAppId,
	uint32_t* pOffSteamId,
	uint32_t* pOffSig,
	uint32_t* pSigSize
)
{
	LOG_TRACE("Calling original\n");
	const uint32_t size = Hooks::IClientUser_GetAppOwnershipTicketExtendedData->originalFn.fn
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

	LOG_ONCE("%s(%u)->%u\n", Hooks::IClientUser_GetAppOwnershipTicketExtendedData->name.c_str(), appId, size);

	if (size)
	{
		Ticket::getTicketOwnershipExtendedData(appId);
	}

	return size;
}

static bool hkClientUser_GetEncryptedAppTicket(IClientUser* pClientUser, void* pTicket, uint32_t ticketSize, uint32_t* pTicketSize)
{
	LOG_TRACE("Calling original\n");
	const bool success = Hooks::IClientUser_GetEncryptedAppTicket->originalFn.fn(pClientUser, pTicket, ticketSize, pTicketSize);

	LOG_DEBUG
	(
		"%s(%p, %p, %u, %p) -> %u\n",

		Hooks::IClientUser_GetEncryptedAppTicket->name.c_str(),
		reinterpret_cast<void*>(pClientUser),
		pTicket,
		ticketSize,
		reinterpret_cast<void*>(pTicketSize),
		success
	);

	if (success)
	{
		Ticket::getEncryptedAppTicket(FakeAppIds::getRealAppIdForCurrentPipe());
	}

	return success;
}

static bool hkClientUser_GetLegacyCDKey(IClientUser* pClientUser, AppId_t appId, char* pChKey, uint32_t keySize)
{
	Apps::getLegacyCDKey(appId);

	return Hooks::IClientUser_GetLegacyCDKey->originalFn.fn(pClientUser, appId, pChKey, keySize);
}

static uint8_t hkClientUser_IsUserSubscribedAppInTicket(IClientUser* pClientUser, uint64_t steamId, AppId_t appId)
{
	LOG_TRACE("Calling original\n");
	const uint8_t ticketState = Hooks::IClientUser_IsUserSubscribedAppInTicket->originalFn.fn(pClientUser, steamId, appId);
	//LOG_ONCE("IClientUser::IsUserSubscribedAppInTicket(0x%x, %u, %u, %u, %u) -> %i\n", pClientUser, steamId, a2, a3, appId, ticketState);
	//Don't log the steamId, protect users from themselves and stuff
	LOG_ONCE
	(
		"%s(%p, %u) -> %i\n",

		Hooks::IClientUser_IsUserSubscribedAppInTicket->name.c_str(),
		reinterpret_cast<void*>(pClientUser),
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

static CSteamId hkClientUser_GetSteamId(const CSteamId& steamId)
{
	const auto utils = g_pSteamEngine->getUtils();
	if (!utils)
	{
		return steamId;
	}

	//Never spoof inside the Steamclient
	const AppId_t realAppId = FakeAppIds::getRealAppIdForCurrentPipe();
	if (!realAppId)
	{
		return steamId;
	}

	const auto overrides = g_config.steamIdOverride.get();
	if (overrides->contains(realAppId))
	{
		const uint64_t& id64 = overrides->at(realAppId);
		if (id64)
		{
			return CSteamId(id64);
		}

		const auto cached = Ticket::getCachedTicket(realAppId);
		if (cached)
		{
			return cached->steamId;
		}

		LOG_ONCE
		(
			"SteamIdOverride for %u is set with automatic mode, but no AppOwnershipTicket exists in cache! Falling through to normal operation\n",
			realAppId
		);
	}

	if (Ticket::oneTimeSteamIdSpoof.contains(realAppId))
	{
		const CSteamId newId = Ticket::oneTimeSteamIdSpoof.at(realAppId);
		Ticket::oneTimeSteamIdSpoof.erase(realAppId);

		return newId;
	}

	const auto ticket = Ticket::getCachedEncryptedTicket(realAppId);
	if (!ticket)
	{
		return steamId;
	}

	if (g_config.smartTickets.copy() & CConfig::k_ESmartTicketsDenuvo)
	{
		const auto& proc = g_processMap.at(utils->getCurrentSteamPipe());

		if (!proc.denuvo)
		{
			return steamId;
		}
	}

	return ticket->steamId;
}

static AppId_t hkClientUtils_GetAppId(IClientUtils* pClientUtils)
{
	LOG_TRACE("Calling original\n");
	AppId_t appId = Hooks::IClientUtils_GetAppId->originalFn.fn(pClientUtils);

	LOG_DEBUG
	(
		"%s(%p) -> %u\n",

		Hooks::IClientUtils_GetAppId->name.c_str(),
		reinterpret_cast<void*>(pClientUtils),
		appId
	);

	const AppId_t real = FakeAppIds::getRealAppIdForCurrentPipe(false);
	if (real)
	{
		LOG_DEBUG("Overwriting appId with %u\n", real);
		return real;
	}

	return appId;
}

static bool hkClientUtils_GetOfflineMode(IClientUtils* pClientUtils)
{
	LOG_TRACE("Calling original\n");
	const bool ret = Hooks::IClientUtils_GetOfflineMode->originalFn.fn(pClientUtils);

	if (Misc::shouldFakeOffline())
	{
		return true;
	}

	return ret;
}

static void hkCGameInfoDialog_ServerResponded(void* pSteamMatchingPingResponse, gameserverdetails_t* details)
{
	FakeAppIds::pingResponse(details);

	LOG_TRACE("Calling tramp\n");
	Hooks::CGameInfoDialog_ServerResponded->tramp.fn(pSteamMatchingPingResponse, details);

	LOG_DEBUG
	(
		"%s(%p, %p) for %u\n",
		Hooks::CGameInfoDialog_ServerResponded->name.c_str(),
		pSteamMatchingPingResponse,
		reinterpret_cast<void*>(details),
		details ? details->appId : 0
	);
}

static bool hkClientCompat_BIsCompatLayerEnabled(IClientCompat* pClientCompat)
{
	if (!g_pClientCompat)
	{
		g_pClientCompat = pClientCompat;
		LOG_DEBUG("g_pClientCompat at %p\n", reinterpret_cast<void*>(g_pClientCompat));
	}

	return Hooks::IClientCompat_BIsCompatLayerEnabled->tramp.fn(pClientCompat);
}

static bool hkClientConfigStore_SetString(void* pClientConfigStore, uint32_t store, const char* key, const char* value)
{
	LOG_TRACE("Calling tramp\n");
	const bool success = Hooks::IClientConfigStore_SetString->tramp.fn(pClientConfigStore, store, key, value);

	//LOG_DEBUG
	//(
	//	"%s(0x%x, %u, %s, %s) -> %u\n",

	//	Hooks::IClientConfigStore_SetString->name.c_str(),
	//	pClientConfigStore,
	//	store,
	//	key,
	//	value,
	//	success
	//);

	if (success)
	{
		Apps::setConfigStoreString(key, value);
	}

	return success;
}

namespace Hooks
{
	DetourHook<TraceIPC_t>* TraceIPC = nullptr;

	DetourHook<CAPIJob_SendAndRecv_t>* CAPIJob_SendAndRecv = nullptr;

	DetourHook<CAppDataCache_BParseResponseFromMessage_t>* CAppDataCache_BParseResponseFromMessage = nullptr;

	DetourHook<CClientUnifiedServiceMethod_SendAndRecvMsg_t>* CClientUnifiedServiceMethod_SendAndRecvMsg = nullptr;

	DetourHook<CCMInterface_RecvPkt_t>* CCMInterface_RecvPkt = nullptr;

	DetourHook<CSteamMatchmakingServers_GetServerDetails_t>* CSteamMatchmakingServers_GetServerDetails = nullptr;
	DetourHook<CSteamMatchmakingServers_RequestInternetServerList_t>* CSteamMatchmakingServers_RequestInternetServerList = nullptr;

	DetourHook<CSteamEngine_ProcessIPCFrame_t>* CSteamEngine_ProcessIPCFrame = nullptr;
	DetourHook<CSteamEngine_SetAppIdForCurrentPipe_t>* CSteamEngine_SetAppIdForCurrentPipe = nullptr;

	DetourHook<CUser_CheckAppOwnership_t>* CUser_CheckAppOwnership = nullptr;
	DetourHook<CUser_GetSubscribedApps_t>* CUser_GetSubscribedApps = nullptr;
	DetourHook<CUser_PostCallbackToAppId_t>* CUser_PostCallbackToAppId = nullptr;
	DetourHook<CUser_SpawnGameId_t>* CUser_SpawnGameId = nullptr;

	DetourHook<CUserAppManager_BuildDepotDependency_t>* CUserAppManager_BuildDepotDependency = nullptr;

	DetourHook<CWebSocketConnection_BBuildAndAsyncSendFrame_t>* CWebSocketConnection_BBuildAndAsyncSendFrame = nullptr;

	DetourHook<IClientCompat_BIsCompatLayerEnabled_t>* IClientCompat_BIsCompatLayerEnabled = nullptr;

	DetourHook<IClientConfigStore_SetString_t>* IClientConfigStore_SetString = nullptr;

	DetourHook<IClientFriends_GetFriendGamePlayed_t>* IClientFriends_GetFriendGamePlayed = nullptr;

	DetourHook<IClientRemoteStorage_IsCloudEnabledForApp_t>* IClientRemoteStorage_IsCloudEnabledForApp = nullptr;


	VFTHook<IClientAppManager_BCanRemotePlayTogether_t>* IClientAppManager_BCanRemotePlayTogether = nullptr;
	VFTHook<IClientAppManager_BIsDlcEnabled_t>* IClientAppManager_BIsDlcEnabled = nullptr;
	VFTHook<IClientAppManager_GetAppUpdateInfo_t>* IClientAppManager_GetAppUpdateInfo = nullptr;
	VFTHook<IClientAppManager_GetAppStateInfo_t>* IClientAppManager_GetAppStateInfo = nullptr;
	VFTHook<IClientAppManager_LaunchApp_t>* IClientAppManager_LaunchApp = nullptr;
	VFTHook<IClientAppManager_IsAppDlcInstalled_t>* IClientAppManager_IsAppDlcInstalled = nullptr;

	VFTHook<IClientApps_GetDLCDataByIndex_t>* IClientApps_GetDLCDataByIndex = nullptr;
	VFTHook<IClientApps_GetDLCCount_t>* IClientApps_GetDLCCount = nullptr;

	VFTHook<IClientUser_BLoggedOn_t>* IClientUser_BLoggedOn = nullptr;
	VFTHook<IClientUser_BUpdateAppOwnershipTicket_t>* IClientUser_BUpdateAppOwnershipTicket = nullptr;
	VFTHook<IClientUser_GetAppOwnershipTicketExtendedData_t>* IClientUser_GetAppOwnershipTicketExtendedData = nullptr;
	VFTHook<IClientUser_GetEncryptedAppTicket_t>* IClientUser_GetEncryptedAppTicket = nullptr;
	VFTHook<IClientUser_GetLegacyCDKey_t>* IClientUser_GetLegacyCDKey = nullptr;
	VFTHook<IClientUser_IsUserSubscribedAppInTicket_t>* IClientUser_IsUserSubscribedAppInTicket = nullptr;

	VFTHook<IClientUtils_GetAppId_t>* IClientUtils_GetAppId = nullptr;
	VFTHook<IClientUtils_GetOfflineMode_t>* IClientUtils_GetOfflineMode = nullptr;


	//steamui.so
	DetourHook<CGameInfoDialog_ServerResponded_t>* CGameInfoDialog_ServerResponded = nullptr;
}

bool Hooks::init()
{
	LOG_DEBUG("Hooks::init()\n");

	{
		const auto name = std::string("14CCompatManager");
		if (!Decompiler::vftables.contains(name))
		{
			LOG_ERROR("Failed to get %s VFTable!\n", name.c_str());
			return false;
		}

		auto& compatMan = Decompiler::vftables.at(name);

		IClientCompat_BIsCompatLayerEnabled = new DetourHook
		(
			VFTIndexes::IClientCompat::BIsCompatLayerEnabled.getPrintName().c_str(),
			compatMan.functions[VFTIndexes::IClientCompat::BIsCompatLayerEnabled.index],
			hkClientCompat_BIsCompatLayerEnabled
		);
	}

	{
		const auto name = std::string("12CConfigStore");
		if (!Decompiler::vftables.contains(name))
		{
			LOG_ERROR("Failed to get %s VFTable!\n", name.c_str());
			return false;
		}

		auto& store = Decompiler::vftables.at(name);

		IClientConfigStore_SetString = new DetourHook
		(
			VFTIndexes::IClientConfigStore::SetString.getPrintName().c_str(),
			store.functions[VFTIndexes::IClientConfigStore::SetString.index],
			hkClientConfigStore_SetString
		);
	}

	{
		const auto name = std::string("12CUserFriends");
		if (!Decompiler::vftables.contains(name))
		{
			LOG_ERROR("Failed to get %s VFTable!\n", name.c_str());
			return false;
		}

		auto& friends = Decompiler::vftables.at(name);

		IClientFriends_GetFriendGamePlayed = new DetourHook
		(
			VFTIndexes::IClientFriends::GetFriendGamePlayed.getPrintName().c_str(),
			friends.functions[VFTIndexes::IClientFriends::GetFriendGamePlayed.index],
			hkClientFriends_GetFriendGamePlayed
		);
	}

	{
		const auto name = std::string("18CUserRemoteStorage");
		if (!Decompiler::vftables.contains(name))
		{
			LOG_ERROR("Failed to get %s VFTable!\n", name.c_str());
			return false;
		}

		auto& storage = Decompiler::vftables.at(name);

		//We detourhook because the vftable seems to get relocated, so at this point in time
		//the pointers are all wrong and would need manual adjustment which breaks
		//current assumptions by VFTHook<T>
		IClientRemoteStorage_IsCloudEnabledForApp = new DetourHook
		(
			VFTIndexes::IClientRemoteStorage::IsCloudEnabledForApp.getPrintName().c_str(),
			storage.functions[VFTIndexes::IClientRemoteStorage::IsCloudEnabledForApp.index],
			hkClientRemoteStorage_IsCloudEnabledForApp
		);
	}

	TraceIPC = new DetourHook(Patterns::TraceIPC, hkTraceIPC);

	CAPIJob_SendAndRecv = new DetourHook(Patterns::CAPIJob::SendAndRecv, hkAPIJob_SendAndRecv);

	CAppDataCache_BParseResponseFromMessage = new DetourHook(Patterns::CAppDataCache::BParseResponseMessage, hkAppDataCache_BParseResponseFromMessage);

	CClientUnifiedServiceMethod_SendAndRecvMsg = new DetourHook(VFTIndexes::CClientUnifiedServiceTransport::SendAndRecvMsg, hkClientUnifiedServiceTransport_SendAndRecvMsg);

	CCMInterface_RecvPkt = new DetourHook(VFTIndexes::CCMInterface::RecvPkt, hkCMInterface_RecvPkt);

	CSteamMatchmakingServers_GetServerDetails = new DetourHook(VFTIndexes::CSteamMatchmakingServers::GetServerDetails, hkSteamMatchmakingServers_GetServerDetails);
	CSteamMatchmakingServers_RequestInternetServerList = new DetourHook(VFTIndexes::CSteamMatchmakingServers::RequestInternetServerList, hkSteamMatchmakingServers_RequestInternetServerList);

	CSteamEngine_ProcessIPCFrame = new DetourHook(Patterns::CSteamEngine::ProcessIPCFrame, hkSteamEngine_ProcessIPCFrame);
	CSteamEngine_SetAppIdForCurrentPipe = new DetourHook(Patterns::CSteamEngine::SetAppIdForCurrentPipe, hkSteamEngine_SetAppIdForCurrentPipe);

	CUser_CheckAppOwnership = new DetourHook(Patterns::CUser::CheckAppOwnership, hkUser_CheckAppOwnership);
	CUser_GetSubscribedApps = new DetourHook(Patterns::CUser::GetSubscribedApps, hkUser_GetSubscribedApps);
	CUser_PostCallbackToAppId = new DetourHook(Patterns::CUser::PostCallbackToAppId, hkUser_PostCallbackToAppId);
	CUser_SpawnGameId = new DetourHook(Patterns::CUser::SpawnGameId, hkUser_SpawnGameId);

	CUserAppManager_BuildDepotDependency = new DetourHook(Patterns::CUserAppManager::BuildDepotDependency, hkUserAppManager_BuildDepotDependency);

	CWebSocketConnection_BBuildAndAsyncSendFrame = new DetourHook(Patterns::CWebSocketConnection::BBuildAndAsyncSendFrame, hkWebSocketConnection_BBuildAndAsyncSendFrame);

	CGameInfoDialog_ServerResponded = new DetourHook(VFTIndexes::CGameInfoDialog::ServerResponded, hkCGameInfoDialog_ServerResponded);

	setupAll();
	placeAll();

	return true;
}

void Hooks::setupAll()
{
	LOG_DEBUG("Hooks::setupAll()\n");

	for (auto hook : IHook::hooks)
	{
		hook->setup();
	}
}

void Hooks::placeAll()
{
	LOG_DEBUG("Hooks::placeAll()\n");

	for (auto hook : IHook::hooks)
	{
		hook->place();
	}
}

void Hooks::placeVFTHooks()
{
	//Most of the steamclient runs in a single thread
	//But just in case things change (they probably won't)
	static std::mutex mutex;
	std::lock_guard guard(mutex);

	static bool hooked = false;
	if (hooked)
	{
		return;
	}

	const auto usr = g_pSteamEngine->getUser();
	if (!usr)
	{
		return;
	}

	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Waddress-of-packed-member"

	LOG_DEBUG("CUser at %p\n", reinterpret_cast<void*>(usr));

	{
		const auto appManager = usr->getAppManager();
		LOG_DEBUG("CUserAppManager at %p\n", reinterpret_cast<void*>(appManager));

		auto vft = std::make_shared<lm_vmt_t>();
		LM_VmtNew(*reinterpret_cast<lm_address_t**>(appManager), vft.get());

		Hooks::IClientAppManager_BCanRemotePlayTogether = new VFTHook(vft, VFTIndexes::IClientAppManager::BCanRemotePlayTogether, hkClientAppManager_BCanRemotePlayTogether);
		Hooks::IClientAppManager_BIsDlcEnabled = new VFTHook(vft, VFTIndexes::IClientAppManager::BIsDlcEnabled, hkClientAppManager_BIsDlcEnabled);
		Hooks::IClientAppManager_GetAppUpdateInfo = new VFTHook(vft, VFTIndexes::IClientAppManager::GetUpdateInfo, hkClientAppManager_GetUpdateInfo);
		Hooks::IClientAppManager_GetAppStateInfo = new VFTHook(vft, VFTIndexes::IClientAppManager::GetAppStateInfo, hkClientAppManager_GetAppStateInfo);
		Hooks::IClientAppManager_LaunchApp = new VFTHook(vft, VFTIndexes::IClientAppManager::LaunchApp, hkClientAppManager_LaunchApp);
		Hooks::IClientAppManager_IsAppDlcInstalled = new VFTHook(vft, VFTIndexes::IClientAppManager::IsAppDlcInstalled, hkClientAppManager_IsAppDlcInstalled);

		Hooks::IClientAppManager_BCanRemotePlayTogether = new VFTHook(vft, VFTIndexes::IClientAppManager::BCanRemotePlayTogether, hkClientAppManager_BCanRemotePlayTogether);
		Hooks::IClientAppManager_BIsDlcEnabled = new VFTHook(vft, VFTIndexes::IClientAppManager::BIsDlcEnabled, hkClientAppManager_BIsDlcEnabled);
		Hooks::IClientAppManager_GetAppUpdateInfo = new VFTHook(vft, VFTIndexes::IClientAppManager::GetUpdateInfo, hkClientAppManager_GetUpdateInfo);
		Hooks::IClientAppManager_GetAppStateInfo = new VFTHook(vft, VFTIndexes::IClientAppManager::GetAppStateInfo, hkClientAppManager_GetAppStateInfo);
		Hooks::IClientAppManager_LaunchApp = new VFTHook(vft, VFTIndexes::IClientAppManager::LaunchApp, hkClientAppManager_LaunchApp);
		Hooks::IClientAppManager_IsAppDlcInstalled = new VFTHook(vft, VFTIndexes::IClientAppManager::IsAppDlcInstalled, hkClientAppManager_IsAppDlcInstalled);

		LOG_DEBUG("IClientAppManager->vft at %p\n", reinterpret_cast<void*>(vft.get()));
	}

	{
		const auto clientApps = usr->getClientApps();
		LOG_DEBUG("CUserAppInfo at %p\n", reinterpret_cast<void*>(clientApps));

		auto vft = std::make_shared<lm_vmt_t>();
		LM_VmtNew(*reinterpret_cast<lm_address_t**>(clientApps), vft.get());

		Hooks::IClientApps_GetDLCDataByIndex = new VFTHook(vft, VFTIndexes::IClientApps::GetDLCDataByIndex, hkClientApps_GetDLCDataByIndex);
		Hooks::IClientApps_GetDLCCount = new VFTHook(vft, VFTIndexes::IClientApps::GetDLCCount, hkClientApps_GetDLCCount);

		LOG_DEBUG("IClientApps->vft at %p\n", reinterpret_cast<void*>(vft.get()));
	}

	{
		const auto clientUser = usr->getClientUser();
		LOG_DEBUG("IClientUser at %p\n", reinterpret_cast<void*>(clientUser));

		auto vft = std::make_shared<lm_vmt_t>();
		LM_VmtNew(*reinterpret_cast<lm_address_t**>(clientUser), vft.get());

		Hooks::IClientUser_BLoggedOn = new VFTHook(vft, VFTIndexes::IClientUser::BLoggedOn, &hkClientUser_BLoggedOn);
		Hooks::IClientUser_BUpdateAppOwnershipTicket = new VFTHook(vft, VFTIndexes::IClientUser::BUpdateAppOwnershipTicket, hkClientUser_BUpdateAppOwnershipTicket);
		Hooks::IClientUser_GetAppOwnershipTicketExtendedData = new VFTHook(vft, VFTIndexes::IClientUser::GetAppOwnershipTicketExtendedData, hkClientUser_GetAppOwnershipTicketExtendedData);
		//GetEncryptedAppTicket is just a wrapper for CUser::GetEncryptedAppTicket. But there is no need to go deeper
		//since we load the encrypted ticket in the Networking layer. We just need this function to spoof our steamId once
		Hooks::IClientUser_GetEncryptedAppTicket = new VFTHook(vft, VFTIndexes::IClientUser::GetEncryptedAppTicket, hkClientUser_GetEncryptedAppTicket);
		Hooks::IClientUser_GetLegacyCDKey = new VFTHook(vft, VFTIndexes::IClientUser::GetLegacyCDKey, hkClientUser_GetLegacyCDKey);
		Hooks::IClientUser_IsUserSubscribedAppInTicket = new VFTHook(vft, VFTIndexes::IClientUser::IsUserSubscribedAppInTicket, hkClientUser_IsUserSubscribedAppInTicket);

		LOG_DEBUG("IClientUser->vft at %p\n", reinterpret_cast<void*>(vft.get()));
	}

	{
		const auto utils = g_pSteamEngine->getUtils();
		LOG_DEBUG("IClientUtils at %p\n", reinterpret_cast<void*>(utils));

		auto vft = std::make_shared<lm_vmt_t>();
		LM_VmtNew(*reinterpret_cast<lm_address_t**>(utils), vft.get());

		Hooks::IClientUtils_GetAppId = new VFTHook(vft, VFTIndexes::IClientUtils::GetAppId, hkClientUtils_GetAppId);
		Hooks::IClientUtils_GetOfflineMode = new VFTHook(vft, VFTIndexes::IClientUtils::GetOfflineMode, hkClientUtils_GetOfflineMode);

		LOG_DEBUG("IClientUtils->vft at %p\n", reinterpret_cast<void*>(vft.get()));
	}

	#pragma GCC diagnostic pop

	setupAll();
	placeAll();

	hooked = true;

	Lua::fireCallback(Lua::Callbacks::SLSsteam_Initialized);
}

void Hooks::removeAll(const bool deleteAll)
{
	LOG_DEBUG("Hooks::removeAll()\n");

	for (IHook* hook : IHook::hooks)
	{
		hook->remove();

		if (deleteAll)
		{
			delete hook;
		}
	}
}
