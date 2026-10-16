#pragma once

#include "protobufs/enums_clientserver.pb.h"
#include "protobufs/steammessages_base.pb.h"

#include "steam.hpp"
#include "types.hpp"

#include "../log.hpp"

#include <cstdint>
#include <string>


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
	uint8_t __pad0x0[0x10];			//0x0
	CNetPacketBody* body;			//0x10
	uint32_t size;					//0x18
	int32_t refs;					//0x1C
	CNetPacketBody* originalBody;	//0x20 - Ref counted pointer
	uint8_t __pad0x10[0x30];		//0x28
	
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

		uint8_t* mem = reinterpret_cast<uint8_t*>(Steam::Plat_Alloc(newSize));

		if (!mem)
		{
			LOG_ERROR("Failed to allocate new packet body with size %u!\n", newSize);
			return;
		}

		auto newBdy = reinterpret_cast<CNetPacketBody*>(mem);

		if (header)
		{
			if (!header->SerializeToArray(mem + headerOffset, headerSize))
			{
				LOG_ERROR("Failed to serialize header!\n");
				goto failed;
			}

			newBdy->headerSize = headerSize;
		}
		else
		{
			memcpy(mem, body, msgOffset);
		}

		if (!msg.SerializeToArray(mem + msgOffset, msg.ByteSizeLong()))
		{
			LOG_ERROR("Failed to serialize 0x%x!\n", getType());
			goto failed;
		}

		if (body)
		{
			newBdy->type = body->type;
			Steam::Plat_Free(body);
		}

		body = reinterpret_cast<CNetPacketBody*>(mem);
		size = newSize;
		originalBody = body;

		return;

	failed:
		Steam::Plat_Free(mem);
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
}; //0x58
