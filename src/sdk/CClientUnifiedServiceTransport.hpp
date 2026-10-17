#pragma once

#include "types.hpp"

#include <cstdint>


SDK_Class CClientUnifiedServiceTransport
{
public:
	bool sendAndRecvMsg(const char* name, void* send, void* recv);
};
