#include "CClientUnifiedServiceTransport.hpp"

#include "../hooks.hpp"



bool CClientUnifiedServiceTransport::wasResultSuccess(const uint32_t res)
{
	return reinterpret_cast<const uint8_t*>(&res)[0]; //res[0] = eresult == 1;
}

uint32_t CClientUnifiedServiceTransport::sendAndRecvMsg(const char* name, void* send, void* recv)
{
	return Hooks::CClientUnifiedServiceMethod_SendAndRecvMsg->tramp.fn(this, name, send, recv, nullptr);
}
