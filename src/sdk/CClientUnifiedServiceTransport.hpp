#pragma once

#include "types.hpp"

#include <cstdint>


SDK_Class CClientUnifiedServiceTransport
{
public:
	uint32_t sendAndRecvMsg(const char* name, void* send, void* recv);
};
