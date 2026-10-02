#include "CSteamEngine.hpp"

#include "CUtl.hpp"
#include "IClientUtils.hpp"
#include "types.hpp"

#include "../hooks.hpp"
#include "../patterns.hpp"

#include "libmem/libmem.h"

#include <sstream>


std::string EIPCCmd_ToString(const EIPCCmd cmd)
{
	switch(cmd)
	{
		case EIPCCmd::RunInterface:
			return "RunInterface";
		case EIPCCmd::SerializeCallbacks:
			return "SerializeCallbacks";
		case EIPCCmd::CreateGlobalUser:
			return "CreateGlobalUser";
		case EIPCCmd::DisconnectGlobalUser:
			return "DisconnectGlobalUser";
		case EIPCCmd::ClosePipe:
			return "ClosePipe";
		case EIPCCmd::Heartbeat:
			return "Heartbeat";
		case EIPCCmd::ConnectPipe:
			return "ConnectPipe";
	}

	std::ostringstream ss;
	ss << "Unkown IPCCmd 0x" << std::hex << static_cast<unsigned int>(cmd);

	return ss.str();
}

std::string EIPCInterface_ToString(const EIPCInterface interface)
{
	switch(interface)
	{
		case EIPCInterface::User:
			return "User";
		case EIPCInterface::GameServerInternal:
			return "GameServerInternal";
		case EIPCInterface::Friends:
			return "Friends";
		case EIPCInterface::Utils:
			return "Utils";
		case EIPCInterface::Billing:
			return "Billing";
		case EIPCInterface::Matchmaking:
			return "Matchmaking";
		case EIPCInterface::Apps:
			return "Apps";
		case EIPCInterface::UserStats:
			return "UserStats";
		case EIPCInterface::Networking:
			return "Networking";
		case EIPCInterface::RemoteStorage:
			return "RemoteStorage";
		case EIPCInterface::DepotBuilder:
			return "DepotBuilder";
		case EIPCInterface::AppManager:
			return "AppManager";
		case EIPCInterface::ConfigStore:
			return "ConfigStore";
		case EIPCInterface::GameCoordinator:
			return "GameCoordinator";
		case EIPCInterface::GameServerStats:
			return "GameServerStats";
		case EIPCInterface::GameStats:
			return "GameStats";
		case EIPCInterface::HTTP:
			return "HTTP";
		case EIPCInterface::Screenshots:
			return "Screenshots";
		case EIPCInterface::Audio:
			return "Audio";
		case EIPCInterface::UnifiedMessages:
			return "UnifiedMessages";
		case EIPCInterface::StreamLauncher:
			return "StreamLauncher";
		case EIPCInterface::ParentalSettings:
			return "ParentalSettings";
		case EIPCInterface::NetworkDeviceManager:
			return "NetworkDeviceManager";
		case EIPCInterface::Music:
			return "Music";
		case EIPCInterface::RemoteClientManager:
			return "RemoteClientManager";
		case EIPCInterface::UGC:
			return "UGC";
		case EIPCInterface::StreamClient:
			return "StreamClient";
		case EIPCInterface::ProductBuilder:
			return "ProductBuilder";
		case EIPCInterface::Shortcuts:
			return "Shortcuts";
		case EIPCInterface::GameNotifications:
			return "GameNotifications";
		case EIPCInterface::Video:
			return "Video";
		case EIPCInterface::Inventory:
			return "Inventory";
		case EIPCInterface::VR:
			return "VR";
		case EIPCInterface::ControllerSerialized:
			return "ControllerSerialized";
		case EIPCInterface::AppDisableUpdate:
			return "AppDisableUpdate";
		case EIPCInterface::SharedConnection:
			return "SharedConnection";
		case EIPCInterface::Shader:
			return "Shader";
		case EIPCInterface::NetworkingSocketsSerialized:
			return "NetworkingSocketsSerialized";
		case EIPCInterface::Compat:
			return "Compat";
		case EIPCInterface::Parties:
			return "Parties";
		case EIPCInterface::NetworkingUtilsSerialized:
			return "NetworkingUtilsSerialized";
		case EIPCInterface::RemotePlay:
			return "RemotePlay";
		case EIPCInterface::GameServerPacketHandler:
			return "GameServerPacketHandler";
		case EIPCInterface::SystemManager:
			return "SystemManager";
		case EIPCInterface::SystemPerfManager:
			return "SystemPerfManager";
		case EIPCInterface::SystemDockManager:
			return "SystemDockManager";
		case EIPCInterface::SystemAudioManager:
			return "SystemAudioManager";
		case EIPCInterface::SystemDisplayManager:
			return "SystemDisplayManager";
		case EIPCInterface::Timeline:
			return "Timeline";
	}

	std::ostringstream ss;
	ss << "Unkown IPCInterface 0x" << std::hex << static_cast<unsigned int>(interface);

	return ss.str();
}

CServerPipe* CSteamEngine::getServerPipe(const HSteamPipe pipe)
{
	const static auto fn = reinterpret_cast<CServerPipe*(*)(void*, HSteamPipe)>(Patterns::CSteamEngine::GetServerPipe.address);
	return fn(this, pipe);
}

CUser* CSteamEngine::getUser(const uint32_t index)
{
	const static auto offset = *reinterpret_cast<lm_address_t*>(Patterns::CSteamEngine::Offset_User.address + 2);
	const auto vec = reinterpret_cast<const CUtlVector<CUser*>*>(this + offset);
	if (index >= vec->size)
	{
		return nullptr;
	}

	return *(&vec->mem.base[index * 2] + 1);

	//const auto ppUserMap = *reinterpret_cast<uint8_t**>(this + offset);
	//const auto ppUser = ppUserMap + index * 8;
	//return *reinterpret_cast<CUser**>(ppUser + 4);
}

IClientUtils* CSteamEngine::getUtils()
{
	if (!getUser())
	{
		return nullptr;
	}

	const static lm_address_t offset = MemHlp::searchOffsetByTypeName(this, "12CSteamEngine");
	return reinterpret_cast<IClientUtils*>(this + offset);
}

void CSteamEngine::setAppIdForCurrentPipe(const AppId_t appId)
{
	//Last argument needs to be 0, otherwise steam crashes.
	//Might be only 1 when steam first sets it, then 0
	Hooks::CSteamEngine_SetAppIdForCurrentPipe->tramp.fn(this, appId, 0);
}

CSteamEngine* g_pSteamEngine = nullptr;
