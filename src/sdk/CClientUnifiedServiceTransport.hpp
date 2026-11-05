#pragma once

#include "types.hpp"

#include <cstdint>


SDK_Class CClientUnifiedServiceTransport
{
public:
	static bool wasResultSuccess(const uint32_t res);

	uint32_t sendAndRecvMsg(const char* name, void* send, void* recv);
};
