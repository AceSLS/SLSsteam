#pragma once

#include "process.hpp"

#include <cstdint>
#include <elf.h>
#include <libmem/libmem.h>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


typedef Elf64_Ehdr Elf_Ehdr;
//typedef Elf32_Shdr Elf_Shdr;
typedef SectionHdr_t Elf_Shdr;

struct ARMCPUContext_t
{
public:

	union Reg_t
	{
		uint64_t xu;
		int64_t xi;

		uint32_t wu;
		int32_t wi;
	};

	constexpr static unsigned int NUM_REGS = 31;
	Reg_t registers[NUM_REGS] { };

	Reg_t& reg0 = registers[0];
	Reg_t& reg1 = registers[1];
	Reg_t& reg2 = registers[2];
	Reg_t& reg3 = registers[3];
	Reg_t& reg4 = registers[4];
	Reg_t& reg5 = registers[5];
	Reg_t& reg6 = registers[6];
	Reg_t& reg7 = registers[7];
	Reg_t& reg8 = registers[8];
	Reg_t& reg9 = registers[9];
	Reg_t& reg10 = registers[10];
	Reg_t& reg11 = registers[11];
	Reg_t& reg12 = registers[12];
	Reg_t& reg13 = registers[13];
	Reg_t& reg14 = registers[14];
	Reg_t& reg15 = registers[15];
	Reg_t& reg16 = registers[16];
	Reg_t& reg17 = registers[17];
	Reg_t& reg18 = registers[18];
	Reg_t& reg19 = registers[19];
	Reg_t& reg20 = registers[20];
	Reg_t& reg21 = registers[21];
	Reg_t& reg22 = registers[22];
	Reg_t& reg23 = registers[23];
	Reg_t& reg24 = registers[24];
	Reg_t& reg25 = registers[25];
	Reg_t& reg26 = registers[26];
	Reg_t& reg27 = registers[27];
	Reg_t& reg28 = registers[28];
	Reg_t& reg29 = registers[29];
	Reg_t& reg30 = registers[30];

	Reg_t sp { };
	Reg_t pc { };
	Reg_t br { };

	Reg_t& regFromString(const std::string& name);
};

class CARMEmu
{
public:

	ARMCPUContext_t cpu;

	CARMEmu(const lm_address_t pc);

	//Parse instruction and return changed registers
	std::unordered_set<ARMCPUContext_t::Reg_t*> run(const lm_inst_t& inst);
};

struct VFTable
{
	struct TypeInfo
	{
		void* classTypeInfo;
		const char* typeInfoName;
		//Not all TypeInfos have a baseClass, struct is shorter for them
		//void* baseClassType;
	};

	lm_address_t moduleBase;
	lm_address_t address;
	TypeInfo* typeInfo;
	std::vector<lm_address_t> functions;
	std::map<unsigned int, VFTable> subclasses;

	void init(const lm_address_t addr, const lm_module_t& mod);
	unsigned int analyze();
};


namespace Decompiler
{
	constexpr int MIN_STRING_SIZE = 5;

	extern std::unordered_map<std::string, Elf_Shdr> sections;
	extern std::unordered_map<lm_address_t, std::string> picThunks;
	extern std::unordered_map<lm_address_t, std::string> strings;
	extern std::unordered_map<lm_address_t, std::string> typeInfos;
	extern std::unordered_map<std::string, VFTable> vftables;

	unsigned int getString(const lm_address_t addr, std::string* outStr);
	lm_address_t extractHexNum(const std::string& str);
	bool isPICThunk(const lm_inst_t& callInstr, std::string* targetRegister);
	bool getRelativeTarget(const lm_inst_t& instr, lm_address_t& target);
	lm_address_t getLeaOffset(lm_inst_t& callInstr);

	Elf_Shdr* getSection(const lm_module_t& mod, const char* name);

	void collectStrings(const lm_module_t& mod, const Elf_Shdr& section);
	void collectTypeInfos(const lm_module_t& mod, const Elf_Shdr& section);
	bool collectVFTables(const lm_module_t& mod, const Elf_Shdr& section);

	bool parseHeader(const lm_module_t& mod);
	void parseModule(const lm_module_t& mod);

	void parseFunction(const lm_address_t begin, std::unordered_map<lm_address_t, unsigned int>& references);
	void __parseFunction
	(
		const lm_address_t begin,
		std::unordered_map<lm_address_t, unsigned int>& references,
		std::unordered_set<lm_address_t>& branchesTaken,
		CARMEmu* armCPU
	);
	
	std::map<std::string, unsigned int> parseInterfaceMapBase(const char* interface);

	void cleanUp();
}
