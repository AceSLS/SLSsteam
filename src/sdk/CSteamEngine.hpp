#pragma once

#include "CUtl.hpp"
#include "types.hpp"

#include <cstdint>
#include <string>


class CUser;
class IClientUtils;

enum class EIPCCmd : uint8_t
{
	RunInterface = 1,
	SerializeCallbacks = 2,
	CreateGlobalUser = 3, //Also used to connect to global user
	DisconnectGlobalUser = 4,
	ClosePipe = 5,
	Heartbeat = 6,
	ConnectPipe = 9
};

std::string EIPCCmd_ToString(const EIPCCmd cmd);

enum class EIPCExitCode : uint8_t
{
	Success = 0xb
};

enum class EIPCInterface : uint8_t
{
	User = 0x1,
	GameServerInternal = 0x2,
	Friends = 0x3,
	Utils = 0x4,
	Billing = 0x5,
	Matchmaking = 0x6,
	Apps = 0x8,
	UserStats = 0xb,
	Networking = 0xc,
	RemoteStorage = 0xd,
	DepotBuilder = 0x10,
	AppManager = 0x11,
	ConfigStore = 0x12,
	GameCoordinator = 0x13,
	GameServerStats = 0x14,
	GameStats = 0x15,
	HTTP = 0x16,
	Screenshots = 0x17,
	Audio = 0x18,
	UnifiedMessages = 0x19,
	StreamLauncher = 0x1a,
	ParentalSettings = 0x1b,
	NetworkDeviceManager = 0x1d,
	Music = 0x1e,
	RemoteClientManager = 0x1f,
	UGC = 0x20,
	StreamClient = 0x21,
	ProductBuilder = 0x22,
	Shortcuts = 0x23,
	GameNotifications = 0x25,
	Video = 0x26,
	Inventory = 0x27,
	VR = 0x28,
	ControllerSerialized = 0x29,
	AppDisableUpdate = 0x2a,
	SharedConnection = 0x2c,
	Shader = 0x2d,
	NetworkingSocketsSerialized = 0x2e,
	Compat = 0x30,
	Parties = 0x31,
	NetworkingUtilsSerialized = 0x32,
	RemotePlay = 0x34,
	GameServerPacketHandler = 0x35,
	SystemManager = 0x36,
	SystemPerfManager = 0x39,
	SystemDockManager = 0x3a,
	SystemAudioManager = 0x3b,
	SystemDisplayManager = 0x3c,
	Timeline = 0x3d
};

std::string EIPCInterface_ToString(const EIPCInterface interface);

class IProcessPipe { };
class CCrossProcessPipe : IProcessPipe { };
class CSingleProcessPipe : IProcessPipe { };

SDK_Class CServerPipe
{
public:
	IProcessPipe* internalPipe;		//0x0
	IProcessPipe* singleProcessPipe;	//0x4
	HSteamPipe pipeHandle;				//0x8
	uint8_t __pad0xC[0x8];				//0xC
	int32_t pid;						//0x14
	int32_t threadId;					//0x18
	CUtlString processName;				//0x1C - Was empty on the stuff I tried, maybe it's defunct on linux?
	uint8_t __pad0x20[0x1];				//0x20
	HSteamUser userHandle;					//0x21
	uint8_t __pad0x25[0x7];				//0x25
	void* queueCallbackMsg;				//0x2C
	uint8_t __pad0x30[0x8];				//0x30
	uint32_t numQueuedCallbacks;		//0x38
	uint8_t __pad0x3C[0x14];			//0x3C
	CUtlVector<void> debugText;			//0x50
}; //0x60

SDK_Class CSteamEngine
{
public:
	CServerPipe* getServerPipe(const HSteamPipe pipe);
	CUser* getUser(const uint32_t index = 0);
	IClientUtils* getUtils();

	void setAppIdForCurrentPipe(const AppId_t appId);
};

extern CSteamEngine* g_pSteamEngine;
