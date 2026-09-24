#pragma once

#include "sdk/sdk.hpp"

#include "lua.hpp"

#include "libmem/libmem.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>


struct Pattern_t;
struct VFTableInfo_t;

template<typename T>
struct FunctionUnion_t
{
	union
	{
		T fn;
		lm_address_t address;
	};

	FunctionUnion_t();
	FunctionUnion_t(const lm_address_t address);
	FunctionUnion_t(const T fn);
};

class IHook
{
public:
	static std::unordered_set<IHook*> hooks;

	enum class EType
	{
		Unknown,
		Detour,
		VFT,
		Lua
	};
	
	IHook();
	virtual ~IHook();

	virtual EType getType() = 0;
	virtual bool isHooked() = 0;
	virtual void setup() = 0;

	virtual void place() = 0;
	virtual void remove() = 0;
};

template<typename T>
class Hook : public IHook
{
	lm_address_t targetAddress = LM_ADDRESS_BAD;
	lm_address_t* targetAddressPtr = nullptr;

protected:
	bool setupRan = false;

public:
	std::string name = "";
	FunctionUnion_t<T> originalFn = nullptr;
	FunctionUnion_t<T> hookFn = nullptr;

	Hook(const std::string& name, lm_address_t targetAddress, const T hookFn);
	//VFTIndex_t & Pattern_t hooks
	Hook(const std::string& name, lm_address_t* targetAddressPtr, const T hookFn);

	constexpr virtual IHook::EType getType()
	{
		return IHook::EType::Unknown;
	}

	constexpr virtual bool isHooked()
	{
		return false;
	}

	virtual void setup();
};

template<typename T>
class DetourHook : public Hook<T>
{
public:
	FunctionUnion_t<T> tramp = nullptr;
	size_t size = 0;

	DetourHook(const std::string& name, lm_address_t targetAddress, const T hookFn);
	DetourHook(Pattern_t& targetPattern, const T hookFn);
	DetourHook(VFTableInfo_t& targetVFTInfo, const T hookFn);

	constexpr virtual IHook::EType getType()
	{
		return IHook::EType::Detour;
	}

	constexpr virtual bool isHooked()
	{
		return size > 0;
	}

	virtual void place();
	virtual void remove();
};

template<typename T>
class VFTHook : public Hook<T>
{
	std::shared_ptr<lm_vmt_t> vft = nullptr;

public:
	unsigned int index = 0;
	bool hooked = false;

	VFTHook(const std::shared_ptr<lm_vmt_t>& vft, const VFTableInfo_t& mapVFTInfo, const T hookFn);

	constexpr virtual IHook::EType getType()
	{
		return IHook::EType::Detour;
	}

	constexpr virtual bool isHooked()
	{
		return hooked;
	}

	virtual void place();
	virtual void remove();
};

// Lua hooks are a little special for now
class LuaHook
{
	static int hookCount;
public:
	static std::unordered_map<int, LuaHook*> hooks;

	std::string name = "";
	lm_address_t fn = LM_ADDRESS_BAD;
	lm_address_t hookFn = LM_ADDRESS_BAD;
	lm_address_t tramp = 0;
	size_t size = 0;
	int idx = -1;

	LuaHook(const char* name, const Lua::Ptr_t targetFn);
	~LuaHook();

	Lua::Ptr_t place();
	bool remove();
};

namespace Hooks
{
	typedef void(*TraceIPC_t)(const char*, const char*);

	typedef uint32_t(*CAPIJob_SendAndRecv_t)(CAPIJob*, CProtoBufMsgBase*, uint32_t, uint32_t, CProtoBufMsgBase*, EMsg);

	typedef uint32_t(*CAppDataCache_BParseResponseFromMessage_t)(void*, CProtoBufMsgBase*);

	typedef uint32_t(*CClientUnifiedServiceMethod_SendAndRecvMsg_t)(CClientUnifiedServiceTransport*, const char*, void*, void*, void*);

	typedef void(*CCMInterface_RecvPkt_t)(CCMInterface*, CNetPacket*);

	typedef uint32_t(*CSteamEngine_ProcessIPCFrame_t)(CSteamEngine*, HSteamPipe, CUtlBuffer*, CUtlBuffer*);
	typedef AppId_t(*CSteamEngine_SetAppIdForCurrentPipe_t)(CSteamEngine*, AppId_t, bool);

	typedef gameserverdetails_t*(*CSteamMatchmakingServers_GetServerDetails_t)(void*, uint32_t, uint32_t);
	typedef uint32_t(*CSteamMatchmakingServers_RequestInternetServerList_t)(void*, AppId_t, uint32_t, uint32_t, uint32_t);

	typedef bool(*CUser_CheckAppOwnership_t)(CUser*, AppId_t, AppOwnershipInfo_t*);
	typedef uint32_t(*CUser_GetSubscribedApps_t)(CUser*, AppId_t*, uint32_t, uint8_t);
	typedef uint32_t(*CUser_PostCallbackToAppId_t)(CUser*, AppId_t, uint32_t, void*, uint32_t);
	typedef uint32_t(*CUser_SpawnGameId_t)(void*, const char*, const char*, const char*, GameId_t*, const char*, int32_t, int32_t, int32_t, int32_t, void*);

	typedef bool(*CUserAppManager_BuildDepotDependency_t)(IClientAppManager*, AppId_t, void*, CUtlVector<DepotInfo_t>*, CUtlVector<DepotInfo_t>*, void*, uint32_t*, bool*);

	typedef bool(*CWebSocketConnection_BBuildAndAsyncSendFrame_t)(CWebSocketConnection*, EWebSocketConnectionSendType, void*, uint32_t);

	typedef bool(*IClientCompat_BIsCompatLayerEnabled_t)(IClientCompat*);

	typedef bool(*IClientConfigStore_SetString_t)(void*, uint32_t, const char*, const char*);

	typedef uint32_t(*IClientFriends_GetFriendGamePlayed_t)(void*, uint64_t, GamePlayed_t*);

	typedef bool(*IClientRemoteStorage_IsCloudEnabledForApp_t)(void*, AppId_t);

	extern DetourHook<TraceIPC_t>* TraceIPC;

	extern DetourHook<CAPIJob_SendAndRecv_t>* CAPIJob_SendAndRecv;

	extern DetourHook<CAppDataCache_BParseResponseFromMessage_t>* CAppDataCache_BParseResponseFromMessage;

	extern DetourHook<CClientUnifiedServiceMethod_SendAndRecvMsg_t>* CClientUnifiedServiceMethod_SendAndRecvMsg;

	extern DetourHook<CCMInterface_RecvPkt_t>* CCMInterface_RecvPkt;

	extern DetourHook<CSteamMatchmakingServers_GetServerDetails_t>* CSteamMatchmakingServers_GetServerDetails;
	extern DetourHook<CSteamMatchmakingServers_RequestInternetServerList_t>* CSteamMatchmakingServers_RequestInternetServerList;

	extern DetourHook<CSteamEngine_ProcessIPCFrame_t>* CSteamEngine_ProcessIPCFrame;
	extern DetourHook<CSteamEngine_SetAppIdForCurrentPipe_t>* CSteamEngine_SetAppIdForCurrentPipe;

	extern DetourHook<CUser_CheckAppOwnership_t>* CUser_CheckAppOwnership;
	extern DetourHook<CUser_GetSubscribedApps_t>* CUser_GetSubscribedApps;
	extern DetourHook<CUser_PostCallbackToAppId_t>* CUser_PostCallbackToAppId;
	extern DetourHook<CUser_SpawnGameId_t>* CUser_SpawnGameId;

	extern DetourHook<CUserAppManager_BuildDepotDependency_t>* CUserAppManager_BuildDepotDependency;

	extern DetourHook<CWebSocketConnection_BBuildAndAsyncSendFrame_t>* CWebSocketConnection_BBuildAndAsyncSendFrame;

	extern DetourHook<IClientCompat_BIsCompatLayerEnabled_t>* IClientCompat_BIsCompatLayerEnabled;

	extern DetourHook<IClientConfigStore_SetString_t>* IClientConfigStore_SetString;

	extern DetourHook<IClientFriends_GetFriendGamePlayed_t>* IClientFriends_GetFriendGamePlayed;

	extern DetourHook<IClientRemoteStorage_IsCloudEnabledForApp_t>* IClientRemoteStorage_IsCloudEnabledForApp;

	typedef unsigned int(*IClientApps_GetDLCCount_t)(IClientApps*, AppId_t);
	typedef bool(*IClientApps_GetDLCDataByIndex_t)(IClientApps*, AppId_t, int, AppId_t*, bool*, char*, size_t);

	typedef bool(*IClientAppManager_BCanRemotePlayTogether_t)(IClientAppManager*, AppId_t);
	typedef bool(*IClientAppManager_BIsDlcEnabled_t)(IClientAppManager*, AppId_t, AppId_t, void*);
	typedef bool(*IClientAppManager_GetAppUpdateInfo_t)(IClientAppManager*, AppId_t, uint32_t*);
	typedef bool(*IClientAppManager_GetAppStateInfo_t)(IClientAppManager*, AppId_t, AppStateInfo_t*);
	typedef void*(*IClientAppManager_LaunchApp_t)(IClientAppManager*, AppId_t*, void*, void*, void*);
	typedef bool(*IClientAppManager_IsAppDlcInstalled_t)(IClientAppManager*, AppId_t, AppId_t);

	typedef bool(*IClientUser_BLoggedOn_t)(IClientUser*);
	typedef uint32_t(*IClientUser_BUpdateAppOwnershipTicket_t)(IClientUser*, AppId_t, bool);
	typedef uint32_t(*IClientUser_GetAppOwnershipTicketExtendedData_t)(IClientUser*, uint32_t, void*, uint32_t, uint32_t*, uint32_t*, uint32_t*, uint32_t*);
	typedef bool(*IClientUser_GetEncryptedAppTicket_t)(IClientUser*, void*, uint32_t, uint32_t*);
	typedef bool(*IClientUser_GetLegacyCDKey_t)(IClientUser*, AppId_t, char*, uint32_t);
	typedef uint8_t(*IClientUser_IsUserSubscribedAppInTicket_t)(IClientUser*, uint64_t, AppId_t);

	typedef AppId_t(*IClientUtils_GetAppId_t)(IClientUtils*);
	typedef bool(*IClientUtils_GetOfflineMode_t)(IClientUtils*);

	extern VFTHook<IClientAppManager_BCanRemotePlayTogether_t>* IClientAppManager_BCanRemotePlayTogether;
	extern VFTHook<IClientAppManager_BIsDlcEnabled_t>* IClientAppManager_BIsDlcEnabled;
	extern VFTHook<IClientAppManager_GetAppUpdateInfo_t>* IClientAppManager_GetAppUpdateInfo;
	extern VFTHook<IClientAppManager_GetAppStateInfo_t>* IClientAppManager_GetAppStateInfo;
	extern VFTHook<IClientAppManager_LaunchApp_t>* IClientAppManager_LaunchApp;
	extern VFTHook<IClientAppManager_IsAppDlcInstalled_t>* IClientAppManager_IsAppDlcInstalled;

	extern VFTHook<IClientApps_GetDLCDataByIndex_t>* IClientApps_GetDLCDataByIndex;
	extern VFTHook<IClientApps_GetDLCCount_t>* IClientApps_GetDLCCount;

	extern VFTHook<IClientUser_BLoggedOn_t>* IClientUser_BLoggedOn;
	extern VFTHook<IClientUser_BUpdateAppOwnershipTicket_t>* IClientUser_BUpdateAppOwnershipTicket;
	extern VFTHook<IClientUser_GetAppOwnershipTicketExtendedData_t>* IClientUser_GetAppOwnershipTicketExtendedData;
	extern VFTHook<IClientUser_GetEncryptedAppTicket_t>* IClientUser_GetEncryptedAppTicket;
	extern VFTHook<IClientUser_GetLegacyCDKey_t>* IClientUser_GetLegacyCDKey;
	extern VFTHook<IClientUser_IsUserSubscribedAppInTicket_t>* IClientUser_IsUserSubscribedAppInTicket;

	extern VFTHook<IClientUtils_GetAppId_t>* IClientUtils_GetAppId;
	extern VFTHook<IClientUtils_GetOfflineMode_t>* IClientUtils_GetOfflineMode;


	//steamui.so
	typedef void(*CGameInfoDialog_ServerResponded_t)(void*, gameserverdetails_t*);

	extern DetourHook<CGameInfoDialog_ServerResponded_t>* CGameInfoDialog_ServerResponded;

	bool init();
	void setupAll();
	void placeAll();
	void placeVFTHooks();
	void removeAll(const bool deleteAll = false);
}
