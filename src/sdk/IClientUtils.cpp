#include "IClientUtils.hpp"

#include "types.hpp"

#include "../decompiler.hpp"
#include "../hooks.hpp"
#include "../patterns.hpp"

#include "libmem/libmem.h"


HSteamPipe IClientUtils::getCurrentSteamPipe()
{
	//Offset found in IClientUtils::GetAppId

#ifdef AARCH64
	static int32_t offset = 0;
	if (!offset)
	{
		auto emu = CARMEmu(Patterns::IClientUtils::Offset_GetPipeIndex.address);
		for (unsigned int i = 0; i < 4; i++)
		{
			auto regs = emu.run();
			if (i == 3)
			{
				offset = (*regs.begin())->wi;
			}
		}

		LOG_DEBUG("IClientUtils::getCurrentSteamPipe offset %i\n", offset);
	}
#else
	const static auto offset = *reinterpret_cast<int32_t*>(Patterns::IClientUtils::Offset_GetPipeIndex.address + 0x2);
#endif

	return *reinterpret_cast<HSteamPipe*>(this + offset);
}

AppId_t IClientUtils::getAppId()
{
	return Hooks::IClientUtils_GetAppId->originalFn.fn(this);
}
