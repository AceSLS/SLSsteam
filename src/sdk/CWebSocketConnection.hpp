#pragma once

#include "types.hpp"

#include <cstdint>


enum EWebSocketConnectionSendType : uint32_t
{
	k_EWebSocketConnectionSendRaw = 2
};


SDK_Class CWebSocketConnection
{
public:
	bool buildAndAsyncSendFrame(const EWebSocketConnectionSendType type, void* data, const steamsize_t dataSize);
	bool buildAndAsyncSendFrameHk(const EWebSocketConnectionSendType type, void* data, const steamsize_t dataSize);
};
