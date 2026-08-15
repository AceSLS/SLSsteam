#pragma once

#include "sdk/sdk.hpp"

#include "libmem/libmem.h"

#include <cstddef>
#include <memory>
#include <string>


struct Pattern_t;
struct VFTableInfo_t;

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

	bool setup(const char* name, lm_address_t fn, T hookFn);
	bool setup(const Pattern_t& pattern, T hookFn);
	bool setup(const VFTableInfo_t& info, T hookFn);
};

template<typename T>
class VFTHook : public Hook<T>
{
public:
	std::shared_ptr<lm_vmt_t> vft;
	unsigned int index;
	bool hooked;

	VFTHook(const char* name);
	VFTHook();

	virtual void place();
	virtual void remove();

	void setup(std::shared_ptr<lm_vmt_t> vft, const VFTableInfo_t&, T hookFn);
};

namespace Hooks
{
	typedef void(*TraceIPC_t)(const char*, const char*);

	typedef uint32_t(*CAPIJob_SendAndRecv_t)(CAPIJob*, CProtoBufMsgBase*, uint32_t, uint32_t, CProtoBufMsgBase*, EMsg);

	typedef uint32_t(*CAppDataCache_BParseResponseFromMessage_t)(void*, CProtoBufMsgBase*);

	typedef uint32_t(*CClientUnifiedServiceMethod_SendAndRecvMsg_t)(CClientUnifiedServiceTransport*, const char*, void*, void*, void*);

	typedef void(*CCMInterface_RecvPkt_t)(CCMInterface*, CNetPacket*);

	typedef void(*CRemoteClientManager_RecvPkt_t)(void*, CNetPacket*);
	typedef bool(*CRemoteClientManager_SendProtoBufMsg_t)(void*, CProtoBufMsgBase*, void*);

	typedef uint32_t(*CSteamEngine_ProcessIPCFrame_t)(CSteamEngine*, HSteamPipe, CUtlBuffer*, CUtlBuffer*);
	typedef AppId_t(*CSteamEngine_SetAppIdForCurrentPipe_t)(CSteamEngine*, AppId_t, bool);

	typedef gameserverdetails_t*(*CSteamMatchmakingServers_GetServerDetails_t)(void*, uint32_t, uint32_t);
	typedef uint32_t(*CSteamMatchmakingServers_RequestInternetServerList_t)(void*, AppId_t, uint32_t, uint32_t, uint32_t);

	typedef uint32_t(*CUser_CheckAppOwnership_t)(CUser*, AppId_t, AppOwnershipInfo_t*);
	typedef uint32_t(*CUser_GetSubscribedApps_t)(CUser*, AppId_t*, uint32_t, uint8_t);
	typedef uint32_t(*CUser_PostCallbackToAppId_t)(CUser*, AppId_t, uint32_t, void*, uint32_t);

	typedef bool(*CUserAppManager_BuildDepotDependency_t)(IClientAppManager*, AppId_t, void*, CUtlVector<DepotInfo_t>*, CUtlVector<DepotInfo_t>*, void*, uint32_t*, bool*);

	typedef bool(*CWebSocketConnection_BBuildAndAsyncSendFrame_t)(CWebSocketConnection*, EWebSocketConnectionSendType, void*, uint32_t);

	typedef bool(*IClientCompat_BIsCompatLayerEnabled_t)(IClientCompat*);

	typedef bool(*IClientConfigStore_SetString_t)(void*, uint32_t, const char*, const char*);

	typedef uint32_t(*IClientFriends_GetFriendGamePlayed_t)(void*, uint64_t, GamePlayed_t*);

	typedef bool(*IClientRemoteStorage_IsCloudEnabledForApp_t)(void*, AppId_t);

	extern DetourHook<TraceIPC_t> TraceIPC;

	extern DetourHook<CAPIJob_SendAndRecv_t> CAPIJob_SendAndRecv;

	extern DetourHook<CAppDataCache_BParseResponseFromMessage_t> CAppDataCache_BParseResponseFromMessage;

	extern DetourHook<CClientUnifiedServiceMethod_SendAndRecvMsg_t> CClientUnifiedServiceMethod_SendAndRecvMsg;

	extern DetourHook<CCMInterface_RecvPkt_t> CCMInterface_RecvPkt;

	extern DetourHook<CRemoteClientManager_RecvPkt_t> CRemoteClientManager_RecvPkt;
	extern DetourHook<CRemoteClientManager_SendProtoBufMsg_t> CRemoteClientManager_SendProtoBufMsg;

	extern DetourHook<CSteamMatchmakingServers_GetServerDetails_t> CSteamMatchmakingServers_GetServerDetails;
	extern DetourHook<CSteamMatchmakingServers_RequestInternetServerList_t> CSteamMatchmakingServers_RequestInternetServerList;

	extern DetourHook<CSteamEngine_ProcessIPCFrame_t> CSteamEngine_ProcessIPCFrame;
	extern DetourHook<CSteamEngine_SetAppIdForCurrentPipe_t> CSteamEngine_SetAppIdForCurrentPipe;

	extern DetourHook<CUser_CheckAppOwnership_t> CUser_CheckAppOwnership;
	extern DetourHook<CUser_GetSubscribedApps_t> CUser_GetSubscribedApps;
	extern DetourHook<CUser_PostCallbackToAppId_t> CUser_PostCallbackToAppId;

	extern DetourHook<CUserAppManager_BuildDepotDependency_t> CUserAppManager_BuildDepotDependency;

	extern DetourHook<CWebSocketConnection_BBuildAndAsyncSendFrame_t> CWebSocketConnection_BBuildAndAsyncSendFrame;

	extern DetourHook<IClientCompat_BIsCompatLayerEnabled_t> IClientCompat_BIsCompatLayerEnabled;

	extern DetourHook<IClientConfigStore_SetString_t> IClientConfigStore_SetString;

	extern DetourHook<IClientFriends_GetFriendGamePlayed_t> IClientFriends_GetFriendGamePlayed;

	extern DetourHook<IClientRemoteStorage_IsCloudEnabledForApp_t> IClientRemoteStorage_IsCloudEnabledForApp;

	typedef unsigned int(*IClientApps_GetDLCCount_t)(IClientApps*, AppId_t);
	typedef bool(*IClientApps_GetDLCDataByIndex_t)(IClientApps*, AppId_t, int, AppId_t*, bool*, char*, size_t);

	typedef bool(*IClientAppManager_BCanRemotePlayTogether_t)(IClientAppManager*, AppId_t);
	typedef bool(*IClientAppManager_BIsDlcEnabled_t)(IClientAppManager*, AppId_t, AppId_t, void*);
	typedef bool(*IClientAppManager_GetAppUpdateInfo_t)(IClientAppManager*, AppId_t, uint32_t*);
	typedef void*(*IClientAppManager_LaunchApp_t)(IClientAppManager*, AppId_t*, void*, void*, void*);
	typedef bool(*IClientAppManager_IsAppDlcInstalled_t)(IClientAppManager*, AppId_t, AppId_t);

	typedef bool(*IClientUser_BLoggedOn_t)(IClientUser*);
	typedef uint32_t(*IClientUser_BUpdateAppOwnershipTicket_t)(IClientUser*, AppId_t, bool);
	typedef uint32_t(*IClientUser_GetAppOwnershipTicketExtendedData_t)(IClientUser*, uint32_t, void*, uint32_t, uint32_t*, uint32_t*, uint32_t*, uint32_t*);
	typedef bool(*IClientUser_GetEncryptedAppTicket_t)(IClientUser*, void*, uint32_t, uint32_t*);
	typedef bool(*IClientUser_GetLegacyCDKey_t)(IClientUser*, AppId_t, char*, uint32_t);
	typedef uint8_t(*IClientUser_IsUserSubscribedAppInTicket_t)(IClientUser*, uint32_t, uint32_t, uint32_t, AppId_t);

	typedef AppId_t(*IClientUtils_GetAppId_t)(IClientUtils*);
	typedef bool(*IClientUtils_GetOfflineMode_t)(IClientUtils*);

	extern VFTHook<IClientAppManager_BCanRemotePlayTogether_t> IClientAppManager_BCanRemotePlayTogether;
	extern VFTHook<IClientAppManager_BIsDlcEnabled_t> IClientAppManager_BIsDlcEnabled;
	extern VFTHook<IClientAppManager_GetAppUpdateInfo_t> IClientAppManager_GetAppUpdateInfo;
	extern VFTHook<IClientAppManager_LaunchApp_t> IClientAppManager_LaunchApp;
	extern VFTHook<IClientAppManager_IsAppDlcInstalled_t> IClientAppManager_IsAppDlcInstalled;

	extern VFTHook<IClientApps_GetDLCDataByIndex_t> IClientApps_GetDLCDataByIndex;
	extern VFTHook<IClientApps_GetDLCCount_t> IClientApps_GetDLCCount;

	extern VFTHook<IClientUser_BLoggedOn_t> IClientUser_BLoggedOn;
	extern VFTHook<IClientUser_BUpdateAppOwnershipTicket_t> IClientUser_BUpdateAppOwnershipTicket;
	extern VFTHook<IClientUser_GetAppOwnershipTicketExtendedData_t> IClientUser_GetAppOwnershipTicketExtendedData;
	extern VFTHook<IClientUser_GetEncryptedAppTicket_t> IClientUser_GetEncryptedAppTicket;
	extern VFTHook<IClientUser_GetLegacyCDKey_t> IClientUser_GetLegacyCDKey;
	extern VFTHook<IClientUser_IsUserSubscribedAppInTicket_t> IClientUser_IsUserSubscribedAppInTicket;

	extern VFTHook<IClientUtils_GetAppId_t> IClientUtils_GetAppId;
	extern VFTHook<IClientUtils_GetOfflineMode_t> IClientUtils_GetOfflineMode;


	//steamui.so
	typedef void(*CGameInfoDialog_ServerResponded_t)(void*, gameserverdetails_t*);

	extern DetourHook<CGameInfoDialog_ServerResponded_t> CGameInfoDialog_ServerResponded;

	bool setup();
	void place();
	void placeVFTHooks();
	void remove();
}
