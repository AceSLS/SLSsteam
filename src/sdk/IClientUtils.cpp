#include "IClientUtils.hpp"

#include "types.hpp"

#include "../decompiler.hpp"
#include "../hooks.hpp"
#include "../patterns.hpp"
#include "../utils.hpp"

#include "libmem/libmem.h"


HSteamPipe IClientUtils::getCurrentSteamPipe()
{
	//Offset found in IClientUtils::GetAppId

#ifdef ARM64
	static lm_address_t offset = LM_ADDRESS_BAD;

	lm_address_t curAddr = Patterns::IClientUtils::Offset_GetPipeIndex.address;

	int stage = 0;
	lm_inst_t inst { };
	std::string offsetReg;

	for (;;)
	{
		LM_Disassemble(curAddr, &inst);
		curAddr += inst.size;
		LOG_DEBUG("Disassembled %s %s\n", inst.mnemonic, inst.op_str)

		switch (stage)
		{
			case 0:
				offsetReg = Utils::strsplit(inst.op_str, ", ")[0];
				offset = Decompiler::extractHexNum(inst.op_str);
				LOG_DEBUG("Stage 0 offset %lu\n", offset);
				stage++;
				break;

			case 1:
				if (strcmp(inst.mnemonic, "add") != 0)
				{
					break;
				}

				offsetReg = Utils::strsplit(inst.op_str, ", ")[0];
				stage++;
				break;

			case 2:
				if (strcmp(inst.mnemonic, "ldr") != 0)
				{
					break;
				}

				if (!strstr(inst.op_str, offsetReg.c_str()))
				{
					break;
				}

				offset += Decompiler::extractHexNum(inst.op_str);
				LOG_DEBUG("Stage 2 offset %lu\n", offset);
				stage++;
				break;
		}

		if (stage > 2)
		{
			break;
		}
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
