#pragma once

#include "types.hpp"

#include <cstdint>


SDK_Class CClientUnifiedServiceTransport
{
public:
	uint32_t sendAndRecvMsg(const char* name, void* send, void* recv);

	constexpr static bool wasResultSuccess(const uint32_t res)
	{
		return reinterpret_cast<const uint8_t*>(&res)[0]; //res[0] = eresult == 1;
	}
};
