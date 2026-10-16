#pragma once

#include "process.hpp"

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
	extern std::unordered_map<std::string, VFTable> vftables;

	unsigned int getString(const lm_address_t addr, std::string* outStr);
	lm_address_t extractHexNum(const std::string& str);
	bool isPICThunk(const lm_inst_t& callInstr, std::string* targetRegister);
	bool getRelativeTarget(const lm_inst_t& instr, lm_address_t& target);
	lm_address_t getLeaOffset(lm_inst_t& callInstr);

	Elf_Shdr* getSection(const lm_module_t& mod, const char* name);

	void collectStrings(const lm_module_t& mod, const Elf_Shdr& section);
	bool collectVFTables(const lm_module_t& mod, const Elf_Shdr& section);

	bool parseHeader(const lm_module_t& mod);
	void parseModule(const lm_module_t& mod);

	void parseFunction(const lm_address_t begin, std::unordered_map<lm_address_t, unsigned int>& references);
	void __parseFunction
	(
		const lm_address_t begin,
		std::unordered_map<lm_address_t, unsigned int>& references,
		std::unordered_set<lm_address_t>& branchesTaken
	);
	
	std::map<std::string, unsigned int> parseInterfaceMapBase(const char* interface);

	void cleanUp();
}
