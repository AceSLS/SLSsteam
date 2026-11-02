#include "decompiler.hpp"

#include "log.hpp"
#include "utils.hpp"

#include "libmem/libmem.h"

#include <cctype>
#include <cstring>
#include <dlfcn.h>
#include <elf.h>
#include <regex>
#include <sstream>
#include <string>
#include <vector>


ARMCPUContext_t::Reg_t& ARMCPUContext_t::regFromString(const std::string& name)
{
	if (name == "sp")
	{
		return sp;
	}
	else if (name == "pc")
	{
		return pc;
	}
	else if (name == "br")
	{
		return br;
	}

	const auto numStr = name.substr(1, name.size() - 1);
	const auto idx = std::stoul(numStr);

	return registers[idx];
}

CARMEmu::CARMEmu(const lm_address_t pc)
{
	cpu.pc.xu = pc;
}

std::unordered_set<ARMCPUContext_t::Reg_t*> CARMEmu::run()
{
	lm_inst_t inst { };
	if (!LM_Disassemble(cpu.pc.xu, &inst))
	{
		LOG_ERROR("Failed to disassemble code at %lx!", cpu.pc.xu);
	}

	return run(inst);
}

std::unordered_set<ARMCPUContext_t::Reg_t*> CARMEmu::run(const lm_inst_t& inst)
{
	auto changedRegs = std::unordered_set<ARMCPUContext_t::Reg_t*>();
	
	if (strcmp(inst.mnemonic, "add") == 0)
	{
		const auto split = Utils::strsplit(inst.op_str, ", ");

		//This just assumes every register is 64 bits
		//But for our intends and purposes this should be enough
		auto& targetReg = cpu.regFromString(split[0]);
		const auto& reg1 = cpu.regFromString(split[1]);

		if (split[2][0] == '#')
		{
			targetReg.xu = reg1.xu + Decompiler::extractHexNum(split[2]);
		}
		else
		{
			const auto& reg2 = cpu.regFromString(split[2]);
			targetReg.xu = reg1.xu + reg2.xu;
		}

		changedRegs.emplace(&targetReg);
	}
	else if (strcmp(inst.mnemonic, "adrp") == 0)
	{
		//libmem already handles the relative addressing
		//adrp x21, #0xffff323d6000
		const auto split = Utils::strsplit(inst.op_str, ", ");

		const lm_address_t val = Decompiler::extractHexNum(split[1]);
		auto& reg = cpu.regFromString(split[0]);
		reg.xu = val;

		changedRegs.emplace(&reg);
	}
	else if (strcmp(inst.mnemonic, "ldr") == 0)
	{
		//ldr w4, [x2, #0x100]
		//ldr w4, [x2]
		//etc
		const auto split = Utils::strsplit(inst.op_str, ", ");

		lm_address_t ldrAddr = 0;

		for (unsigned int i = 1; i < split.size(); i++)
		{
			auto addr = *std::next(split.begin(), i);

			if (addr.starts_with("["))
			{
				addr = addr.substr(1, addr.size() - 1);
			}
			if (addr.ends_with("]"))
			{
				addr = addr.substr(0, addr.size() - 1);
			}

			if (addr[0] == '#')
			{
				ldrAddr += Decompiler::extractHexNum(addr);
			}
			else
			{
				const auto& sourceReg = cpu.regFromString(addr);
				ldrAddr += sourceReg.xu;
			}
		}

		auto& targetReg = cpu.regFromString(split[0]);
		targetReg.xu = ldrAddr;

		changedRegs.emplace(&targetReg);
	}
	else if (strcmp(inst.mnemonic, "mov") == 0 || strcmp(inst.mnemonic, "movz") == 0)
	{
		const auto split = Utils::strsplit(inst.op_str, ", ");
		auto& targetReg = cpu.regFromString(split[0]);

		if (split[1][0] == '#')
		{
			targetReg.xu = Decompiler::extractHexNum(split[1]);
		}
		else
		{
			const auto& sourceReg = cpu.regFromString(split[1]);
			targetReg.xu = sourceReg.xu;
		}

		changedRegs.emplace(&targetReg);
	}
	else if (strcmp(inst.mnemonic, "movn") == 0)
	{
		//TODO: Merge into mov family
		const auto split = Utils::strsplit(inst.op_str, ", ");
		auto& targetReg = cpu.regFromString(split[0]);

		if (split[1][0] == '#')
		{
			targetReg.xu = ~Decompiler::extractHexNum(split[1]);
		}
		else
		{
			const auto& sourceReg = cpu.regFromString(split[1]);
			targetReg.xu = ~sourceReg.xu;
		}

		changedRegs.emplace(&targetReg);
	}

done:
	cpu.pc.xu += inst.size;
	return changedRegs;
}

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
std::unordered_map<lm_address_t, std::string> Decompiler::typeInfos = std::unordered_map<lm_address_t, std::string>();
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

	if (outStr)
	{
		if (i && nullTerminated)
		{
			*outStr = std::string(pChAddr, i);
		}
		else
		{
			outStr->clear();
		}
	}

	//i = offset
	//i + 1 = size/num read
	return i + 1;
}

lm_address_t Decompiler::extractHexNum(const std::string& str)
{
	unsigned int start = 0;
	unsigned int end = start;

	bool found0x = false;

	for (unsigned int i = 0; i < str.size(); i++)
	{
		if (!found0x)
		{
			if (i && str.at(i) == 'x' && str.at(i - 1) == '0')
			{
				found0x = true;
				start = i + 1;
			}

			continue;
		}

		if (!std::isxdigit(str.at(i)) || i == str.size() - 1)
		{
			end = i + 1;
			break;
		}
	}

	if (start >= end)
	{
		return LM_ADDRESS_BAD;
	}

	const auto numStr = str.substr(start, end - start);
	return std::stoull(numStr, nullptr, 16);
}

bool Decompiler::getRelativeTarget(const lm_inst_t& instr, lm_address_t& target)
{
	target = extractHexNum(instr.op_str);

	return target != LM_ADDRESS_BAD;
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
		//LOG_DEBUG("Found string %s at 0x%lx with size %lu\n", strBuf.c_str(), begin, strBuf.size());
	}
}

void Decompiler::collectTypeInfos(const lm_module_t& mod, const Elf_Shdr& section)
{
	const lm_address_t start = mod.base + section.rva;
	const lm_address_t end = start + section.size;

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

			//LOG_DEBUG("TypeInfo for %s at 0x%lx\n", name.c_str(), typeInfo);
			typeInfos[typeInfo] = name;
		}
	}
}

bool Decompiler::collectVFTables(const lm_module_t& mod, const Elf_Shdr& section)
{
	const lm_address_t start = mod.base + section.rva;
	const lm_address_t end = start + section.size;

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
			//LOG_DEBUG("VFTable %s at 0x%lx\n", name.c_str(), vft.address);
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

	//AMD64 & partially ARM64
	const Elf_Shdr* shDataRelRO = getSection(mod, ".data.rel.ro");
	if (shDataRelRO)
	{
		LOG_DEBUG("Scanning .data.rel.ro for VFTables in %s\n", mod.name);
		//Use collected strings to identify typeInfos, then cross reference those to find VFTables
		collectTypeInfos(mod, *shDataRelRO);
		collectVFTables(mod, *shDataRelRO);
	}

	//ARM64 exclusive
	const Elf_Shdr* shDataRelROLocal = getSection(mod, ".data.rel.ro.local");
	if (shDataRelROLocal)
	{
		LOG_DEBUG("Scanning .data.rel.ro.local for VFTables in %s\n", mod.name);
		//Use collected strings to identify typeInfos, then cross reference those to find VFTables
		collectVFTables(mod, *shDataRelROLocal);
	}
}

void Decompiler::parseFunction(const lm_address_t begin, std::unordered_map<lm_address_t, unsigned int>& references)
{
	auto branchesTaken = std::unordered_set<lm_address_t>();
	CARMEmu cpu = CARMEmu(begin);
	__parseFunction(begin, references, branchesTaken, &cpu);
#ifdef ARM64
#else
	__parseFunction(begin, references, branchesTaken, nullptr);
#endif
}

void Decompiler::__parseFunction
(
	const lm_address_t begin,
	std::unordered_map<lm_address_t, unsigned int>& references,
	std::unordered_set<lm_address_t>& branchesTaken,
	CARMEmu* armCPU
)
{

#ifdef ARM64
	//TODO: Add handling of TB* (TBNZ for example)
	constexpr const char* JMP_MNEMONIC = "b";
	constexpr const char* CALL_MNEMONIC = "bl";
#else
	constexpr const char* JMP_MNEMONIC = "jmp";
#endif

	lm_address_t addr = begin;
	lm_inst_t instr;

#ifdef X86_64
	unsigned int numCC = 0;
#endif

	for (;;)
	{
		if (!LM_Disassemble(addr, &instr))
		{
			LOG_WARN("Failed to disassemble function 0x%lx at 0x%lx!\n", begin, addr);
			return;
		}

		//LOG_DEBUG("0x%lx: %s %s\n", addr, instr.mnemonic, instr.op_str);

		addr += instr.size;

	  #ifdef X86_64
		//Shitty way to detect function boundaries
		//Arm functions have proper flow, so we don't have to hack around it
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
	  #endif

	#ifdef ARM64
		//Follow branches, not calls
		if (instr.mnemonic[0] == JMP_MNEMONIC[0] && !std::string(instr.mnemonic).starts_with(CALL_MNEMONIC))
	#else
		if (instr.mnemonic[0] == JMP_MNEMONIC[0])
	#endif
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

			//Unconditional branch, take
			if (strcmp(instr.mnemonic, JMP_MNEMONIC) == 0)
			{
				//Explored already, so we abort
				if (taken)
				{
					return;
				}

				addr = branch;
			}
			//Conditional branch, recurse
			else
			{
				if (!taken)
				{
					if (armCPU)
					{
						CARMEmu cpuCpy(*armCPU);
						__parseFunction(branch, references, branchesTaken, &cpuCpy);
					}
					else
					{
						__parseFunction(branch, references, branchesTaken, nullptr);
					}
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

	#ifdef ARM64
		const auto regs = armCPU->run(instr);
		for (const auto reg : regs)
		{
			if (strings.contains(reg->xu))
			{
				//LOG_DEBUG("Found reference to %s!\n", strings.at(reg->xu).c_str());
				references[reg->xu]++;
			}
		}
	#else
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

		//LOG_DEBUG("Target addr for op 0x%lx\n", targetAddr);

		if (!strings.contains(targetAddr))
		{
			continue;
		}

		//const auto& str = strings.at(targetAddr);
		//LOG_DEBUG("String reference to %s at 0x%x\n", str.c_str(), instr.address);
		references[targetAddr]++;
	#endif
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
	typeInfos.clear();
	vftables.clear();
}
