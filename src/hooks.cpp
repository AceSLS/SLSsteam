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
#include "memhlp.hpp"
#include "patterns.hpp"
#include "vftableinfo.hpp"

#include "libmem/libmem.h"

#include <atomic>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <pthread.h>
#include <strings.h>
#include <unordered_map>
#include <unordered_set>
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
bool DetourHook<T>::setup(const char* name, const lm_address_t fn, T hookFn)
{
	this->name = name;
	this->originalFn.address = fn;
	this->hookFn.fn = hookFn;

	return true;
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
	if (!this->size)
	{
		return;
	}

	LM_UnhookCode(this->originalFn.address, this->tramp.address, this->size);
	this->size = 0;

	LOG_DEBUG("Unhooked %s\n", this->name.c_str());
}

template<typename T>
void VFTHook<T>::place()
{
	LM_VmtHook(this->vft.get(), this->index, this->hookFn.address);
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
	//No clue how libmem reacts when unhooking a non existent hook
	//so we do this
	if (!this->hooked)
	{
		return;
	}

	LM_VmtUnhook(this->vft.get(), this->index);
	this->hooked = false;

	LOG_DEBUG("Unhooked %s!\n", this->name.c_str());
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

__attribute__((hot))
static void hkTraceIPC(const char* iface, const char* fn)
{
	if (g_config.extendedLogging.get())
	{
		LOG_DEBUG
		(
			"%s(%s, %s)\n",

			Hooks::TraceIPC.name.c_str(),
			iface,
			fn
		);
	}

	LOG_TRACE("Calling tramp\n");
	Hooks::TraceIPC.tramp.fn(iface, fn);
}

static uint32_t hkAPIJob_SendAndRecv(CAPIJob* pAPIJob, CProtoBufMsgBase* send, uint32_t a2, uint32_t timeOut, CProtoBufMsgBase* recv, EMsg targetType)
{
	uint32_t ret = Achievements::sendAndRecvGetUserStats(pAPIJob, send, timeOut, recv, targetType);

	if (!ret)
	{
		LOG_TRACE("Calling tramp\n");
		ret = Hooks::CAPIJob_SendAndRecv.tramp.fn(pAPIJob, send, a2, timeOut, recv, targetType);
	}

	LOG_DEBUG
	(
		"%s(%p, %s, %u, %u, %s, %u) -> %u\n",

		Hooks::CAPIJob_SendAndRecv.name.c_str(),
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
	const uint32_t ret = Hooks::CAppDataCache_BParseResponseFromMessage.tramp.fn(pAppDataCache, pMsg);

	LOG_DEBUG
	(
		"%s(%p, %p) -> %i\n",
		Patterns::CAppDataCache::BParseResponseMessage.name.c_str(),
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
		ret = Hooks::CClientUnifiedServiceMethod_SendAndRecvMsg.tramp.fn(pUnifiedServiceTransport, name, send, recv, arg4);
	}

	Apps::sendAndRecvLastPlayedTimes(name, reinterpret_cast<CPlayer_GetLastPlayedTimes_Response*>(recv));

	LOG_DEBUG
	(
		"%s(%p, %s, %s, %s, %p) -> %u\n",

		Hooks::CClientUnifiedServiceMethod_SendAndRecvMsg.name.c_str(),
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
		"RecvPkt with CMInterface at %p %s -> 0x%x\n",
		reinterpret_cast<void*>(pCMInterface),
		pNetPacket->getProtoBufTypeName().c_str(),
		pNetPacket->getType()
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
		const bool disableFamilyShareLock = g_config.disableFamilyLock.get();

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

		Misc::recvMsg(pNetPacket);
		Ticket::recvMsg(pNetPacket);
	}

	LOG_TRACE("Calling tramp\n");
	Hooks::CCMInterface_RecvPkt.tramp.fn(pCMInterface, pNetPacket);
}

namespace
{
	constexpr uint32_t REMOTE_APP_STATUS_EMSG = 9502;
	constexpr uint32_t REMOTE_APP_CANDIDATE_FIELDS = 0x1fc;
	constexpr size_t MIN_FULL_REMOTE_STATUS_ENTRIES = 64;
	constexpr size_t MAX_REMOTE_MESSAGE_SIZE = 64 * 1024 * 1024;

	struct RemoteAppStatusEntry
	{
		AppId_t appId = 0;
		uint32_t appState = 0;
		uint32_t presentFields = 0;
		size_t begin = 0;
		size_t end = 0;
	};

	struct ParsedRemoteAppStatus
	{
		bool valid = false;
		std::vector<RemoteAppStatusEntry> entries;
	};

	bool readVarInt(const uint8_t*& cursor, const uint8_t* end, uint64_t& value)
	{
		value = 0;
		for (unsigned int shift = 0; shift < 64 && cursor < end; shift += 7)
		{
			const uint8_t byte = *cursor++;
			value |= static_cast<uint64_t>(byte & 0x7f) << shift;
			if (!(byte & 0x80))
			{
				return true;
			}
		}
		return false;
	}

	bool skipProtoField(const uint32_t wireType, const uint8_t*& cursor, const uint8_t* end)
	{
		uint64_t value = 0;
		switch (wireType)
		{
			case 0:
				return readVarInt(cursor, end, value);
			case 1:
				if (static_cast<size_t>(end - cursor) < 8)
				{
					return false;
				}
				cursor += 8;
				return true;
			case 2:
				if (!readVarInt(cursor, end, value) || value > static_cast<uint64_t>(end - cursor))
				{
					return false;
				}
				cursor += value;
				return true;
			case 5:
				if (static_cast<size_t>(end - cursor) < 4)
				{
					return false;
				}
				cursor += 4;
				return true;
			default:
				return false;
		}
	}

	bool parseRemoteAppStatusEntry(
		const uint8_t* cursor,
		const uint8_t* end,
		RemoteAppStatusEntry& entry)
	{
		while (cursor < end)
		{
			uint64_t tag = 0;
			if (!readVarInt(cursor, end, tag) || !tag)
			{
				return false;
			}

			const uint32_t field = tag >> 3;
			const uint32_t wireType = tag & 7;
			uint64_t value = 0;

			if ((field == 1 || field == 2) && wireType == 0)
			{
				if (!readVarInt(cursor, end, value))
				{
					return false;
				}

				if (field == 1)
				{
					entry.appId = value;
					entry.presentFields |= 1 << 7;
				}
				else
				{
					entry.appState = value;
					entry.presentFields |= 1 << 8;
				}
				continue;
			}

			if (field >= 3 && field <= 9)
			{
				entry.presentFields |= 1 << (field - 3);
			}

			if (!skipProtoField(wireType, cursor, end))
			{
				return false;
			}
		}

		return cursor == end;
	}

	ParsedRemoteAppStatus parseRemoteAppStatus(const uint8_t* body, const size_t size)
	{
		ParsedRemoteAppStatus parsed;
		const uint8_t* cursor = body;
		const uint8_t* const end = body + size;

		while (cursor < end)
		{
			const size_t fieldBegin = cursor - body;
			uint64_t tag = 0;
			if (!readVarInt(cursor, end, tag) || !tag)
			{
				return parsed;
			}

			const uint32_t field = tag >> 3;
			const uint32_t wireType = tag & 7;
			if (field != 1 || wireType != 2)
			{
				if (!skipProtoField(wireType, cursor, end))
				{
					return parsed;
				}
				continue;
			}

			uint64_t entrySize = 0;
			if (!readVarInt(cursor, end, entrySize) || entrySize > static_cast<uint64_t>(end - cursor))
			{
				return parsed;
			}

			RemoteAppStatusEntry entry;
			entry.begin = fieldBegin;
			const uint8_t* const entryEnd = cursor + entrySize;
			if (!parseRemoteAppStatusEntry(cursor, entryEnd, entry))
			{
				return parsed;
			}
			cursor = entryEnd;
			entry.end = cursor - body;
			parsed.entries.emplace_back(entry);
		}

		parsed.valid = true;
		return parsed;
	}

	bool getRemoteMessageBody(
		const std::vector<uint8_t>& message,
		size_t& bodyOffset)
	{
		if (message.size() < sizeof(CNetPacketBody))
		{
			return false;
		}

		uint32_t type = 0;
		uint32_t headerSize = 0;
		memcpy(&type, message.data(), sizeof(type));
		memcpy(&headerSize, message.data() + sizeof(type), sizeof(headerSize));
		bodyOffset = sizeof(CNetPacketBody) + headerSize;

		return (type & ~PROTOBUF_TYPE_MASK) == REMOTE_APP_STATUS_EMSG
			&& bodyOffset <= message.size();
	}

	bool serializeRemoteClientMessage(
		CProtoBufMsgBase* pMsg,
		std::vector<uint8_t>& message)
	{
		if (!pMsg)
		{
			return false;
		}

		const auto vtable = *reinterpret_cast<uintptr_t**>(pMsg);
		if (!vtable || !vtable[4] || !vtable[5])
		{
			return false;
		}

		using GetSize_t = uint32_t(*)(void*);
		using Serialize_t = uint32_t(*)(void*, void*, uint32_t);
		const auto getSize = reinterpret_cast<GetSize_t>(vtable[4]);
		const auto serialize = reinterpret_cast<Serialize_t>(vtable[5]);
		const uint32_t size = getSize(pMsg);
		if (!size || size > MAX_REMOTE_MESSAGE_SIZE)
		{
			return false;
		}

		message.resize(size);
		return serialize(pMsg, message.data(), size) == size;
	}

	void writeVarInt(std::vector<uint8_t>& message, uint64_t value)
	{
		while (value >= 0x80)
		{
			message.emplace_back(static_cast<uint8_t>(value & 0x7f) | 0x80);
			value >>= 7;
		}
		message.emplace_back(value);
	}

	void appendRemoteAppStatus(std::vector<uint8_t>& body, const AppId_t appId)
	{
		//Match Steam's native AdditionalApps status entries.
		std::vector<uint8_t> status;
		status.emplace_back(1 << 3);
		writeVarInt(status, appId);
		status.emplace_back(2 << 3);
		writeVarInt(status, 4);
		status.insert(status.end(), { 5 << 3, 1, 6 << 3, 0, 7 << 3, 0, 8 << 3, 1, 9 << 3 | 2, 0 });

		body.emplace_back(1 << 3 | 2);
		writeVarInt(body, status.size());
		body.insert(body.end(), status.begin(), status.end());
	}

	std::mutex remoteStatusCacheMutex;
	std::vector<uint8_t> remoteStatusPrefix;
	std::vector<uint8_t> remoteStatusBaseBody;
	std::unordered_map<void*, uint64_t> remoteClientSyncGeneration;

	bool cacheRemoteAppStatus(
		const std::vector<uint8_t>& message,
		const size_t bodyOffset,
		const ParsedRemoteAppStatus& parsed)
	{
		if (!parsed.valid || parsed.entries.size() < MIN_FULL_REMOTE_STATUS_ENTRIES)
		{
			return false;
		}

		const auto addedApps = g_config.addedAppIds.get();
		const uint8_t* const body = message.data() + bodyOffset;
		const size_t bodySize = message.size() - bodyOffset;
		std::vector<uint8_t> baseBody;
		baseBody.reserve(bodySize);

		size_t cursor = 0;
		for (const auto& entry : parsed.entries)
		{
			baseBody.insert(baseBody.end(), body + cursor, body + entry.begin);
			if (!addedApps.contains(entry.appId))
			{
				baseBody.insert(baseBody.end(), body + entry.begin, body + entry.end);
			}
			cursor = entry.end;
		}
		baseBody.insert(baseBody.end(), body + cursor, body + bodySize);

		const std::lock_guard lock(remoteStatusCacheMutex);
		remoteStatusPrefix.assign(message.begin(), message.begin() + bodyOffset);
		remoteStatusBaseBody = std::move(baseBody);
		return true;
	}

	bool buildRemoteAppStatus(std::vector<uint8_t>& message)
	{
		{
			const std::lock_guard lock(remoteStatusCacheMutex);
			if (remoteStatusPrefix.empty())
			{
				return false;
			}
			message = remoteStatusPrefix;
			message.insert(message.end(), remoteStatusBaseBody.begin(), remoteStatusBaseBody.end());
		}

		auto addedApps = g_config.addedAppIds.get();
		std::vector<AppId_t> sortedApps(addedApps.begin(), addedApps.end());
		std::sort(sortedApps.begin(), sortedApps.end());
		for (const auto appId : sortedApps)
		{
			appendRemoteAppStatus(message, appId);
		}
		return true;
	}

	struct CachedRemoteMessageVTable
	{
		void* unused[4] {};
		uint32_t(*getSize)(void*);
		uint32_t(*serialize)(void*, void*, uint32_t);
	};

	struct CachedRemoteMessage
	{
		CachedRemoteMessageVTable* vtable;
		const std::vector<uint8_t>* message;
	};

	uint32_t cachedRemoteMessageSize(void* pMessage)
	{
		const auto* cached = reinterpret_cast<CachedRemoteMessage*>(pMessage);
		return cached->message->size();
	}

	uint32_t serializeCachedRemoteMessage(void* pMessage, void* output, const uint32_t size)
	{
		const auto* cached = reinterpret_cast<CachedRemoteMessage*>(pMessage);
		const auto* message = cached->message;
		if (!message || size < message->size())
		{
			return 0;
		}
		memcpy(output, message->data(), message->size());
		return message->size();
	}

	//Enough of CProtoBufMsgBase for Steam's sender to serialize the cached packet.
	CachedRemoteMessageVTable cachedRemoteMessageVTable
	{
		{},
		cachedRemoteMessageSize,
		serializeCachedRemoteMessage
	};
}

static void hkRemoteClientManager_RecvPkt(
	void* pRemoteClientManager,
	CNetPacket* pNetPacket)
{
	bool updateRemoteSnapshot = false;
	std::unordered_set<AppId_t> remoteApps;
	CNetPacket replayPacket {};
	bool replayPacketValid = false;

	if (
		pNetPacket
		&& pNetPacket->isValid()
		&& pNetPacket->isProtoBuf()
	)
	{
		const uint32_t type = pNetPacket->getProtoBufType();
		const size_t bodyOffset = sizeof(CNetPacketBody) + pNetPacket->body->headerSize;
		if (bodyOffset <= pNetPacket->size && type == REMOTE_APP_STATUS_EMSG)
		{
			const auto* body = reinterpret_cast<const uint8_t*>(pNetPacket->body) + bodyOffset;
			const auto parsed = parseRemoteAppStatus(body, pNetPacket->size - bodyOffset);
			const bool syncEnabled = g_config.syncRemoteAdditionalApps.get();
			auto* usr = g_pSteamEngine ? g_pSteamEngine->getUser() : nullptr;

			if (parsed.valid && usr)
			{
				for (size_t i = 0; i < parsed.entries.size(); ++i)
				{
					const auto& entry = parsed.entries[i];
					if (
						entry.presentFields != REMOTE_APP_CANDIDATE_FIELDS
						|| entry.appState != 4
					)
					{
						continue;
					}

					AppOwnershipInfo_t ownership {};
					if (usr->checkAppOwnership(entry.appId, &ownership) && !ownership.ownsLicense)
					{
						remoteApps.emplace(entry.appId);
						LOG_DEBUG
						(
							"REMOTE CANDIDATE app=%u idx=%zu has=0x%x "
							"state=0x%x owns=%u expired=%u release=%d sub=%d\n",
							entry.appId,
							i,
							entry.presentFields,
							entry.appState,
							ownership.ownsLicense,
							ownership.licenseExpired,
							ownership.releaseState,
							ownership.subId
						);
					}
				}
			}

			static std::atomic<size_t> fullStatusEntryCount = 0;
			const size_t previousFullCount = fullStatusEntryCount.load();
			const size_t fullCountFloor = previousFullCount > 4
				? previousFullCount / 2
				: 2;
			const bool fullSnapshot = parsed.valid && parsed.entries.size() >= fullCountFloor;
			if (fullSnapshot)
			{
				fullStatusEntryCount.store(parsed.entries.size());
			}

			if (syncEnabled && fullSnapshot && usr)
			{
				updateRemoteSnapshot = true;
				void* const replayBody = Steam::Plat_Alloc(pNetPacket->size);
				if (replayBody)
				{
					memcpy(replayBody, pNetPacket->body, pNetPacket->size);
					replayPacket = *pNetPacket;
					replayPacket.body = reinterpret_cast<CNetPacketBody*>(replayBody);
					replayPacket.originalBody = replayPacket.body;
					replayPacket.refs = 1;
					replayPacketValid = true;
				}
			}

			const auto header = pNetPacket->deserializeHeader();
			const int32_t clientSessionId = header.has_client_sessionid()
				? header.client_sessionid()
				: -1;
			const uint64_t sourceJobId = header.has_jobid_source()
				? header.jobid_source()
				: 0;
			LOG_DEBUG
			(
				"RemoteAppStatus RX count=%zu candidates=%zu session=%d "
				"sourceJob=%llu packet=%p source=%p sync=%u full=%u fullCount=%zu\n",
				parsed.entries.size(),
				remoteApps.size(),
				clientSessionId,
				static_cast<unsigned long long>(sourceJobId),
				reinterpret_cast<void*>(pNetPacket),
				pRemoteClientManager,
				syncEnabled,
				fullSnapshot,
				fullStatusEntryCount.load()
			);
		}
	}

	Hooks::CRemoteClientManager_RecvPkt.tramp.fn(pRemoteClientManager, pNetPacket);

	const bool registered = updateRemoteSnapshot && Apps::updateRemoteApps(remoteApps);
	if (registered && replayPacketValid)
	{
		LOG_DEBUG("REMOTE STATUS REPLAY source=manager-recv\n");
		Hooks::CRemoteClientManager_RecvPkt.tramp.fn(pRemoteClientManager, &replayPacket);
	}

	if (replayPacket.body)
	{
		Steam::Plat_Free(replayPacket.body);
		replayPacket.body = nullptr;
	}
}

static bool hkRemoteClientManager_SendProtoBufMsg(
	void* pRemoteClientManager,
	CProtoBufMsgBase* pMsg,
	void* pRemoteClient)
{
	bool fullNativeStatus = false;
	if (
		pMsg
		&& static_cast<uint32_t>(pMsg->getProtoBufType()) == REMOTE_APP_STATUS_EMSG
	)
	{
		std::vector<uint8_t> message;
		size_t bodyOffset = 0;
		if (
			serializeRemoteClientMessage(pMsg, message)
			&& getRemoteMessageBody(message, bodyOffset)
		)
		{
			const auto parsed = parseRemoteAppStatus(
				message.data() + bodyOffset,
				message.size() - bodyOffset
			);
			fullNativeStatus = cacheRemoteAppStatus(message, bodyOffset, parsed);
			if (fullNativeStatus)
			{
				LOG_DEBUG("RemoteAppStatus TX cached count=%zu client=%p\n", parsed.entries.size(), pRemoteClient);
			}
		}
	}

	const bool sent = Hooks::CRemoteClientManager_SendProtoBufMsg.tramp.fn(
		pRemoteClientManager,
		pMsg,
		pRemoteClient
	);

	if (!g_config.syncRemoteAdditionalApps.get() || !pRemoteClient)
	{
		return sent;
	}

	const uint64_t generation = Apps::getRemoteAppBroadcastGeneration();
	if (fullNativeStatus)
	{
		const std::lock_guard lock(remoteStatusCacheMutex);
		remoteClientSyncGeneration[pRemoteClient] = generation;
		return sent;
	}

	{
		const std::lock_guard lock(remoteStatusCacheMutex);
		if (
			remoteStatusPrefix.empty()
			|| remoteClientSyncGeneration[pRemoteClient] >= generation
		)
		{
			return sent;
		}
	}

	std::vector<uint8_t> message;
	if (!buildRemoteAppStatus(message))
	{
		return sent;
	}

	{
		const std::lock_guard lock(remoteStatusCacheMutex);
		if (remoteClientSyncGeneration[pRemoteClient] >= generation)
		{
			return sent;
		}
		remoteClientSyncGeneration[pRemoteClient] = generation;
	}

	CachedRemoteMessage cachedMessage { &cachedRemoteMessageVTable, &message };
	const bool resent = Hooks::CRemoteClientManager_SendProtoBufMsg.tramp.fn(
		pRemoteClientManager,
		reinterpret_cast<CProtoBufMsgBase*>(&cachedMessage),
		pRemoteClient
	);
	LOG_DEBUG
	(
		"REMOTE STATUS RESEND generation=%llu bytes=%zu client=%p sent=%u\n",
		static_cast<unsigned long long>(generation),
		message.size(),
		pRemoteClient,
		resent
	);

	return sent;
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
	const bool log = g_config.extendedLogging.get();

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
		ret = Hooks::CSteamEngine_ProcessIPCFrame.tramp.fn(pSteamEngine, hPipe, pBufIn, pBufOut);

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
		ret = Hooks::CSteamEngine_ProcessIPCFrame.tramp.fn(pSteamEngine, hPipe, pBufIn, pBufOut);
	}

	if (cmd == EIPCCmd::ClosePipe)
	{
		FakeAppIds::closePipe(hPipe);
	}

	//LOG_DEBUG("In\n%s\n", MemHlp::hexdump(pBufIn->mem.base, pBufIn->offset).c_str());
	//LOG_DEBUG("Out\n%s\n", MemHlp::hexdump(pBufOut->mem.base, pBufOut->offset).c_str());

	return ret;
}

static AppId_t hkSteamEngine_SetAppIdForCurrentPipe(CSteamEngine* pSteamEngine, AppId_t appId, bool a2)
{
	FakeAppIds::setAppIdForCurrentPipe(appId);

	LOG_TRACE("Calling tramp\n");
	const AppId_t ret = Hooks::CSteamEngine_SetAppIdForCurrentPipe.tramp.fn(pSteamEngine, appId, a2);

	LOG_DEBUG
	(
		"%s(%p, %u, %i) -> %i\n",

		Hooks::CSteamEngine_SetAppIdForCurrentPipe.name.c_str(),
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

				Apps::sendMsg(&packet);
				FakeAppIds::sendMsg(&packet);
			}

			LOG_TRACE("Calling tramp\n");
			const bool success = Hooks::CWebSocketConnection_BBuildAndAsyncSendFrame.tramp.fn(pWebSocketConnection, type, packet.body, packet.size);
			packet.free();
			return success;
		}
	}

	LOG_TRACE("Calling tramp\n");
	return Hooks::CWebSocketConnection_BBuildAndAsyncSendFrame.tramp.fn(pWebSocketConnection, type, pData, dataSize);
}

static gameserverdetails_t* hkSteamMatchmakingServers_GetServerDetails(void* pSteamMatchmakingServers, uint32_t handle, uint32_t serverIdx)
{
	LOG_TRACE("Calling tramp\n");
	gameserverdetails_t* ret = Hooks::CSteamMatchmakingServers_GetServerDetails.tramp.fn(pSteamMatchmakingServers, handle, serverIdx);

	LOG_DEBUG
	(
		"%s(%p, 0x%x, %u) -> %p\n",

		Hooks::CSteamMatchmakingServers_GetServerDetails.name.c_str(),
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
	uint32_t handle = Hooks::CSteamMatchmakingServers_RequestInternetServerList.tramp.fn(pSteamMatchmakingServers, fake ? fake : appId, a2, a3, a4);

	LOG_DEBUG
	(
		"%s(%p, %u, 0x%x, 0x%x, 0x%x)->0x%x\n",

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
static uint32_t hkUser_CheckAppOwnership(CUser* pUser, AppId_t appId, AppOwnershipInfo_t* pOwnershipInfo)
{
	LOG_TRACE("Calling tramp\n");
	const uint32_t ret = Hooks::CUser_CheckAppOwnership.tramp.fn(pUser, appId, pOwnershipInfo);

	//Do not log pOwnershipInfo because it gets deleted very quickly, so it's pretty much useless in the logs
	LOG_ONCE
	(
		"%s(%p, %u) -> %i\n",

		Hooks::CUser_CheckAppOwnership.name.c_str(),
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
	uint32_t count = Hooks::CUser_GetSubscribedApps.tramp.fn(pUser, pAppList, size, a3);

	Apps::getSubscribedApps(pAppList, size, count);

	LOG_DEBUG
	(
		"%s(%p, %p, %i, %i) -> %i\n",

		Hooks::CUser_GetSubscribedApps.name.c_str(),
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
	const uint32_t ret = Hooks::CUser_PostCallbackToAppId.tramp.fn(pUser, appId, type, pCallback, callbackSize);

	LOG_DEBUG
	(
		"%s(%p, %u, %u, %p, %u) -> %u\n",

		Hooks::CUser_PostCallbackToAppId.name.c_str(),
		reinterpret_cast<void*>(pUser),
		appId,
		type,
		pCallback,
		callbackSize,
		ret
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
	const bool success = Hooks::CUserAppManager_BuildDepotDependency.tramp.fn(pAppManager, appId, a2, depots, sharedDepots, a5, pBuildId, a7);

	LOG_DEBUG
	(
		"%s(%p, %u) -> %i\n",
		Hooks::CUserAppManager_BuildDepotDependency.name.c_str(),
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
	const bool ret = Hooks::IClientAppManager_BCanRemotePlayTogether.originalFn.fn(pAppManager, appId);

	LOG_DEBUG
	(
		"%s(%p, %u) -> %u\n",
		Hooks::IClientAppManager_BCanRemotePlayTogether.name.c_str(),
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

			Hooks::IClientAppManager_LaunchApp.name.c_str(),
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
	return Hooks::IClientAppManager_LaunchApp.originalFn.fn(pAppManager, pAppId, a2, a3, a4);
}

static bool hkClientAppManager_IsAppDlcInstalled(IClientAppManager* pAppManager, AppId_t appId, AppId_t dlcId)
{
	LOG_TRACE("Calling original\n");
	const bool ret = Hooks::IClientAppManager_IsAppDlcInstalled.originalFn.fn(pAppManager, appId, dlcId);

	LOG_ONCE
	(
		"%s(%p, %u, %u) -> %i\n",

		Hooks::IClientAppManager_IsAppDlcInstalled.name.c_str(),
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

	const bool ret = Hooks::IClientAppManager_BIsDlcEnabled.originalFn.fn(pAppManager, appId, dlcId, a3);
	LOG_ONCE
	(
		"%s(%p, %u, %u, %p) -> %i\n",

		Hooks::IClientAppManager_BIsDlcEnabled.name.c_str(),
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

	const bool success = Hooks::IClientAppManager_GetAppUpdateInfo.originalFn.fn(pAppManager, appId, a2);
	LOG_ONCE
	(
		"%s(%p, %u, %p) -> %i\n",
		Hooks::IClientAppManager_GetAppUpdateInfo.name.c_str(),
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

static unsigned int hkClientApps_GetDLCCount(IClientApps* pClientApps, AppId_t appId)
{
	LOG_TRACE("Calling original\n");

	uint32_t count = Hooks::IClientApps_GetDLCCount.originalFn.fn(pClientApps, appId);
	LOG_ONCE
	(
		"%s(%p, %u) -> %u\n",

		Hooks::IClientApps_GetDLCCount.name.c_str(),
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
		|| Hooks::IClientApps_GetDLCDataByIndex.originalFn.fn(pClientApps, appId, dlcIndex, pDlcId, pIsAvailable, pChDlcName, dlcNameLen);


	LOG_ONCE
	(
		"%s(%p, %u, %i, %p, %p, %s, %i) -> %i\n",

		Hooks::IClientApps_GetDLCDataByIndex.name.c_str(),
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
	const uint32_t ret = Hooks::IClientFriends_GetFriendGamePlayed.tramp.fn(pClientFriends, steamId, gamePlayed);

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
	const bool enabled = Hooks::IClientRemoteStorage_IsCloudEnabledForApp.tramp.fn(pClientRemoteStorage, appId);
	LOG_ONCE
	(
		"%s(%p, %u) -> %i\n",

		Hooks::IClientRemoteStorage_IsCloudEnabledForApp.name.c_str(),
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
	const bool ret = Hooks::IClientUser_BLoggedOn.originalFn.fn(pClientUser);
	//Useless logging
	//LOG_DEBUG
	//(
	//	"%s(0x%x) -> %i\n",
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

static uint32_t hkClientUser_BUpdateAppOwnershipTicket(IClientUser* pClientUser, AppId_t appId, bool staleOnly)
{
	const auto cached = Ticket::getCachedTicket(appId);
	if (g_pSteamEngine->getUser(0)->isSubscribed(appId) && !cached)
	{
		staleOnly = false;
		LOG_DEBUG("Force re-requesting OwnershipInfo for %u\n", appId);
	}

	LOG_TRACE("Calling original\n");
	const uint32_t ret = Hooks::IClientUser_BUpdateAppOwnershipTicket.originalFn.fn(pClientUser, appId, staleOnly);

	LOG_DEBUG
	(
		"%s(%p, %u, %i) -> %u\n",

		Hooks::IClientUser_BUpdateAppOwnershipTicket.name.c_str(),
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
	const uint32_t size = Hooks::IClientUser_GetAppOwnershipTicketExtendedData.originalFn.fn
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

	LOG_ONCE("%s(%u)->%u\n", Hooks::IClientUser_GetAppOwnershipTicketExtendedData.name.c_str(), appId, size);

	if (size)
	{
		Ticket::getTicketOwnershipExtendedData(appId);
	}

	return size;
}

static bool hkClientUser_GetEncryptedAppTicket(IClientUser* pClientUser, void* pTicket, uint32_t ticketSize, uint32_t* pTicketSize)
{
	LOG_TRACE("Calling original\n");
	const bool success = Hooks::IClientUser_GetEncryptedAppTicket.originalFn.fn(pClientUser, pTicket, ticketSize, pTicketSize);

	LOG_DEBUG
	(
		"%s(%p, %p, %u, %p) -> %u\n",

		Hooks::IClientUser_GetEncryptedAppTicket.name.c_str(),
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

	return Hooks::IClientUser_GetLegacyCDKey.originalFn.fn(pClientUser, appId, pChKey, keySize);
}

static uint8_t hkClientUser_IsUserSubscribedAppInTicket(IClientUser* pClientUser, uint32_t steamId, uint32_t a2, uint32_t a3, AppId_t appId)
{
	LOG_TRACE("Calling original\n");
	const uint8_t ticketState = Hooks::IClientUser_IsUserSubscribedAppInTicket.originalFn.fn(pClientUser, steamId, a2, a3, appId);
	//LOG_ONCE("IClientUser::IsUserSubscribedAppInTicket(0x%x, %u, %u, %u, %u) -> %i\n", pClientUser, steamId, a2, a3, appId, ticketState);
	//Don't log the steamId, protect users from themselves and stuff
	LOG_ONCE
	(
		"%s(%p, %u, %u, %u) -> %i\n",

		Hooks::IClientUser_IsUserSubscribedAppInTicket.name.c_str(),
		reinterpret_cast<void*>(pClientUser),
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
	if (overrides.contains(realAppId))
	{
		const uint64_t& id64 = overrides.at(realAppId);
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

	//Use pipe AppId, getCachedEncryptedTicket handles FakeAppIds internally
	const auto ticket = Ticket::getCachedEncryptedTicket(utils->getAppId());
	if (ticket)
	{
		return ticket->steamId;
	}

	return steamId;
}

static AppId_t hkClientUtils_GetAppId(IClientUtils* pClientUtils)
{
	LOG_TRACE("Calling original\n");
	AppId_t appId = Hooks::IClientUtils_GetAppId.originalFn.fn(pClientUtils);

	LOG_DEBUG
	(
		"%s(%p) -> %u\n",

		Hooks::IClientUtils_GetAppId.name.c_str(),
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
	const bool ret = Hooks::IClientUtils_GetOfflineMode.originalFn.fn(pClientUtils);

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
	Hooks::CGameInfoDialog_ServerResponded.tramp.fn(pSteamMatchingPingResponse, details);

	LOG_DEBUG
	(
		"%s(%p, %p) for %u\n",
		Hooks::CGameInfoDialog_ServerResponded.name.c_str(),
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

	return Hooks::IClientCompat_BIsCompatLayerEnabled.tramp.fn(pClientCompat);
}

static bool hkClientConfigStore_SetString(void* pClientConfigStore, uint32_t store, const char* key, const char* value)
{
	LOG_TRACE("Calling tramp\n");
	const bool success = Hooks::IClientConfigStore_SetString.tramp.fn(pClientConfigStore, store, key, value);

	//LOG_DEBUG
	//(
	//	"%s(0x%x, %u, %s, %s) -> %u\n",

	//	Hooks::IClientConfigStore_SetString.name.c_str(),
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
	//TODO: Lazily intialize in a different way, or preload glibc
	DetourHook<TraceIPC_t> TraceIPC;

	DetourHook<CAPIJob_SendAndRecv_t> CAPIJob_SendAndRecv;

	DetourHook<CAppDataCache_BParseResponseFromMessage_t> CAppDataCache_BParseResponseFromMessage;

	DetourHook<CClientUnifiedServiceMethod_SendAndRecvMsg_t> CClientUnifiedServiceMethod_SendAndRecvMsg;

	DetourHook<CCMInterface_RecvPkt_t> CCMInterface_RecvPkt;

	DetourHook<CRemoteClientManager_RecvPkt_t> CRemoteClientManager_RecvPkt;
	DetourHook<CRemoteClientManager_SendProtoBufMsg_t> CRemoteClientManager_SendProtoBufMsg;

	DetourHook<CSteamMatchmakingServers_GetServerDetails_t> CSteamMatchmakingServers_GetServerDetails;
	DetourHook<CSteamMatchmakingServers_RequestInternetServerList_t> CSteamMatchmakingServers_RequestInternetServerList;

	DetourHook<CSteamEngine_ProcessIPCFrame_t> CSteamEngine_ProcessIPCFrame;
	DetourHook<CSteamEngine_SetAppIdForCurrentPipe_t> CSteamEngine_SetAppIdForCurrentPipe;

	DetourHook<CUser_CheckAppOwnership_t> CUser_CheckAppOwnership;
	DetourHook<CUser_GetSubscribedApps_t> CUser_GetSubscribedApps;
	DetourHook<CUser_PostCallbackToAppId_t> CUser_PostCallbackToAppId;

	DetourHook<CUserAppManager_BuildDepotDependency_t> CUserAppManager_BuildDepotDependency;

	DetourHook<CWebSocketConnection_BBuildAndAsyncSendFrame_t> CWebSocketConnection_BBuildAndAsyncSendFrame;

	DetourHook<IClientCompat_BIsCompatLayerEnabled_t> IClientCompat_BIsCompatLayerEnabled;
	DetourHook<IClientConfigStore_SetString_t> IClientConfigStore_SetString;

	DetourHook<IClientFriends_GetFriendGamePlayed_t> IClientFriends_GetFriendGamePlayed;

	DetourHook<IClientRemoteStorage_IsCloudEnabledForApp_t> IClientRemoteStorage_IsCloudEnabledForApp;

	VFTHook<IClientAppManager_BCanRemotePlayTogether_t> IClientAppManager_BCanRemotePlayTogether;
	VFTHook<IClientAppManager_BIsDlcEnabled_t> IClientAppManager_BIsDlcEnabled;
	VFTHook<IClientAppManager_GetAppUpdateInfo_t> IClientAppManager_GetAppUpdateInfo;
	VFTHook<IClientAppManager_LaunchApp_t> IClientAppManager_LaunchApp;
	VFTHook<IClientAppManager_IsAppDlcInstalled_t> IClientAppManager_IsAppDlcInstalled;

	VFTHook<IClientApps_GetDLCDataByIndex_t> IClientApps_GetDLCDataByIndex;
	VFTHook<IClientApps_GetDLCCount_t> IClientApps_GetDLCCount;

	VFTHook<IClientUser_BLoggedOn_t> IClientUser_BLoggedOn;
	VFTHook<IClientUser_BUpdateAppOwnershipTicket_t> IClientUser_BUpdateAppOwnershipTicket;
	VFTHook<IClientUser_GetAppOwnershipTicketExtendedData_t> IClientUser_GetAppOwnershipTicketExtendedData;
	VFTHook<IClientUser_GetEncryptedAppTicket_t> IClientUser_GetEncryptedAppTicket;
	VFTHook<IClientUser_GetLegacyCDKey_t> IClientUser_GetLegacyCDKey;
	VFTHook<IClientUser_IsUserSubscribedAppInTicket_t> IClientUser_IsUserSubscribedAppInTicket;

	VFTHook<IClientUtils_GetAppId_t> IClientUtils_GetAppId;
	VFTHook<IClientUtils_GetOfflineMode_t> IClientUtils_GetOfflineMode;


	//steamui.so
	DetourHook<CGameInfoDialog_ServerResponded_t> CGameInfoDialog_ServerResponded;
}

bool Hooks::setup()
{
	LOG_DEBUG("Hooks::setup()\n");

	{
		const auto name = std::string("14CCompatManager");
		if (!Decompiler::vftables.contains(name))
		{
			LOG_ERROR("Failed to get %s VFTable!\n", name.c_str());
			return false;
		}

		auto& compatMan = Decompiler::vftables.at(name);

		IClientCompat_BIsCompatLayerEnabled.setup
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

		IClientConfigStore_SetString.setup
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

		IClientFriends_GetFriendGamePlayed.setup
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
		IClientRemoteStorage_IsCloudEnabledForApp.setup
		(
			VFTIndexes::IClientRemoteStorage::IsCloudEnabledForApp.getPrintName().c_str(),
			storage.functions[VFTIndexes::IClientRemoteStorage::IsCloudEnabledForApp.index],
			hkClientRemoteStorage_IsCloudEnabledForApp
		);
	}

	bool succeeded =
		TraceIPC.setup(Patterns::TraceIPC, &hkTraceIPC)

		&& CAPIJob_SendAndRecv.setup(Patterns::CAPIJob::SendAndRecv, hkAPIJob_SendAndRecv)

		&& CAppDataCache_BParseResponseFromMessage.setup(Patterns::CAppDataCache::BParseResponseMessage, hkAppDataCache_BParseResponseFromMessage)

		//We detour hook this virtual function out of respect for my friend Selectively11. His amazing project
		//CloudRedirect hooks the same function using a VFT hook already
		&& CClientUnifiedServiceMethod_SendAndRecvMsg.setup(VFTIndexes::CClientUnifiedServiceTransport::SendAndRecvMsg, hkClientUnifiedServiceTransport_SendAndRecvMsg)

		//To lazy to move this for now. Doesn't really matter wheter we detour or vft hook
		&& CCMInterface_RecvPkt.setup(VFTIndexes::CCMInterface::RecvPkt, hkCMInterface_RecvPkt)

		&& CRemoteClientManager_RecvPkt.setup
		(
			VFTIndexes::CRemoteClientManager::RecvPkt,
			hkRemoteClientManager_RecvPkt
		)

		&& CRemoteClientManager_SendProtoBufMsg.setup
		(
			Patterns::CRemoteClientManager::SendProtoBufMsg,
			hkRemoteClientManager_SendProtoBufMsg
		)

		&& CSteamMatchmakingServers_GetServerDetails.setup(VFTIndexes::CSteamMatchmakingServers::GetServerDetails, hkSteamMatchmakingServers_GetServerDetails)
		&& CSteamMatchmakingServers_RequestInternetServerList.setup(VFTIndexes::CSteamMatchmakingServers::RequestInternetServerList, hkSteamMatchmakingServers_RequestInternetServerList)

		&& CUser_CheckAppOwnership.setup(Patterns::CUser::CheckAppOwnership, hkUser_CheckAppOwnership)
		&& CUser_GetSubscribedApps.setup(Patterns::CUser::GetSubscribedApps, hkUser_GetSubscribedApps)
		&& CUser_PostCallbackToAppId.setup(Patterns::CUser::PostCallbackToAppId, hkUser_PostCallbackToAppId)

		&& CUserAppManager_BuildDepotDependency.setup(Patterns::CUserAppManager::BuildDepotDependency, hkUserAppManager_BuildDepotDependency)

		&& CSteamEngine_ProcessIPCFrame.setup(Patterns::CSteamEngine::ProcessIPCFrame, hkSteamEngine_ProcessIPCFrame)
		&& CSteamEngine_SetAppIdForCurrentPipe.setup(Patterns::CSteamEngine::SetAppIdForCurrentPipe, hkSteamEngine_SetAppIdForCurrentPipe)

		&& CWebSocketConnection_BBuildAndAsyncSendFrame.setup(Patterns::CWebSocketConnection::BBuildAndAsyncSendFrame, hkWebSocketConnection_BBuildAndAsyncSendFrame)

		&& CGameInfoDialog_ServerResponded.setup(VFTIndexes::CGameInfoDialog::ServerResponded, hkCGameInfoDialog_ServerResponded);


	Hooks::place();
	//This is unnecessary but I'll keep this for now in case I wanna improve error checks
	return succeeded;
}

void Hooks::place()
{
	//Detours
	TraceIPC.place();

	CAPIJob_SendAndRecv.place();

	CAppDataCache_BParseResponseFromMessage.place();

	CClientUnifiedServiceMethod_SendAndRecvMsg.place();

	CCMInterface_RecvPkt.place();

	CRemoteClientManager_RecvPkt.place();
	CRemoteClientManager_SendProtoBufMsg.place();

	CSteamEngine_ProcessIPCFrame.place();
	CSteamEngine_SetAppIdForCurrentPipe.place();

	CSteamMatchmakingServers_GetServerDetails.place();
	CSteamMatchmakingServers_RequestInternetServerList.place();

	CUser_CheckAppOwnership.place();
	CUser_GetSubscribedApps.place();
	CUser_PostCallbackToAppId.place();

	CUserAppManager_BuildDepotDependency.place();

	CWebSocketConnection_BBuildAndAsyncSendFrame.place();

	CGameInfoDialog_ServerResponded.place();

	IClientCompat_BIsCompatLayerEnabled.place();

	IClientConfigStore_SetString.place();

	IClientFriends_GetFriendGamePlayed.place();

	IClientRemoteStorage_IsCloudEnabledForApp.place();
}

void Hooks::placeVFTHooks()
{
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

	//I don't think the IPC layer is multithreaded but better safe than sorry
	static std::mutex mutex;
	std::lock_guard guard(mutex);

	LOG_DEBUG("CUser at %p\n", reinterpret_cast<void*>(usr));

	{
		const auto appManager = usr->getAppManager();
		LOG_DEBUG("CUserAppManager at %p\n", reinterpret_cast<void*>(appManager));

		std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
		LM_VmtNew(*reinterpret_cast<lm_address_t**>(appManager), vft.get());

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

		LOG_DEBUG("IClientAppManager->vft at %p\n", reinterpret_cast<void*>(vft->vtable));
	}

	{
		const auto clientApps = usr->getClientApps();
		LOG_DEBUG("CUserAppInfo at %p\n", reinterpret_cast<void*>(clientApps));

		std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
		LM_VmtNew(*reinterpret_cast<lm_address_t**>(clientApps), vft.get());

		Hooks::IClientApps_GetDLCDataByIndex.setup(vft, VFTIndexes::IClientApps::GetDLCDataByIndex, hkClientApps_GetDLCDataByIndex);
		Hooks::IClientApps_GetDLCCount.setup(vft, VFTIndexes::IClientApps::GetDLCCount, hkClientApps_GetDLCCount);

		Hooks::IClientApps_GetDLCDataByIndex.place();
		Hooks::IClientApps_GetDLCCount.place();

		LOG_DEBUG("IClientApps->vft at %p\n", reinterpret_cast<void*>(vft->vtable));
	}

	{
		const auto clientUser = usr->getClientUser();
		LOG_DEBUG("IClientUser at %p\n", reinterpret_cast<void*>(clientUser));

		std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
		LM_VmtNew(*reinterpret_cast<lm_address_t**>(clientUser), vft.get());

		Hooks::IClientUser_BLoggedOn.setup(vft, VFTIndexes::IClientUser::BLoggedOn, &hkClientUser_BLoggedOn);
		Hooks::IClientUser_BUpdateAppOwnershipTicket.setup(vft, VFTIndexes::IClientUser::BUpdateAppOwnershipTicket, hkClientUser_BUpdateAppOwnershipTicket);
		Hooks::IClientUser_GetAppOwnershipTicketExtendedData.setup(vft, VFTIndexes::IClientUser::GetAppOwnershipTicketExtendedData, hkClientUser_GetAppOwnershipTicketExtendedData);
		//GetEncryptedAppTicket is just a wrapper for CUser::GetEncryptedAppTicket. But there is no need to go deeper
		//since we load the encrypted ticket in the Networking layer. We just need this function to spoof our steamId once
		Hooks::IClientUser_GetEncryptedAppTicket.setup(vft, VFTIndexes::IClientUser::GetEncryptedAppTicket, hkClientUser_GetEncryptedAppTicket);
		Hooks::IClientUser_GetLegacyCDKey.setup(vft, VFTIndexes::IClientUser::GetLegacyCDKey, hkClientUser_GetLegacyCDKey);
		Hooks::IClientUser_IsUserSubscribedAppInTicket.setup(vft, VFTIndexes::IClientUser::IsUserSubscribedAppInTicket, hkClientUser_IsUserSubscribedAppInTicket);

		Hooks::IClientUser_BLoggedOn.place();
		Hooks::IClientUser_BUpdateAppOwnershipTicket.place();
		Hooks::IClientUser_GetAppOwnershipTicketExtendedData.place();
		//Hooks::IClientUser_GetEncryptedAppTicket.place();
		Hooks::IClientUser_GetLegacyCDKey.place();
		Hooks::IClientUser_IsUserSubscribedAppInTicket.place();

		LOG_DEBUG("IClientUser->vft at %p\n", reinterpret_cast<void*>(vft->vtable));
	}

	{
		const auto utils = g_pSteamEngine->getUtils();
		LOG_DEBUG("IClientUtils at %p\n", reinterpret_cast<void*>(utils));

		std::shared_ptr<lm_vmt_t> vft = std::make_shared<lm_vmt_t>();
		LM_VmtNew(*reinterpret_cast<lm_address_t**>(utils), vft.get());

		Hooks::IClientUtils_GetAppId.setup(vft, VFTIndexes::IClientUtils::GetAppId, hkClientUtils_GetAppId);
		Hooks::IClientUtils_GetOfflineMode.setup(vft, VFTIndexes::IClientUtils::GetOfflineMode, hkClientUtils_GetOfflineMode);

		Hooks::IClientUtils_GetAppId.place();
		Hooks::IClientUtils_GetOfflineMode.place();

		LOG_DEBUG("IClientUtils->vft at %p\n", reinterpret_cast<void*>(vft->vtable));
	}

	hooked = true;
}

void Hooks::remove()
{
	//Detours
	TraceIPC.remove();

	CAPIJob_SendAndRecv.remove();

	CAppDataCache_BParseResponseFromMessage.remove();

	CClientUnifiedServiceMethod_SendAndRecvMsg.remove();

	CCMInterface_RecvPkt.remove();

	CRemoteClientManager_RecvPkt.remove();
	CRemoteClientManager_SendProtoBufMsg.remove();

	CSteamEngine_ProcessIPCFrame.remove();
	CSteamEngine_SetAppIdForCurrentPipe.remove();

	CSteamMatchmakingServers_GetServerDetails.remove();
	CSteamMatchmakingServers_RequestInternetServerList.remove();

	CUser_CheckAppOwnership.remove();
	CUser_GetSubscribedApps.remove();
	CUser_PostCallbackToAppId.remove();

	CUserAppManager_BuildDepotDependency.remove();

	CWebSocketConnection_BBuildAndAsyncSendFrame.remove();

	CGameInfoDialog_ServerResponded.remove();

	IClientCompat_BIsCompatLayerEnabled.remove();

	IClientConfigStore_SetString.remove();

	IClientFriends_GetFriendGamePlayed.remove();

	IClientRemoteStorage_IsCloudEnabledForApp.remove();

	//VFT Hooks
	IClientAppManager_BCanRemotePlayTogether.remove();
	IClientAppManager_BIsDlcEnabled.remove();
	IClientAppManager_GetAppUpdateInfo.remove();
	IClientAppManager_LaunchApp.remove();
	IClientAppManager_IsAppDlcInstalled.remove();

	IClientApps_GetDLCDataByIndex.remove();
	IClientApps_GetDLCCount.remove();

	IClientUser_BLoggedOn.remove();
	IClientUser_BUpdateAppOwnershipTicket.remove();
	IClientUser_GetAppOwnershipTicketExtendedData.remove();
	//IClientUser_GetEncryptedAppTicket.remove();
	IClientUser_GetLegacyCDKey.remove();
	IClientUser_IsUserSubscribedAppInTicket.remove();

	IClientUtils_GetAppId.remove();
	IClientUtils_GetOfflineMode.remove();

}
