#include "IClientUtils.hpp"

#include "types.hpp"

#include "../hooks.hpp"
#include "../patterns.hpp"


HSteamPipe IClientUtils::getCurrentSteamPipe()
{
	//Offset found in IClientUtils::GetAppId
	const static auto offset = *reinterpret_cast<int32_t*>(Patterns::IClientUtils::Offset_GetPipeIndex.address + 0x2);
	return *reinterpret_cast<HSteamPipe*>(this + offset);
}

AppId_t IClientUtils::getAppId()
{
	return Hooks::IClientUtils_GetAppId->originalFn.fn(this);
}
