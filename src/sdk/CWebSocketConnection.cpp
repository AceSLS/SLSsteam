#include "CWebSocketConnection.hpp"

#include "../hooks.hpp"


bool CWebSocketConnection::buildAndAsyncSendFrame(const EWebSocketConnectionSendType type, void* data, const steamsize_t dataSize)
{
	return Hooks::CWebSocketConnection_BBuildAndAsyncSendFrame->tramp.fn(this, type, data, dataSize);
}

bool CWebSocketConnection::buildAndAsyncSendFrameHk(const EWebSocketConnectionSendType type, void* data, const steamsize_t dataSize)
{
	return Hooks::CWebSocketConnection_BBuildAndAsyncSendFrame->hookFn.fn(this, type, data, dataSize);
}

CWebSocketConnection* g_pWebSocketConnection = nullptr;
