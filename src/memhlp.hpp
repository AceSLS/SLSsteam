#pragma once

#include "globals.hpp"
#include "log.hpp"

#include "libmem/libmem.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <vector>


namespace MemHlp
{
	extern std::unordered_map<std::string, std::unique_ptr<lm_module_t>> moduleMap;

	enum class SigFollowMode
	{
		None,
		Relative,
		PrologueUpwards
	};

	///Summary:
	///Write assembly code to address and increase address by bytes written
	template<typename ...Args>
	bool assembleCodeAt(lm_address_t& address, const char* fmt, Args... args)
	{
		if (address == LM_ADDRESS_BAD)
		{
			LOG_DEBUG("Can't write to LM_ADDRESS_BAD!\n");
			return false;
		}

		const size_t size = snprintf(nullptr, 0, fmt, args...) + 1;
		std::string code;
		code.resize(size);
		snprintf(code.data(), size, fmt, args...);

		static lm_inst_t inst;
		//TODO: Potentially replace with LM_AssembleEx and only allocate memory as needed
		bool success = false;

		if (!LM_Assemble(code.c_str(), &inst))
		{
			LOG_DEBUG("Failed to assemble %s!\n", code.c_str());
		}
		else if (!LM_WriteMemory(address, inst.bytes, inst.size))
		{
			LOG_DEBUG("Failed to write %s to 0x%x!\n", code.c_str(), address);
		}
		else
		{
			LOG_DEBUG("Wrote %s to 0x%x with %i bytes\n", code.c_str(), address, inst.size);
			address += inst.size;
			success = true;
		}

		return success;
	}

	lm_module_t* getModule(const std::string& name);
	lm_module_t* getModuleByAddress(const lm_address_t address, const bool useCached = true);
	lm_segment_t getSegmentByAddress(const lm_address_t address, const bool useCached = true);

	std::vector<int16_t> patternToBytes(const char* pattern);
	lm_address_t patternScan(const char* pattern, const lm_module_t& module);

	lm_address_t searchSignature
	(
		const char* name,
		const char* signature,
		const lm_module_t& module = *g_modSteamClient,
		const SigFollowMode mode = SigFollowMode::None,
		const void* extraData = nullptr,
		const size_t extraDataSize = 0
	);

	lm_address_t getJmpTarget(const lm_address_t address);
	lm_address_t findPrologue(const lm_address_t address, const std::vector<int16_t>& prologueBytes);

	//TODO: Create hooking wrapper that calls this automatically
	bool fixPICThunkCall(const char* name, const lm_address_t fn, const lm_address_t tramp);

	std::string hexdump(const void* address, const size_t size);

	const char* getTypeName(const void* pClass);
	lm_address_t searchOffsetByTypeName(const void* pClass, const std::string& name);
	
	template<typename tFN, typename ...Args>
	constexpr auto callVFunc(const unsigned int index, void* thisPtr, Args... args)
	{
		const auto fn = reinterpret_cast<tFN>(*(*reinterpret_cast<lm_address_t***>(thisPtr) + index));
		LOG_TRACE("Calling vfunc %u from %s at %p\n", index, getTypeName(thisPtr), reinterpret_cast<void*>(fn));
		return fn(thisPtr, args...);
	}
}
