#include "memhlp.hpp"

#include "decompiler.hpp"
#include "log.hpp"
#include "utils.hpp"

#include "libmem/libmem.h"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>


std::unordered_map<std::string, std::unique_ptr<lm_module_t>> MemHlp::moduleMap = std::unordered_map<std::string, std::unique_ptr<lm_module_t>>();

lm_module_t* MemHlp::getModule(const std::string& name)
{
	if (!moduleMap.contains(name.c_str()))
	{
		LOG_DEBUG("Searching module %s\n", name.c_str());

		lm_module_t mod;
		if (!LM_FindModule(name.c_str(), &mod))
		{
			LOG_ERROR("Module %s not found!\n", name.c_str());
			moduleMap[name] = nullptr;
		}
		else
		{
			LOG_DEBUG("Module %s loaded at 0x%x with size 0x%x\n", mod.name, mod.base, mod.size);
			moduleMap[name] = std::make_unique<lm_module_t>(mod);
		}
	}

	return moduleMap.at(name).get();
}

lm_module_t* MemHlp::getModuleByAddress(const lm_address_t address, const bool useCached)
{
	static auto mods = std::vector<lm_module_t>();

	if (!useCached)
	{
		mods.clear();
	}

	if (!mods.size())
	{
		LM_EnumModules([](lm_module_t* mod, lm_void_t* args)
		{
			mods.emplace_back(*mod);
			return LM_TRUE;
		}, nullptr);
	}

	for (const auto& mod : mods)
	{
		if (mod.base < address && mod.end > address)
		{
			return getModule(mod.name);
		}
	}

	return nullptr;
}

lm_segment_t MemHlp::getSegmentByAddress(const lm_address_t address, const bool useCached)
{
	static auto segs = std::vector<lm_segment_t>();

	if (!useCached)
	{
		segs.clear();
	}

	if (!segs.size())
	{
		segs.clear();

		LM_EnumSegments([](lm_segment_t* seg, lm_void_t* args)
		{
			segs.emplace_back(*seg);
			return LM_TRUE;
		}, nullptr);
	}

	for (const auto& seg : segs)
	{
		if (seg.base < address && seg.end > address)
		{
			return seg;
		}
	}

	return lm_segment_t();
}

std::vector<int16_t> MemHlp::patternToBytes(const char* pattern)
{
	auto bytes = std::vector<int16_t>();

	char* start = const_cast<char*>(pattern);
	const char* end = start + strlen(pattern);

	while (start < end)
	{
		if (*start == '?')
		{
			bytes.emplace_back(-1);
		}
		else if (*start != ' ')
		{
			bytes.emplace_back(std::strtoul(start, &start, 16));
		}

		start++;
	}

	return bytes;
}

lm_address_t MemHlp::patternScan(const char* pattern, const lm_module_t& targetModule)
{
	const auto bytes = patternToBytes(pattern);

	const Elf_Shdr* shText = Decompiler::getSection(targetModule, ".text");
	if (!shText)
	{
		LOG_DEBUG("Didn't find .text section for %s!\n", targetModule.name);
		return LM_ADDRESS_BAD;
	}

	const lm_address_t start = targetModule.base + shText->rva;
	const lm_address_t end = start + shText->size;

	lm_address_t address = LM_ADDRESS_BAD;
	unsigned int matches = 0;

	for (lm_address_t addr = start; addr < end ; addr++)
	{
		bool found = true;

		for (unsigned int i = 0; i < bytes.size(); i++)
		{
			if (bytes.at(i) == -1)
			{
				continue;
			}

			const lm_address_t byteAddr = addr + i;
			if (byteAddr > end)
			{
				found = false;
				break;
			}

			const lm_byte_t* pbyte = reinterpret_cast<lm_byte_t*>(byteAddr);
			if (*pbyte != bytes.at(i))
			{
				found = false;
				break;
			}
		}

		if (found)
		{
			address = addr;
			matches++;

			if (matches > 1)
			{
				LOG_DEBUG("Pattern %s found %i times at 0x%x!\n", pattern, matches, addr);
			}
		}
	}

	return address;
}

lm_address_t MemHlp::searchSignature(const char* name, const char* signature, const lm_module_t& module, const SigFollowMode mode, const void* extraData, const size_t extraDataSize)
{
	//lm_address_t address = LM_SigScan(signature, module.base, module.size);
	lm_address_t address = patternScan(signature, module);
	if (address == LM_ADDRESS_BAD)
	{
		LOG_DEBUG("Unable to find signature for %s!\n", name);
	}
	else
	{
		switch (mode)
		{
			case SigFollowMode::Relative:
				LOG_DEBUG("Resolving relative of %s at 0x%x\n", name, address);
				address = MemHlp::getJmpTarget(address);
				break;

			case SigFollowMode::PrologueUpwards:
			{
				LOG_DEBUG("Searching function prologue of %s from 0x%x\n", name, address);
				auto bytes = std::vector<int16_t>();
				for (unsigned int i = 0; i < extraDataSize; i++)
				{
					bytes.emplace_back(static_cast<const int16_t*>(extraData)[i]);
				}

				address = MemHlp::findPrologue(address, bytes);
				break;
			}

			default:
				break;
		}

		LOG_DEBUG("%s at 0x%x\n", name, address);
	}

	return address;
}

lm_address_t MemHlp::getJmpTarget(const lm_address_t address)
{
	lm_inst_t inst;
	if (!LM_Disassemble(address, &inst)) //Should not happen if we land in a code section
	{
		LOG_DEBUG("Failed to disassemble code at 0x%x!", address);
		return LM_ADDRESS_BAD;
	}

	LOG_DEBUG("Resolved to %s %s\n", inst.mnemonic, inst.op_str);

	if (strcmp(inst.mnemonic, "jmp") != 0 && strcmp(inst.mnemonic, "call") != 0)
	{
		return LM_ADDRESS_BAD;
	}

	return std::stoul(inst.op_str, nullptr, 16);
}

lm_address_t MemHlp::findPrologue(const lm_address_t address, const std::vector<int16_t>& prologueBytes)
{
	constexpr unsigned int scanSize = 0x10000;

	for (unsigned int i = 0u; i < scanSize; i++)
	{
		bool found = true;
		for (unsigned int j = 0u; j < prologueBytes.size(); j++)
		{
			if (prologueBytes[j] == -1)
			{
				continue;
			}

			if (*reinterpret_cast<lm_byte_t*>(address - i - j) != prologueBytes[j])
			{
				found = false;
				break;
			}
		}

		if (found)
		{
			lm_address_t prol = address - i - prologueBytes.size() + 1; //Add 1 byte back since bytesSize would be to big otherwise
			LOG_DEBUG("Prologue found at 0x%x\n", prol);
			return prol;
		}
	}

	LOG_DEBUG("Unable to find prologue after going up 0x%x bytes!\n", scanSize);
	return LM_ADDRESS_BAD;
}

bool MemHlp::fixPICThunkCall(const char* name, const lm_address_t fn, const lm_address_t tramp)
{
	LOG_DEBUG("Fixing PIC thunks for %s's trampoline\n", name);
	constexpr unsigned int maxBytes = 0x5; //Minimum bytes needed to detour a function, so our tramp will at least be of this size
	
	lm_inst_t inst;
	for (unsigned int curTrampOffset = 0; curTrampOffset <= maxBytes; )
	{
		const lm_address_t startAddress = tramp + curTrampOffset;

		if (!LM_Disassemble(startAddress, &inst))
		{
			LOG_DEBUG("Unable to dissassemble code at 0x%x\n", tramp + curTrampOffset);
			return false;
		}
		
		curTrampOffset += inst.size;
		LOG_DEBUG("0x%x: %s %s\n", inst.address, inst.mnemonic, inst.op_str);
		
		if (strcmp(inst.mnemonic, "call") != 0)
		{
			continue;
		}

		//Calculate the call address manually with it's original location
		lm_address_t followAddress = fn + curTrampOffset + *reinterpret_cast<lm_address_t*>(startAddress + 1);
		bool isIPCThunk = true;
		char newInstr[sizeof(inst.mnemonic) + sizeof(inst.op_str)];

		for (unsigned int i = 0; i < 2; i++) //Dissassemble next 2 instructions and check if they're an actual IPC thunk call
		{
			if (!LM_Disassemble(followAddress, &inst))
			{
				LOG_DEBUG("Unable to dissassemble code at 0x%x\n", followAddress);
				return false;
			}

			followAddress += inst.size;

			LOG_DEBUG("0x%x: %s %s\n", inst.address, inst.mnemonic, inst.op_str);

			//Can not declare in switch statement
			auto splits = std::vector<std::string>();
			lm_address_t retAddress = LM_ADDRESS_BAD;
			switch(i)
			{
				case 0:
					if (strcmp(inst.mnemonic, "mov") != 0)
					{
						isIPCThunk = false;
						break;
					}
					
					splits = Utils::strsplit(inst.op_str, ","); //Not checking for splits.size() since mov NEEDS a , somewhere
					retAddress = fn + curTrampOffset; //No need to add any bytes here, since i += inst.size in the outer loop takes care of that
					sprintf(newInstr, "%s %s, %p", inst.mnemonic, splits.at(0).c_str(), reinterpret_cast<void*>(retAddress));
					break;

				case 1:
					if (strcmp(inst.mnemonic, "ret") != 0)
					{
						isIPCThunk = false;
					}
					break;
			}

			if (!isIPCThunk)
			{
				break;
			}
		}

		if (!isIPCThunk)
		{
			continue;
		}

		if (!LM_Assemble(newInstr, &inst))
		{
			LOG_DEBUG("Unable to assemble instruction %s!\n", newInstr);
			return false;
		}

		lm_prot_t oldProt;
		LM_ProtMemory(startAddress, inst.size, LM_PROT_XRW, &oldProt);
		LM_WriteMemory(startAddress, inst.bytes, inst.size);
		LM_ProtMemory(startAddress, inst.size, oldProt, nullptr);
		LOG_DEBUG("Replaced PIC thunk call for %s at 0x%x with %s\n", name, followAddress, newInstr);
		return true;
	}

	return false;
}

std::string MemHlp::hexdump(const void* address, const size_t size)
{
	constexpr unsigned int ROWS = 0x10;
	std::ostringstream ss;

	ss << std::setfill(' ') << std::hex;

	for (unsigned int i = 0; i < ROWS; i++)
	{
		if (i == 0)
		{
			ss << std::setw(13) << i;
		}
		else
		{
			ss << std::setw(3) << i;
		}
	}

	for (uintptr_t i = 0; i < size; i += ROWS)
	{
		const unsigned int byteStart = reinterpret_cast<uintptr_t>(address) + i;
		const unsigned int num = ROWS > size - i ? size - i : ROWS;

		ss << "\n0x" << byteStart;

		for (uintptr_t j = 0; j < num; j++)
		{
			const unsigned int byte = *reinterpret_cast<uint8_t*>(byteStart + j);
			ss << " " << std::setfill('0') << std::setw(2) << byte;
		}

		ss << std::setfill(' ') << std::setw((ROWS - num) * 3 + 1) << " ";

		for (uintptr_t j = 0; j < num; j++)
		{
			const unsigned char byte = *reinterpret_cast<unsigned char*>(byteStart + j);
			if (std::isprint(byte))
			{
				ss << byte;
			}
			else
			{
				ss << ".";
			}
		}
	}

	return ss.str();
}

const char* MemHlp::getTypeName(const void* pClass)
{
	constexpr static auto isValid = [](const lm_address_t addr) -> bool
	{
		const auto mod = getModuleByAddress(addr);
		if (!mod)
		{
			return false;
		}

		const auto seg = getSegmentByAddress(addr);
		return seg.prot & LM_PROT_R;
	};

	const lm_address_t vft = *reinterpret_cast<const lm_address_t*>(pClass);
	if (!isValid(vft))
	{
		return nullptr;
	}

	const lm_address_t typeInfo = *reinterpret_cast<const lm_address_t*>(vft - sizeof(lm_address_t));
	if (!isValid(typeInfo))
	{
		return nullptr;
	}

	const char* name = *reinterpret_cast<const char**>(typeInfo + sizeof(lm_address_t));
	const lm_segment_t seg = getSegmentByAddress(reinterpret_cast<lm_address_t>(name));

	if (!(seg.prot & LM_PROT_R))
	{
		return nullptr;
	}

	return name;
}

lm_address_t MemHlp::searchOffsetByTypeName(const void* pClass, const std::string& name)
{
	constexpr lm_address_t MAX_OFFSET = 0x10000;
	lm_address_t offset = LM_ADDRESS_BAD;

	const auto start = std::chrono::system_clock::now();

	for (lm_address_t off = sizeof(off); off <= MAX_OFFSET; off++)
	{
		const lm_address_t addr = reinterpret_cast<lm_address_t>(pClass) + off;
		const char* typeName = getTypeName(reinterpret_cast<void*>(addr));

		if (!typeName)
		{
			continue;
		}

		if (strcmp(typeName, name.c_str()) == 0)
		{
			offset = off;
			break;
		}
	}

	if (offset == LM_ADDRESS_BAD)
	{
		LOG_ERROR("Failed to find offset for %s!\n", name.c_str());
	}
	else
	{
		const auto end = std::chrono::system_clock::now();
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		LOG_DEBUG("Found offset for %s 0x%x in %llums\n", name.c_str(), offset, ms);
	}

	return offset;
}
