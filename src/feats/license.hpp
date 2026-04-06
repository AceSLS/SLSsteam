#pragma once

#include <cstdint>

class CPackageInfo;
class CProtoBufMsgBase;

namespace License
{
	void injectDepots(CPackageInfo* pkg);
	void getManifest(uint32_t depotId);
	void recvMsg(CProtoBufMsgBase* msg);
}
