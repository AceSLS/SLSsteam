#pragma once

class CPackageInfo;
class CProtoBufMsgBase;

namespace License
{
	void getPackageInfo(CPackageInfo* pkg);
	void recvMsg(CProtoBufMsgBase* msg);
	void runIPCFrame();
}
