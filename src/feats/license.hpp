#pragma once

class CNetPacket;
class CPackageInfo;

namespace License
{
	void getPackageInfo(CPackageInfo* pkg);
	void recvMsg(CNetPacket* pkt);
	void runIPCFrame();
}
