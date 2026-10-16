#include "decompiler.hpp"

#include "log.hpp"
#include "utils.hpp"

#include "libmem/libmem.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <elf.h>
#include <regex>
#include <sstream>
#include <string>
#include <vector>


void VFTable::init(const lm_address_t addr, const lm_module_t& mod)
{
	this->moduleBase = mod.base;
	this->address = addr;
	this->typeInfo = reinterpret_cast<TypeInfo*>(addr + sizeof(this->address));
	this->functions = std::vector<lm_address_t>();
	this->subclasses = std::map<unsigned int, VFTable>();
}

unsigned int VFTable::analyze()
{
	//*(address + 0) = 0
	//*(address + sizeof(lm_address_t)) = TypeInfo*
	//*(address + sizeof(lm_address_t) * 2) = First VFunc
	
	//Already analysed
	if (functions.size())
	{
		return functions.size();
	}

	const lm_address_t start = address + sizeof(lm_address_t) * 2;

	for (unsigned int i = 0; ;i++)
	{
		const lm_address_t offset = *(reinterpret_cast<lm_address_t*>(start) + i);
		if (!offset)
		{
			break;
		}

		//TODO:
		//Proper way would be to cross reference the typeInfos, but I couldn't get the calculations for that
		//right. It worked on the first 4 vftables on CUser but then started failing
		constexpr lm_address_t NEGATIVE_OFFSET = 0xFFFF0000;
		if ((offset & NEGATIVE_OFFSET) == NEGATIVE_OFFSET)
		{
			break;
		}

		this->functions.emplace_back(offset + moduleBase);
	}

	for (auto& sub : subclasses)
	{
		sub.second.analyze();
	}

	return functions.size();
}


std::unordered_map<std::string, Elf_Shdr> Decompiler::sections = std::unordered_map<std::string, Elf_Shdr>();
std::unordered_map<lm_address_t, std::string> Decompiler::picThunks = std::unordered_map<lm_address_t, std::string>();
std::unordered_map<lm_address_t, std::string> Decompiler::strings = std::unordered_map<lm_address_t, std::string>();
std::unordered_map<std::string, VFTable> Decompiler::vftables = std::unordered_map<std::string, VFTable>();

unsigned int Decompiler::getString(const lm_address_t addr, std::string* outStr)
{
	const char* pChAddr = reinterpret_cast<const char*>(addr);
	bool nullTerminated = false;
	unsigned int i = 0;

	for (; ; i++)
	{
		const char c = *(pChAddr + i);
		if (c == '\0')
		{
			nullTerminated = true;
			break;
		}

		//Is char?
		if (!std::isprint(c))
		{
			break;
		}
	}

	if (i && outStr && nullTerminated)
	{
		*outStr = std::string(pChAddr, i);
}

	//i = offset
	//i + 1 = size/num read
	return i + 1;
}

lm_address_t Decompiler::extractHexNum(const std::string& str)
{
	unsigned int start = 0;
	unsigned int end = 0;

	for (unsigned int i = 0; i < str.size(); i++)
	{
		const char c = str.at(i);

		if (c == '0' && i + 2 < str.size())
		{
			const char n = str.at(i + 1);
			if (n != 'x')
			{
				continue;
			}

			start = i + 2;
			i++;
			continue;
		}

		if (!std::isxdigit(c))
		{
			if (start)
			{
				break;
			}

			continue;
		}

		end = i;
	}

	if (start >= end)
	{
		return LM_ADDRESS_BAD;
	}

	const std::string numStr = str.substr(start, end - start + 1);
	return std::stoull(numStr, nullptr, 16);
}

bool Decompiler::getRelativeTarget(const lm_inst_t& instr, lm_address_t& target)
{
	auto str = std::string(instr.op_str);
	//Numbers start with 0x
	if (str.size() < 3)
	{
		return false;
	}
	str = str.substr(2, str.size() - 2);

	if (std::find_if (str.begin(), str.end(), [](const unsigned char c) { return !std::isxdigit(c); }) != str.end())
	{
		return false;
	}

	target = std::stoul(str, nullptr, 16);
	return true;
}

lm_address_t Decompiler::getLeaOffset(lm_inst_t& callInstr)
{
	lm_address_t offset = LM_ADDRESS_BAD;
	std::string thunkReg;

	if (!isPICThunk(callInstr, &thunkReg))
	{
		return LM_ADDRESS_BAD;
	}

	//Thunk moves the return address into our target register
	offset = callInstr.address + callInstr.size;
	//LOG_DEBUG("Found thunk with %s target\n", thunkRegister.c_str());

	lm_inst_t nextInstr;
	if (!LM_Disassemble(offset, &nextInstr))
	{
		LOG_DEBUG("Failed to disassemble next in getLeaOffset at 0x%lx\n", offset);
		return LM_ADDRESS_BAD;
	}

	//Thunk is followed by 'add thunkReg, num'
	if (strcmp(nextInstr.mnemonic, "add") != 0)
	{
		LOG_DEBUG("Failed to get lea offset at 0x%lx, next instruction is not an add instruction!\n", offset);
		return LM_ADDRESS_BAD;
	}

	//LOG_DEBUG("Found %s %s\n", instr.mnemonic, instr.op_str);
	auto split = Utils::strsplit(nextInstr.op_str, ",");
	if (split[0] != thunkReg)
	{
		LOG_DEBUG("Failed to get lea offset at 0x%lx, split[0] != thunkReg\n", callInstr.address);
		return LM_ADDRESS_BAD;
	}

	split[1] = split[1].substr(3, split[1].size() - 3);
	offset += std::stoul(split[1], nullptr, 16);

	return offset;
}

Elf_Shdr* Decompiler::getSection(const lm_module_t& mod, const char* name)
{
	std::ostringstream ss;
	ss << mod.name << "::" << name;

	const auto secName = ss.str();
	if (!sections.contains(secName))
	{
		return nullptr;
	}

	return &sections.at(secName);
}

bool Decompiler::isPICThunk(const lm_inst_t& callInstr, std::string* targetRegister)
{
	//Shit is slow, so we cache thunks we already found
	if (picThunks.contains(callInstr.address))
	{
		if (targetRegister)
		{
			*targetRegister = picThunks.at(callInstr.address);
		}

		return true;
	}

	if (strcmp(callInstr.mnemonic, "call") != 0)
	{
		return false;
	}

	//LOG_DEBUG("Checking %s %s at 0x%x\n", callInstr.mnemonic, callInstr.op_str, callInstr.address);

	lm_address_t target;
	if (!getRelativeTarget(callInstr, target))
	{
		return false;
	}

	//LOG_DEBUG("Call target at 0x%x\n", target);

	std::string espTarget;

	lm_inst_t instr;
	for (unsigned int i = 0; i < 2; i++)
	{
		if (!LM_Disassemble(target, &instr))
		{
			LOG_DEBUG("Failed to disassemble 0x%lx!\n", target);
			return false;
		}

		target += instr.size;

		auto splits = std::vector<std::string>();

		switch(i)
		{
			case 0:
				if (strcmp(instr.mnemonic, "mov") != 0)
				{
					return false;
				}

				splits = Utils::strsplit(instr.op_str, ",");
				espTarget = splits[0];

				//LOG_DEBUG("Target %s\n", espTarget.c_str());

				break;

			case 1:
				if (strcmp(instr.mnemonic, "ret") != 0)
				{
					return false;
				}

				break;
		}
	}

	//LOG_DEBUG("Found PIC thunk call at 0x%x\n", callInstr.address);
	picThunks[callInstr.address] = espTarget;

	if (targetRegister)
	{
		*targetRegister = espTarget;
	}

	return true;
}

void Decompiler::collectStrings(const lm_module_t& mod, const Elf_Shdr& section)
{
	const lm_address_t start = mod.base + section.rva;
	const lm_address_t end = start + section.size;

	std::string strBuf;

	for (lm_address_t addr = start; addr < end; )
	{
		lm_address_t begin = addr;
		unsigned int read = getString(addr, &strBuf);

		addr += read;

		//Check strBuf.size() because read will be filled for non null terminated strings too
		if (strBuf.size() < MIN_STRING_SIZE)
		{
			continue;
		}

		strings[begin] = strBuf;
		//LOG_DEBUG("Found string %s at 0x%x with size %u\n", strBuf.c_str(), begin, strBuf.size());
	}
}

bool Decompiler::collectVFTables(const lm_module_t& mod, const Elf_Shdr& section)
{
	const lm_address_t start = mod.base + section.rva;
	const lm_address_t end = start + section.size;

	auto typeInfos = std::unordered_map<lm_address_t, std::string>();

	//TODO: Iterate the section backwards to do it all in one pass
	//TODO: Improve typeInfo detection, since it also detects arbitrary strings at TypeInfos
	//Potential solutions: Matching the name via regex

	//First pass to collect all typeInfos. Luckily .data.rel.ro seems to be sizeof(lm_address_t) byte aligned
	for (lm_address_t addr = start; addr < end; addr += sizeof(addr))
	{
		const lm_address_t offset = *reinterpret_cast<const lm_address_t*>(addr);
		if (!offset)
		{
			continue;
		}

		const lm_address_t ptr = mod.base + offset;

		if (ptr < mod.base)
		{
			continue;
		}

		if (ptr > mod.end)
		{
			continue;
		}

		if (strings.contains(ptr))
		{
			//TypeName at TypeInfo + sizeof(lm_address_t)
			const lm_address_t typeInfo = addr - sizeof(addr);
			const auto& name = strings.at(ptr);

			//LOG_DEBUG("TypeInfo for %s at 0x%x\n", name.c_str(), typeInfo);
			typeInfos[typeInfo] = name;
		}
	}

	//Second pass to find actual VFTables by looking up the TypeInfos
	for (lm_address_t addr = start; addr < end; addr += sizeof(addr))
	{
		const lm_address_t offset = *reinterpret_cast<const lm_address_t*>(addr);
		if (!offset)
		{
			continue;
		}

		const lm_address_t ptr = mod.base + offset;

		if (ptr < mod.base)
		{
			continue;
		}

		if (ptr > mod.end)
		{
			continue;
		}

		if (typeInfos.contains(ptr))
		{
			const lm_address_t vftAddr = addr - sizeof(addr);
			const auto& name = typeInfos.at(ptr);

			auto vft = VFTable();
			vft.init(vftAddr, mod);

			if (vftables.contains(name))
			{
				auto& parent = vftables[name];
				parent.subclasses[parent.subclasses.size()] = vft;
				continue;
			}

			vftables[name] = vft;
			//LOG_DEBUG("VFTable %s at 0x%x\n", name.c_str(), vft.address);
		}
	}

	return false;
}

bool Decompiler::parseHeader(const lm_module_t& mod)
{
	auto file = CELFExecutableFile();
	if (!file.load(mod.path))
	{
		return false;
	}

	for (const auto& shdr : file.sections)
	{
		LOG_DEBUG("Section header name %s, address 0x%lx, offset 0x%lx\n", shdr.name.c_str(), shdr.rva, shdr.offset);

		auto mapName = std::string(mod.name) + "::" + shdr.name;
		sections[mapName] = shdr;
	}

	return true;
}

void Decompiler::parseModule(const lm_module_t &mod)
{
	if (!parseHeader(mod))
	{
		return;
	}

	const Elf_Shdr* shROData = getSection(mod, ".rodata");
	if (shROData)
	{
		//Collect strings to cross-reference
		LOG_DEBUG("Scanning .rodata for strings in %s\n", mod.name);
		collectStrings(mod, *shROData);
	}

	const Elf_Shdr* shRODataStr = getSection(mod, ".rodata.str");
	if (shRODataStr)
	{
		LOG_DEBUG("Scanning .rodata.str for strings in %s\n", mod.name);
		collectStrings(mod, *shRODataStr);
	}

	const Elf_Shdr* shDataRelRO = getSection(mod, ".data.rel.ro");
	if (shDataRelRO)
	{
		LOG_DEBUG("Scanning .data.rel.ro for VFTables in %s\n", mod.name);
		//Use collected strings to identify typeInfos, then cross reference those to find VFTables
		collectVFTables(mod, *shDataRelRO);
	}
}

void Decompiler::parseFunction(const lm_address_t begin, std::unordered_map<lm_address_t, unsigned int>& references)
{
	auto branchesTaken = std::unordered_set<lm_address_t>();
	__parseFunction(begin, references, branchesTaken);
}

void Decompiler::__parseFunction
(
	const lm_address_t begin,
	std::unordered_map<lm_address_t, unsigned int>& references,
	std::unordered_set<lm_address_t>& branchesTaken
)
{
	lm_address_t addr = begin;
	lm_inst_t instr;

	unsigned int numCC = 0;

	for (;;)
	{
		if (!LM_Disassemble(addr, &instr))
		{
			LOG_WARN("Failed to disassemble function 0x%lx at 0x%lx!\n", begin, addr);
			return;
		}

		//LOG_DEBUG("0x%lx: %s %s\n", addr, instr.mnemonic, instr.op_str);

		addr += instr.size;

		//Shitty way to detect function boundaries
		//TODO: Investigate wheter there is a better way
		if (strcmp(instr.mnemonic, "int3") == 0)
		{
			numCC++;

			if (numCC >= 3)
			{
				return;
			}
		}
		else
		{
			numCC = 0;
		}

		if (instr.mnemonic[0] == 'j')
		{
			lm_address_t branch;

			if (!getRelativeTarget(instr, branch))
			{
				LOG_WARN("Failed to follow %s %s at 0x%lx!\n", instr.mnemonic, instr.op_str, instr.address);
				return;
			}

			const bool taken = branchesTaken.contains(branch);

			//LOG_DEBUG("%s branch %s %s at 0x%lx to 0x%lx\n", (taken ? "Taking" : "Skipping"), instr.mnemonic, instr.op_str, instr.address, branch);
			branchesTaken.emplace(branch);

			if (strcmp(instr.mnemonic, "jmp") == 0)
			{
				//Explored already, so we abort
				if (taken)
				{
					return;
				}

				addr = branch;
			}
			else
			{
				if (!taken)
				{
					__parseFunction(branch, references, branchesTaken);
				}
			}

			continue;
		}
		//Checking the log it seems like capstone is turning all ret instructions into just ret.
		//But just in case we check for all of them
		else if (strcmp(instr.mnemonic, "ret") == 0 || strcmp(instr.mnemonic, "retn") == 0 || strcmp(instr.mnemonic, "retf") == 0)
		{
			//LOG_DEBUG("Hit %s instruction at 0x%x, stopping\n", instr.mnemonic, instr.address);
			return;
		}

		if (strcmp(instr.mnemonic, "lea") != 0)
		{
			continue;
		}

		//TODO: 64-bits Optimize
		const auto str = std::string(instr.op_str);
		const auto re = std::regex(", \\[rip - 0x[0-9,a-f]+\\]");
		std::smatch smatch;

		//LOG_DEBUG("lea found at 0x%lx, %s %s\n", instr.address, instr.mnemonic, instr.op_str);

		if (!std::regex_search(str, smatch, re))
		{
			continue;
		}

		lm_address_t targetAddr = (instr.address + instr.size) - extractHexNum(instr.op_str);

		//if (!strstr(instr.op_str, thunkReg.c_str()))
		//{
		//	continue;
		//}

		//lm_address_t targetAddr;
		//if (strstr(instr.op_str, "-"))
		//{
		//	const lm_address_t offset = extractHexNum(instr.op_str);
		//	targetAddr = leaOffset - offset;
		//}
		//else if (strstr(instr.op_str, "+"))
		//{
		//	const lm_address_t offset = extractHexNum(instr.op_str);
		//	targetAddr = leaOffset + offset;
		//}
		//else
		//{
		//	continue;
		//}

		//LOG_DEBUG("Target addr for op 0x%lx\n", targetAddr);

		if (!strings.contains(targetAddr))
		{
			continue;
		}

		//const auto& str = strings.at(targetAddr);
		//LOG_DEBUG("String reference to %s at 0x%x\n", str.c_str(), instr.address);
		references[targetAddr]++;
	}
}

std::map<std::string, unsigned int> Decompiler::parseInterfaceMapBase(const char* interface)
{
	auto functionMap = std::map<std::string, unsigned int>();
	if (!vftables.contains(interface))
	{
		return functionMap;
	}

	auto& vft = vftables[interface];

	LOG_DEBUG("Disassembling %s's functions\n", interface);

	for (unsigned int i = 0; i < vft.functions.size(); i++)
	{
		//LOG_DEBUG("Decompiling %u\n", i);
		const lm_address_t fn = vft.functions[i];

		auto refs = std::unordered_map<lm_address_t, unsigned int>();
		parseFunction(fn, refs);

		for (const auto& ref : refs)
		{
			auto str = strings[ref.first];
			if (strstr(interface, str.c_str()))
			{
				continue;
			}

			if (functionMap.contains(str))
			{
				//Some functions are overloaded, so we append a 2, 3, etc
				unsigned int idx = 2;
				for (;;)
				{
					auto indexedStr = str + std::to_string(idx);
					if (!functionMap.contains(indexedStr))
					{
						str = indexedStr;
						break;
					}

					idx++;
				}
			}

			//I would love to add a break statement after this.
			//But some functions do reference multiple strings
			//Since we can't know which one is the right one without applying possibly wrong heuristics
			//We just index all of them
			functionMap[str] = i;
			//LOG_DEBUG("%s::%s at %u (%u times)\n", interface, str.c_str(), i, ref.second);
		}
	}

	return functionMap;
}

void Decompiler::cleanUp()
{
	sections.clear();
	picThunks.clear();
	strings.clear();
	vftables.clear();
}
