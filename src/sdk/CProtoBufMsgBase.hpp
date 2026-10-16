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
	uint8_t __pad0x8[0x28];			//0x0
	uint32_t type;					//0x28 - It's actually EMsg, but those get genned as int which is unreliable at best
	uint8_t __pad0x2C[0x4];			//0x2C
	void* hdr;						//0x30
	void* __pBody;					//0x38
	uint8_t __pad0x40[0x10];		//0x40
	
	constexpr EMsg getProtoBufType() const
	{
		return static_cast<EMsg>(type & ~PROTOBUF_TYPE_MASK);
	}
	
	template<typename T> constexpr T* getBody() const
	{
		return reinterpret_cast<T*>(__pBody);
	}
}; //0x50
