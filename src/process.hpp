#pragma once

#include "log.hpp"

#include <elf.h>
#include <filesystem>
#include <sys/types.h>
#include <vector>


struct SectionHdr_t
{
	std::string name;
	uint64_t rva;
	uint64_t offset;
	uint64_t size;
};

class IExecutableFile
{
public:
	std::filesystem::path path;
	FILE* file;
	std::vector<SectionHdr_t> sections;
	//We use this to not clutter the log with failed open file reads
	LogLevelFlags_t errorFlags;

	virtual ~IExecutableFile();

	bool load(const std::string& filePath, const LogLevelFlags_t logErrorFlags = ELogLevel::k_ELogLevelError);
	std::vector<uint8_t> readSection(const SectionHdr_t& section);

	virtual bool checkMagic() = 0;
	virtual bool parseSections() = 0;

	static std::unique_ptr<IExecutableFile> create(const std::string& path, const LogLevelFlags_t logErrorFlags = ELogLevel::k_ELogLevelError);
};

class CPortableExecutableFile : public IExecutableFile
{
public:

	constexpr static uint32_t MACHINE_I386 = 0x14c;
	constexpr static uint32_t MACHINE_X64 = 0x8664;

	constexpr static size_t PE_HEADER32_SIZE = 0xf8;
	constexpr static size_t PE_HEADER64_SIZE = 0x108;

	constexpr static size_t SECTION_HEADER_SIZE = 0x28;
	constexpr static size_t SECTION_HEADER_NAME_SIZE = 0x8;

	virtual bool checkMagic();
	virtual bool parseSections();
};

class CELFExecutableFile : public IExecutableFile
{
public:

	constexpr static uint32_t ISA_X86 = 0x3;
	constexpr static uint32_t ISA_AMD64 = 0x3e;

	constexpr static size_t ELF_HEADER32_SIZE = sizeof(Elf32_Ehdr);
	constexpr static size_t ELF_HEADER64_SIZE = sizeof(Elf64_Ehdr);

	constexpr static size_t ELF_SHEADER32_SIZE = sizeof(Elf32_Shdr);
	constexpr static size_t ELF_SHEADER64_SIZE = sizeof(Elf64_Shdr);

	bool parseElf32Headers(const Elf32_Ehdr& hdr);
	bool parseElf64Headers(const Elf64_Ehdr& hdr);

	virtual bool checkMagic();
	virtual bool parseSections();
};
