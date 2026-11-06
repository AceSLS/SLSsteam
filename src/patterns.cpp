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
	#ifdef AARCH64
		"01 10 80 D2 63 00 00 4B",
		SigFollowMode::PrologueUpwards,
		std::vector<int16_t> { 0xA9, 0xAA, 0x7B, 0xFD }
	#else
		"48 89 D7 C1 E8",
		SigFollowMode::PrologueUpwards,
		std::vector<int16_t> { 0x55, 0x41, 0x56, 0x41, 0x57, 0x41 }
	#endif
	};

	namespace CAPIJob
	{
		Pattern_t SendAndRecv
		{
			"CAPIJob::SendAndRecv",
		#ifdef AARCH64
			"F7 03 05 2A 01 10 40 F9",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xA9, 0xBB, 0x7B, 0xFD }
		#else
			"44 39 F0 0F 94 C0",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xF7, 0x89, 0x49, 0x57, 0x41 }
		#endif
		};
	}

	namespace CAppDataCache
	{
		Pattern_t BParseResponseMessage
		{
			"CAppDataCache::BParseResponseMessage",
		#ifdef AARCH64
			"FB 03 01 AA 00 00 40 B9",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xA9, 0xAC, 0x7B, 0xFD }
		#else
			"41 89 F0 89 DA",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xE5, 0x89, 0x48, 0x55 }
		#endif
		};
	}

	namespace CSteamEngine
	{
		Pattern_t GetServerPipe
		{
			"CSteamEngine::GetServerPipe",
		#ifdef AARCH64
			"44 68 40 B9 23 3C 00 12",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xAA, 0x00, 0x03, 0xE2 }
		#else
			"0F B7 CE 31 D2",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x2C, 0x74, 0xF6, 0x85 }
		#endif
		};
		Pattern_t SetAppIdForCurrentPipe
		{
			"CSteamEngine::SetAppIdForCurrentPipe",
		#ifdef AARCH64
			"FF 03 02 D1 3F 24 00 71",
			SigFollowMode::None,
		#else
			"0F 43 C6 83 FB",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x55, 0x41, 0x56, 0x41, 0x57, 0x41 }
		#endif
		};
		Pattern_t ProcessIPCFrame
		{
			"CSteamEngine::ProcessIPCFrame",
		#ifdef AARCH64
			"E0 03 02 AA E1 3F 00 B9",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xA9, 0xBC, 0x7B, 0xFD }
		#else
			"48 89 DA 44 89 E6 5B",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xCD, 0x89, 0x49, 0x55, 0x41 }
		#endif
		};
		Pattern_t Offset_User
		{
			"CSteamEngine::m_pUser",
		#ifdef AARCH64
			"? ? ? F9 ? ? ? AA ? ? ? 2A ? ? ? 52 ? ? ? 8B ? ? ? F9 ? ? ? ? ? ? ? B9 ? ? ? 91 ? ? ? 6B ? ? ? ? ? ? ? B9 ? ? ? B9",
		#else
			"4C 03 A5 ? ? ? ? 48 89 DF 4C 89 E2 E8 ? ? ? ? 49 8B 74 24",
		#endif
			SigFollowMode::None
		};
	}

	namespace CUser
	{
		Pattern_t CheckAppOwnership
		{
			"CUser::CheckAppOwnership",
		#ifdef AARCH64
			"FF 5B 00 F9 E1 D3 03 91",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xA9, 0xAF, 0x7B, 0xFD }
		#else
			"48 89 E8 31 ED",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xD6, 0x89, 0x49, 0x56, 0x41, 0x57, 0x41 }
		#endif
		};
		Pattern_t GetSubscribedApps
		{
			"CUser::GetSubscribedApps",
		#ifdef AARCH64
			"FB 03 00 AA FC 03 02 91",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xA9, 0xB4, 0x7B, 0xFD }
		#else
			"EB ? 41 8B 50",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x55, 0x41, 0x56, 0x41, 0x57, 0x41 }
		#endif
		};
		Pattern_t PostCallback
		{
			"CUser::PostCallback",
		#ifdef AARCH64
			//2026-10-13 14:05:20.391 XREF Signature #1 @ 0x62d48c (sub_62D398): ? ? ? ? 74 6E 41 B9 60 82 41 B9
			//2026-10-13 14:05:20.406 XREF Signature #2 @ 0x68590c (sub_685790): ? ? ? ? 21 0F 40 F9 94 06 00 11
			//2026-10-13 14:05:20.406 XREF Signature #3 @ 0x604288 (sub_6040B8): ? ? ? ? ? ? ? ? C0 DE 70 D3 00 6C 1C 12
			//2026-10-13 14:05:20.406 XREF Signature #4 @ 0x6a5d2c (sub_6A5CD0): ? ? ? ? 61 12 41 39 ? ? ? ? E0 B6 40 F9
			//2026-10-13 14:05:20.406 XREF Signature #5 @ 0x6a753c (sub_6A72C8): ? ? ? ? ? ? ? ? 3F 03 00 71 24 E0 12 91
			"? ? ? ? ? ? ? ? C0 DE 70 D3 00 6C 1C 12",
		#else
			"E8 ? ? ? ? 4D 8B AE",
		#endif
			SigFollowMode::Relative
		};
		Pattern_t PostCallbackToAppId
		{
			"CUser::PostCallbackToAppId",
		#ifdef AARCH64
			"FD 7B B9 A9 3F 1C 00 71",
			SigFollowMode::None,
		#else
			"44 3B 69 ? 41 89 C5",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x55, 0x41, 0x56, 0x41, 0x57, 0x41 }
		#endif
		};
		Pattern_t SpawnGameId
		{
			"CUser::SpawnGameId",
		#ifdef AARCH64
			"F6 03 00 AA F5 03 06 2A",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xD2, 0x82, 0x20, 0x0C }
		#else
			"31 C0 41 80 78",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xE5, 0x89, 0x48, 0x55 }
		#endif
		};
		Pattern_t UpdateAppOwnershipTicket
		{
			"CUser::UpdateAppOwnershipTicket",
		#ifdef AARCH64
			"? ? ? F9 ? ? ? ? ? ? ? B8 ? ? ? 31 ? ? ? ? ? ? ? 93 ? ? ? 8B ? ? ? B9 ? ? ? 6B ? ? ? ? ? ? ? ? ? ? ? B9 ? ? ? 31 ? ? ? ? ? ? ? D2 ? ? ? ? ? ? ? AA ? ? ? ? ? ? ? 2A",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xA9, 0xB6, 0x7B, 0xFD, 0x34, -1, -1, -1 }
		#else
			"41 89 CD 83 FD",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x55, 0x41, 0x56, 0x41, 0x57, 0x41 }
		#endif
		};
	}

	namespace CUDPConnection
	{
		Pattern_t BAsyncSend
		{
			"CUDPConnection::BAsyncSend",
		#ifdef AARCH64
			"15 00 80 52 00 10 40 B9",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xA9, 0xBC, 0x7B, 0xFD }
		#else
			"4C 63 ED 45 31 C0",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x83, -1, -1, 0x83, -1, -1, 0x8B }
		#endif
		};
	};

	namespace CUserAppManager
	{
		Pattern_t BuildDepotDependency
		{
			"CUserAppManager::BuildDepotDependency",
		#ifdef AARCH64
			"FB 73 05 A9 08 01 40 B9",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xD1, 0x0A, 0xC3, 0xFF }
		#else
			"4D 85 FF 88 44 24",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xCF, 0x89, 0x4D, 0x57, 0x41 }
		#endif
		};
	}

	namespace CWebSocketConnection
	{
		Pattern_t BBuildAndAsyncSendFrame
		{
			"CWebSocketConnection::BBuildAndAsyncSendFrame",
		#ifdef AARCH64
			"F3 03 03 2A 80 00 40 B9",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0xA9, 0xB0, 0x7B, 0xFD }
		#else
			"45 31 F6 83 7F",
			SigFollowMode::PrologueUpwards,
			std::vector<int16_t> { 0x55, 0x41, 0x56, 0x41, 0x57, 0x41 }
		#endif
		};
	}

	namespace IClientUtils
	{
		Pattern_t Offset_GetPipeIndex
		{
			"IClientUtils::m_PipeIndex",
		#ifdef AARCH64
			"E1 30 82 92 02 00 01 8B",
		#else
			"8B B7 ? ? ? ? 64 48 8B 1C 25 ? ? ? ? 48 89 5C 24 ? 48 89 FB 48 81 EF",
		#endif
			SigFollowMode::None,
		};
	}

	std::vector<Pattern_t*> patterns;
}
