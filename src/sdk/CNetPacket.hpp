#pragma once

#include "protobufs/enums_clientserver.pb.h"
#include "protobufs/steammessages_base.pb.h"

#include "steam.hpp"
#include "types.hpp"

#include "../log.hpp"

#include <cstdint>
#include <string>

//Biggest message I have observed was around 600kb. We just set
//a limit so we don't accidentally fill the whole buffer with 1 message
constexpr static unsigned int MAX_PACKET_SIZE = 1024 * 1024 * 1; //1MB
//Theoretical max
constexpr static unsigned int MAX_PACKETS = 8;

//TODO: Move into anonymous namespace or something, so these don't clutter the global namespace
extern uint8_t g_packetsArray[MAX_PACKET_SIZE * MAX_PACKETS];
extern uintptr_t g_packetsArrayOffset;

extern std::mutex g_packetSerializeMutex;


//Helper class to make calculations more legible
SDK_Class CNetPacketBody
{
public:

	ENetPacket type;
	uint32_t headerSize;
	//Header[headerSize]
	//Body[CNetPacket->size - headerSize - sizeof(CNetPacketBody)]
};

SDK_Class CNetPacket
{
public:
	uint8_t __pad0x0[0x4];			//0x0
	CNetPacketBody* body;			//0x4
	uint32_t size;					//0x8
	int32_t refs;					//0xC
	CNetPacketBody* originalBody;	//0x10
	uint8_t __pad0x10[0xC];			//0x14
	
	constexpr bool isValid() const
	{
		return getType() != INVALID_NETPACKET_TYPE && size > sizeof(CNetPacketBody);
	}
	
	constexpr ENetPacket getType() const
	{
		if (!body)
		{
			return INVALID_NETPACKET_TYPE;
		}

		return body->type;
	}

	std::string getProtoBufTypeName() const;

	constexpr bool isProtoBuf() const
	{
		if (getType() == INVALID_NETPACKET_TYPE)
		{
			return INVALID_NETPACKET_TYPE;
		}

		return getType() & PROTOBUF_TYPE_MASK;
	}

	constexpr EMsg getProtoBufType() const
	{
		return static_cast<EMsg>(getType() & ~PROTOBUF_TYPE_MASK);
	}

	CMsgProtoBufHeader deserializeHeader() const;

	template<typename T>
	void serialize(const T& msg, const CMsgProtoBufHeader* header)
	{
		constexpr uintptr_t headerOffset = sizeof(CNetPacketBody);
		const uintptr_t headerSize = header ? header->ByteSizeLong() : body->headerSize;

		const uintptr_t msgOffset = headerSize + headerOffset;
		const uintptr_t newSize = msg.ByteSizeLong() + msgOffset;

		if (newSize >= MAX_PACKET_SIZE)
		{
			LOG_ERROR("Failed to serialize 0x%x! Buffer to small (needed %u, has %u)\n", getType(), newSize, MAX_PACKET_SIZE);
			return;
		}

		const uintptr_t remainingSize = sizeof(g_packetsArray) - g_packetsArrayOffset;
		if (newSize >= remainingSize)
		{
			LOG_DEBUG("New packet size doesn't fit in end of buffer, (needed %u, has %u). Starting anew\n", newSize, remainingSize);
			g_packetsArrayOffset = 0;
		}

		const std::lock_guard lock(g_packetSerializeMutex);
		uint8_t* mem = &g_packetsArray[g_packetsArrayOffset];

		if (header)
		{
			if (!header->SerializeToArray(mem + headerOffset, headerSize))
			{
				LOG_ERROR("Failed to serialize header!\n");
				return;
			}

			CNetPacketBody* newBdy = reinterpret_cast<CNetPacketBody*>(mem);
			newBdy->type = body->type;
			newBdy->headerSize = headerSize;
		}
		else
		{
			memcpy(mem, body, msgOffset);
		}

		if (!msg.SerializeToArray(mem + msgOffset, msg.ByteSizeLong()))
		{
			LOG_ERROR("Failed to serialize 0x%x!\n", getType());
			return;
		}

		body = reinterpret_cast<CNetPacketBody*>(mem);
		size = newSize;
		//If I understand correctly Steam cleans up for us, that's why we crash when we free the oldBody ourself
		//However the body we allocate doesn't get freed, so we just reuse a buffer for it

		LOG_DEBUG("Serialized 0x%x into PACKETS_ARRAY at %u with size %u\n", getType(), g_packetsArrayOffset, newSize);

		g_packetsArrayOffset += size;
	}

	template<typename T>
	constexpr void serialize(const T& msg)
	{
		serialize(msg, nullptr);
	}
	
	template<typename T>
	constexpr T deserializeBody() const
	{
		const uintptr_t msgOffset = body->headerSize + sizeof(CNetPacketBody);
		auto msg = T();

		msg.ParseFromArray(reinterpret_cast<uint8_t*>(body) + msgOffset, size - msgOffset);

		return msg;
	}

	void free();
}; //0x20
