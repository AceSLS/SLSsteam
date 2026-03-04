#pragma once

#include <cstdint>

class CProtoBufMsgBase;

namespace License
{
	void getManifest(uint32_t depotId);
	void recvMsg(CProtoBufMsgBase* msg);
}
