#include "patterns.hpp"

#include "globals.hpp"
#include "memhlp.hpp"

#include "libmem/libmem.h"


Pattern_t::Pattern_t(const char* name, const char* pattern, MemHlp::SigFollowMode followMode, lm_module_t* module)
	:
	Pattern_t(name, pattern, followMode, std::vector<int16_t>(), module)
{
}

Pattern_t::Pattern_t(const char* name, const char* pattern, MemHlp::SigFollowMode followMode, std::vector<int16_t> prologue, lm_module_t* module)
	:
	name(name),
	pattern(pattern),
	followMode(followMode),
	prologue(prologue),
	module(module)
{
	Patterns::patterns.emplace_back(this);
}

bool Pattern_t::find()
{
	address = MemHlp::searchSignature(name.c_str(), pattern.c_str(), module ? *module : *g_modSteamClient , followMode, &prologue[0], prologue.size());
	return address != LM_ADDRESS_BAD;
}

bool Patterns::init()
{
	LOG_DEBUG("Patterns::init()\n");

	bool found = true;

	for (auto& pattern : patterns)
	{
		if (!pattern->find())
		{
			found = false;
		}
	}

	return found;
}

using SigFollowMode = MemHlp::SigFollowMode;

namespace Patterns
{
	Pattern_t TraceIPC
	{
		"TraceIPC",
		"48 89 D7 C1 E8",
		SigFollowMode::PrologueUpwards,
		std::vector<int16_t> { 0x55, 0x41, 0x56, 0x41, 0x57, 0x41 }
	};

	namespace CAPIJob
	{
		Pattern_t SendAndRecv
		{
			"CAPIJob::SendAndRecv",
			"44 39 F0 0F 94 C0",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xF7, 0x89, 0x49, 0x57, 0x41 }
		};
	}

	namespace CAppDataCache
	{
		Pattern_t BParseResponseMessage
		{
			"CAppDataCache::BParseResponseMessage",
			"41 89 F0 89 DA",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xE5, 0x89, 0x48, 0x55 }
		};
	}

	namespace CSteamEngine
	{
		Pattern_t GetServerPipe
		{
			"CSteamEngine::GetServerPipe",
			"0F B7 CE 31 D2",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x2C, 0x74, 0xF6, 0x85 }
		};
		Pattern_t SetAppIdForCurrentPipe
		{
			"CSteamEngine::SetAppIdForCurrentPipe",
			"0F 43 C6 83 FB",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x55, 0x41, 0x56, 0x41, 0x57, 0x41 }
		};
		Pattern_t ProcessIPCFrame
		{
			"CSteamEngine::ProcessIPCFrame",
			"48 89 DA 44 89 E6 5B",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xCD, 0x89, 0x49, 0x55, 0x41 }
		};
		Pattern_t Offset_User
		{
			"CSteamEngine::m_pUser",
			"4C 03 A5 ? ? ? ? 48 89 DF 4C 89 E2 E8 ? ? ? ? 49 8B 74 24",
			SigFollowMode::None
		};
	}

	namespace CUser
	{
		Pattern_t CheckAppOwnership
		{
			"CUser::CheckAppOwnership",
			"48 89 E8 31 ED",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xD6, 0x89, 0x49, 0x56, 0x41, 0x57, 0x41 }
		};
		Pattern_t GetSubscribedApps
		{
			"CUser::GetSubscribedApps",
			"EB ? 41 8B 50",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x55, 0x41, 0x56, 0x41, 0x57, 0x41 }
		};
		Pattern_t PostCallback
		{
			"CUser::PostCallback",
			"E8 ? ? ? ? 4D 8B AE",
			SigFollowMode::Relative
		};
		Pattern_t PostCallbackToAppId
		{
			"CUser::PostCallbackToAppId",
			"44 3B 69 ? 41 89 C5",
			MemHlp::SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x55, 0x41, 0x56, 0x41, 0x57, 0x41 }
		};
		Pattern_t SpawnGameId
		{
			"CUser::SpawnGameId",
			"31 C0 41 80 78",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xE5, 0x89, 0x48, 0x55 }
		};
		Pattern_t UpdateAppOwnershipTicket
		{
			"CUser::UpdateAppOwnershipTicket",
			"41 89 CD 83 FD",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x55, 0x41, 0x56, 0x41, 0x57, 0x41 }
		};
	}

	namespace CUserAppManager
	{
		Pattern_t BuildDepotDependency
		{
			"CUserAppManager::BuildDepotDependency",
			"4D 85 FF 88 44 24",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xCF, 0x89, 0x4D, 0x57, 0x41 }
		};
	}

	namespace CWebSocketConnection
	{
		Pattern_t BBuildAndAsyncSendFrame
		{
			"CWebSocketConnection::BBuildAndAsyncSendFrame",
			"45 31 F6 83 7F",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x55, 0x41, 0x56, 0x41, 0x57, 0x41 }
		};
	}

	namespace IClientUtils
	{
		Pattern_t Offset_GetPipeIndex
		{
			"IClientUtils::m_PipeIndex",
			"8B B7 ? ? ? ? 64 48 8B 1C 25 ? ? ? ? 48 89 5C 24 ? 48 89 FB 48 81 EF",
			SigFollowMode::None,
		};
	}

	std::vector<Pattern_t*> patterns;
}
