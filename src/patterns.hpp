#pragma once

#include "memhlp.hpp"

#include "libmem/libmem.h"

#include <string>
#include <vector>


struct Pattern_t
{
public:
	const std::string name;
	const std::string pattern;
	const MemHlp::SigFollowMode followMode;
	std::vector<uint8_t> prologue;

	lm_address_t address;
	lm_module_t* module;

	Pattern_t(const char* name, const char* pattern, MemHlp::SigFollowMode followMode, lm_module_t* module = nullptr);
	Pattern_t(const char* name, const char* pattern, MemHlp::SigFollowMode followMode, std::vector<uint8_t> prologue, lm_module_t* module = nullptr);
	//~CPattern();

	bool find();
};

namespace Patterns
{
	extern Pattern_t GetManifest;
	extern Pattern_t TraceIPC;

	namespace CAPIJob
	{
		extern Pattern_t SendAndRecv;
	}

	namespace CAppDataCache
	{
		extern Pattern_t BParseResponseMessage;
	}

	namespace CSteamEngine
	{
		extern Pattern_t SetAppIdForCurrentPipe;
		extern Pattern_t RunInterface;

		extern Pattern_t Offset_User;
	}

	namespace CWebSocketConnection
	{
		extern Pattern_t BBuildAndAsyncSendFrame;
	}

	namespace CUser
	{
		//TODO: Order & Convert old patterns
		extern Pattern_t CheckAppOwnership;
		extern Pattern_t GetSubscribedApps;
		extern Pattern_t PostCallback;
		extern Pattern_t UpdateAppOwnershipTicket;
	}

	namespace CUserAppManager
	{
		extern Pattern_t BuildDepotDependency;
	}

	namespace IClientAppManager
	{
		extern Pattern_t RunIPCFrame;
	}

	namespace IClientApps
	{
		extern Pattern_t RunIPCFrame;
	}

	namespace IClientInventory
	{
		extern Pattern_t RunIPCFrame;
	}

	namespace IClientRemoteStorage
	{
		extern Pattern_t RunIPCFrame;
	}

	namespace IClientUser
	{
		extern Pattern_t GetSteamId;
		extern Pattern_t RunIPCFrame;
	}

	namespace IClientUtils
	{
		extern Pattern_t RunIPCFrame;
		extern Pattern_t Offset_GetPipeIndex;
	}


	//steamui.so
	namespace ISteamMatchmakingPingResponse
	{
		extern Pattern_t ServerResponded;
	}

	extern std::vector<Pattern_t*> patterns;
	bool init();
}
