#include "CClientUnifiedServiceTransport.hpp"

#include "../hooks.hpp"


bool CClientUnifiedServiceTransport::sendAndRecvMsg(const char* name, void* send, void* recv)
{
	return Hooks::CClientUnifiedServiceMethod_SendAndRecvMsg->tramp.fn(this, name, send, recv, nullptr);
}
