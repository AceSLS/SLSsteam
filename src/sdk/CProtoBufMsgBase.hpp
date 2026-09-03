#pragma once

#include "types.hpp"

#include "protobufs/enums_clientserver.pb.h"
#include "protobufs/steammessages_base.pb.h"
#include "protobufs/encrypted_app_ticket.pb.h"
#include "protobufs/steammessages_clientserver.pb.h"
#include "protobufs/steammessages_clientserver_2.pb.h"
#include "protobufs/steammessages_clientserver_appinfo.pb.h"
#include "protobufs/steammessages_clientserver_friends.pb.h"
#include "protobufs/steammessages_clientserver_login.pb.h"
#include "protobufs/steammessages_clientserver_userstats.pb.h"
#include "protobufs/steammessages_player.steamclient.pb.h"

#include <cstdint>


enum EGameFlags
{
	//1 << 0 is set for spacewar, not other mp games. idk
	k_EGameFlagJoinable = 1 << 1,
	k_EGameFlagMultiplayer = 1 << 13,
};

SDK_Class CProtoBufMsgBase
{
public:
	uint8_t __pad_0x0[0x14];	//0x0
	ENetPacket type;			//0x14
	uint8_t __pad_0x16[0x4];	//0x18
	CMsgProtoBufHeader* header; //0x1C
	void* __pBody;				//0x20
	uint8_t __pad_0x24[0x8];	//0x24
	
	constexpr EMsg getProtoBufType() const
	{
		return static_cast<EMsg>(type & ~PROTOBUF_TYPE_MASK);
	}
	
	template<typename T> constexpr T* getBody() const
	{
		return reinterpret_cast<T*>(__pBody);
	}
}; //0x2C
