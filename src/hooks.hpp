#pragma once

#include "libmem/libmem.h"

#include <cstddef>
#include <memory>
#include <string>

class CAppOwnershipInfo;
class CProtoBufMsgBase;

struct gameserverdetails_t;

struct Pattern_t;

template<typename T>
union FunctionUnion_t
{
	T fn;
	lm_address_t address;
};

//TODO: Look up if there's an interface kinda thing for C++
template<typename T>
class Hook
{
public:
	//TODO: Add base setup fn to set hookFn
	std::string name;
	FunctionUnion_t<T> originalFn;
	FunctionUnion_t<T> hookFn;

	Hook(const char* name);

	virtual void place() = 0;
	virtual void remove() = 0;
};

template<typename T>
class DetourHook : public Hook<T>
{
public:
	FunctionUnion_t<T> tramp;
	size_t size;

	DetourHook(const char* name);
	DetourHook();

	virtual void place();
	virtual void remove();

	bool setup(Pattern_t pattern, T hookFn);
};

template<typename T>
class VFTHook : public Hook<T>
{
public:
	std::shared_ptr<lm_vmt_t> vft;
	unsigned int index;
	bool hooked;

	VFTHook(const char* name);

	virtual void place();
	virtual void remove();

	void setup(std::shared_ptr<lm_vmt_t> vft, unsigned int index, T hookFn);
};

namespace Hooks
{
	typedef void(*TraceIPC_t)(const char*, const char*);

	typedef void(*IClientAppManager_RunIPCFrame_t)(void*, void*, void*, void*);
	typedef void(*IClientApps_RunIPCFrame_t)(void*, void*, void*, void*);
	typedef void(*IClientRemoteStorage_RunIPCFrame_t)(void*, void*, void*, void*);
	typedef void(*IClientUGC_RunIPCFrame_t)(void*, void*, void*, void*);
	typedef void(*IClientUtils_RunIPCFrame_t)(void*, void*, void*, void*);
	typedef void(*IClientUser_RunIPCFrame_t)(void*, void*, void*, void*);
	typedef void(*IClientUserStats_RunIPCFrame_t)(void*, void*, void*, void*);

	typedef uint32_t(*CAPIJob_GetPlayerStats_t)(void*);

	typedef void(*CProtoBufMsgBase_InitFromPacket_t)(CProtoBufMsgBase*, void*);
	typedef uint32_t(*CProtoBufMsgBase_Send_t)(CProtoBufMsgBase*);

	typedef void(*CSteamEngine_Init_t)(void*);
	typedef uint32_t(*CSteamEngine_SetAppIdForCurrentPipe_t)(void*, uint32_t, bool);

	typedef gameserverdetails_t*(*CSteamMatchmakingServers_GetServerDetails_t)(void*, uint32_t, uint32_t);
	typedef uint32_t(*CSteamMatchmakingServers_RequestInternetServerList_t)(void*, uint32_t, uint32_t, uint32_t, uint32_t);

	typedef uint32_t(*CUser_CheckAppOwnership_t)(void*, uint32_t, CAppOwnershipInfo*);
	typedef uint32_t(*CUser_GetSubscribedApps_t)(void*, uint32_t*, uint32_t, uint8_t);
	typedef bool(*IClientAppManager_BCanRemotePlayTogether_t)(void*, uint32_t);

	typedef bool(*IClientUser_BLoggedOn_t)(void*);
	typedef uint32_t(*IClientUser_BUpdateAppOwnershipTicket_t)(void*, uint32_t, bool);
	typedef uint32_t(*IClientUser_GetAppOwnershipTicketExtendedData_t)(void*, uint32_t, void*, uint32_t, uint32_t*, uint32_t*, uint32_t*, uint32_t*);
	typedef uint8_t(*IClientUser_IsUserSubscribedAppInTicket_t)(void*, uint32_t, uint32_t, uint32_t, uint32_t);
	typedef bool(*IClientUser_RequiresLegacyCDKey_t)(void*, uint32_t, uint32_t*);

	typedef bool(*IClientUtils_GetOfflineMode_t)(void*);

	extern DetourHook<TraceIPC_t> TraceIPC;

	extern DetourHook<CAPIJob_GetPlayerStats_t> CAPIJob_GetPlayerStats;

	extern DetourHook<CProtoBufMsgBase_InitFromPacket_t> CProtoBufMsgBase_InitFromPacket;
	extern DetourHook<CProtoBufMsgBase_Send_t> CProtoBufMsgBase_Send;

	extern DetourHook<CSteamMatchmakingServers_GetServerDetails_t> CSteamMatchmakingServers_GetServerDetails;
	extern DetourHook<CSteamMatchmakingServers_RequestInternetServerList_t> CSteamMatchmakingServers_RequestInternetServerList;

	extern DetourHook<IClientAppManager_RunIPCFrame_t> IClientAppManager_RunIPCFrame;
	extern DetourHook<IClientApps_RunIPCFrame_t> IClientApps_RunIPCFrame;
	extern DetourHook<IClientRemoteStorage_RunIPCFrame_t> IClientRemoteStorage_RunIPCFrame;
	extern DetourHook<IClientUGC_RunIPCFrame_t> IClientUGC_RunIPCFrame;
	extern DetourHook<IClientUtils_RunIPCFrame_t> IClientUtils_RunIPCFrame;
	extern DetourHook<IClientUser_RunIPCFrame_t> IClientUser_RunIPCFrame;
	extern DetourHook<IClientUserStats_RunIPCFrame_t> IClientUserStats_RunIPCFrame;

	extern DetourHook<CSteamEngine_Init_t> CSteamEngine_Init;
	extern DetourHook<CSteamEngine_SetAppIdForCurrentPipe_t> CSteamEngine_SetAppIdForCurrentPipe;

	extern DetourHook<CUser_CheckAppOwnership_t> CUser_CheckAppOwnership;
	extern DetourHook<CUser_GetSubscribedApps_t> CUser_GetSubscribedApps;

	extern DetourHook<IClientAppManager_BCanRemotePlayTogether_t> IClientAppManager_BCanRemotePlayTogether;

	extern DetourHook<IClientUser_BLoggedOn_t> IClientUser_BLoggedOn;
	extern DetourHook<IClientUser_BUpdateAppOwnershipTicket_t> IClientUser_BUpdateAppOwnershipTicket;
	extern DetourHook<IClientUser_GetAppOwnershipTicketExtendedData_t> IClientUser_GetAppOwnershipTicketExtendedData;
	extern DetourHook<IClientUser_IsUserSubscribedAppInTicket_t> IClientUser_IsUserSubscribedAppInTicket;
	extern DetourHook<IClientUser_RequiresLegacyCDKey_t> IClientUser_RequiresLegacyCDKey;

	typedef bool(*IClientAppManager_BIsDlcEnabled_t)(void*, uint32_t, uint32_t, void*);
	typedef bool(*IClientAppManager_GetAppUpdateInfo_t)(void*, uint32_t, uint32_t*);
	typedef void*(*IClientAppManager_LaunchApp_t)(void*, uint32_t*, void*, void*, void*);
	typedef bool(*IClientAppManager_IsAppDlcInstalled_t)(void*, uint32_t, uint32_t);
	typedef bool(*IClientAppManager_GetAppOwnershipInfo_t)(void*, uint32_t, uint32_t*);
	typedef unsigned int(*IClientApps_GetDLCCount_t)(void*, uint32_t);
	typedef bool(*IClientApps_GetDLCDataByIndex_t)(void*, uint32_t, int, uint32_t*, bool*, char*, size_t);
	typedef bool(*IClientRemoteStorage_IsCloudEnabledForApp_t)(void*, uint32_t);
	typedef uint32_t(*IClientUtils_GetAppId_t)(void*);

	extern VFTHook<IClientAppManager_BIsDlcEnabled_t> IClientAppManager_BIsDlcEnabled;
	extern VFTHook<IClientAppManager_GetAppUpdateInfo_t> IClientAppManager_GetAppUpdateInfo;
	extern VFTHook<IClientAppManager_LaunchApp_t> IClientAppManager_LaunchApp;
	extern VFTHook<IClientAppManager_IsAppDlcInstalled_t> IClientAppManager_IsAppDlcInstalled;
	extern VFTHook<IClientAppManager_GetAppOwnershipInfo_t> IClientAppManager_GetAppOwnershipInfo;

	extern VFTHook<IClientApps_GetDLCDataByIndex_t> IClientApps_GetDLCDataByIndex;
	extern VFTHook<IClientApps_GetDLCCount_t> IClientApps_GetDLCCount;

	extern VFTHook<IClientRemoteStorage_IsCloudEnabledForApp_t> IClientRemoteStorage_IsCloudEnabledForApp;

	extern VFTHook<IClientUtils_GetAppId_t> IClientUtils_GetAppId;
	extern VFTHook<IClientUtils_GetOfflineMode_t> IClientUtils_GetOfflineMode;

	typedef void(*ISteamMatchmakingPingResponse_ServerResponded_t)(void*, gameserverdetails_t*);


	//steamui.so
	extern DetourHook<ISteamMatchmakingPingResponse_ServerResponded_t> ISteamMatchmakingPingResponse_ServerResponded;


	//Naked
	extern lm_address_t IClientUser_GetSteamId;

	bool setup();
	void place();
	void remove();
}
