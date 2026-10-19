#include "process.hpp"

#include "sdk/CSteamEngine.hpp"

#include "config.hpp"
#include "log.hpp"
#include "utils.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iterator>
#include <memory>
#include <regex>
#include <unordered_set>


IExecutableFile::~IExecutableFile()
{
	if (file)
	{
		fclose(file);
	}
}

bool IExecutableFile::load(const std::string& filePath, const LogLevelFlags_t logErrorFlags)
{
	path = filePath;
	file = fopen(path.c_str(), "r");
	errorFlags = logErrorFlags;

	if (!file)
	{
		LOG_CUSTOM(errorFlags, "Failed to open %s!\n", path.c_str());
		return false;
	}

	if (!checkMagic())
	{
		return false;
	}

	if (!parseSections())
	{
		return false;
	}
	
	return true;
}

std::vector<uint8_t> IExecutableFile::readSection(const SectionHdr_t& section)
{
	if (fseek(file, section.offset, SEEK_SET) != 0)
	{
		LOG_CUSTOM(errorFlags, "Failed to seek to section %s!\n", section.name.c_str());
		return { };
	}

	auto bytes = std::vector<uint8_t>();
	bytes.resize(section.size);

	if (fread(bytes.data(), bytes.size(), 1, file) < 1)
	{
		LOG_CUSTOM(errorFlags, "Failed to read section %s!\n", section.name.c_str());
		return { };
	}

	return bytes;
}

std::unique_ptr<IExecutableFile> IExecutableFile::create(const std::string& path, const LogLevelFlags_t logErrorFlags)
{
	std::unique_ptr<IExecutableFile> file = std::make_unique<CPortableExecutableFile>();
	if (file->load(path, logErrorFlags))
	{
		return file;
	}

	file = std::make_unique<CELFExecutableFile>();
	if (file->load(path, logErrorFlags))
	{
		return file;
	}

	return nullptr;
}

bool CPortableExecutableFile::checkMagic()
{
	if (fseek(file, 0, SEEK_SET) != 0)
	{
		LOG_CUSTOM(errorFlags, "Failed to seek to magic!\n");
		return false;
	}

	std::string magic;
	magic.resize(2);

	if (fread(magic.data(), magic.size(), 1, file) < 1)
	{
		LOG_CUSTOM(errorFlags, "Failed to read e_magic!\n");
		return false;
	}

	if (magic != "MZ")
	{
		return false;
	}

	return true;
}

bool CPortableExecutableFile::parseSections()
{
	if (!checkMagic())
	{
		return false;
	}

	LOG_DEBUG("Parsing PE %s\n", path.filename().c_str());

	if (fseek(file, 0x3C, SEEK_SET) != 0)
	{
		LOG_CUSTOM(errorFlags, "Failed to seek to e_lfanew!\n");
		return false;
	}

	uint32_t e_lfanew;
	if (fread(&e_lfanew, sizeof(e_lfanew), 1, file) < 1)
	{
		LOG_CUSTOM(errorFlags, "Failed to read e_lfanew!\n");
		return false;
	}

	if (fseek(file, e_lfanew, SEEK_SET) != 0)
	{
		LOG_CUSTOM(errorFlags, "Failed to seek to e_lfanew!\n");
		return false;
	}

	uint8_t peHdr[8] { };

	if (fread(peHdr, sizeof(peHdr), 1, file) < 1)
	{
		LOG_CUSTOM(errorFlags, "Failed to read NT_HEADER64!\n");
		return false;
	}

	const uint16_t machine = *reinterpret_cast<uint16_t*>(&peHdr[4]); //Bitness
	const uint16_t numberOfSections = *reinterpret_cast<uint16_t*>(&peHdr[6]);

	uint64_t sectionHdrsOffset = e_lfanew;

	if (machine == MACHINE_I386)
	{
		sectionHdrsOffset += PE_HEADER32_SIZE;
		LOG_DEBUG("Parsing as 32 bit file\n");
	}
	else if (machine == MACHINE_X64)
	{
		sectionHdrsOffset += PE_HEADER64_SIZE;
		LOG_DEBUG("Parsing as 64 bit file\n");
	}
	else
	{
		LOG_CUSTOM(errorFlags, "Unknown machine %u!\n", machine);
		return false;
	}

	if (fseek(file, sectionHdrsOffset, SEEK_SET) != 0)
	{
		LOG_CUSTOM(errorFlags, "Failed to seek to section headers!\n");
		return false;
	}

	for (size_t i = 0; i < numberOfSections; i++)
	{
		uint8_t sectHdr[SECTION_HEADER_SIZE];

		if (fread(sectHdr, sizeof(sectHdr), 1, file) < 1)
		{
			LOG_CUSTOM(errorFlags, "Failed to read section header %lu!\n", i);
			return false;
		}

		char name[SECTION_HEADER_NAME_SIZE + 1] { }; //Zero allocate buffer for null termination
		strncpy(name, reinterpret_cast<char*>(sectHdr), sizeof(name) - 1);
		//snprintf(name, sizeof(name), "%s", reinterpret_cast<char*>(sectHdr));

		const uint32_t rva = *reinterpret_cast<uint32_t*>(&sectHdr[0xC]);
		const uint32_t size = *reinterpret_cast<uint32_t*>(&sectHdr[0x10]);
		const uint32_t ptr = *reinterpret_cast<uint32_t*>(&sectHdr[0x14]);

		if (g_config.extendedLogging.copy())
		{
			LOG_DEBUG("Section header %s at 0x%x with size 0x%x\n", name, ptr, size);
		}

		sections.emplace_back(SectionHdr_t { std::string(name, strnlen(name, sizeof(name))), rva, ptr, size });
	}

	return true;
}

bool CELFExecutableFile::parseElf32Headers(const Elf32_Ehdr& hdr)
{
	if (sizeof(Elf32_Shdr) < hdr.e_shentsize)
	{
		LOG_CUSTOM(errorFlags, "hdr.e_shentsize < sizeof(Elf_Shdr)!\n");
		return false;
	}

	auto shdrs = std::vector<Elf32_Shdr>();
	shdrs.resize(hdr.e_shnum);

	if (fseek(file, hdr.e_shoff, SEEK_SET) != 0)
	{
		LOG_CUSTOM(errorFlags, "Failed to seek to section headers\n");
		return false;
	}

	if (fread(shdrs.data(), sizeof(Elf32_Shdr), shdrs.size(), file) < shdrs.size())
	{
		LOG_CUSTOM(errorFlags, "Failed to read section headers\n");
		return false;
	}

	const Elf32_Shdr& strHdr = shdrs.at(hdr.e_shstrndx);
	auto strSec = std::vector<char>();
	strSec.resize(strHdr.sh_size);

	if (fseek(file, strHdr.sh_offset, SEEK_SET) != 0)
	{
		LOG_CUSTOM(errorFlags, "Failed to seek to strHdr.sh_offset!\n");
		return false;
	}

	if (fread(strSec.data(), sizeof(unsigned char), strSec.size(), file) < strSec.size())
	{
		LOG_CUSTOM(errorFlags, "Failed to read strHdr!\n");
		return false;
	}

	//LOG_DEBUG("strHdr name %u address 0x%x\n", strHdr.sh_name, strHdr.sh_offset);

	for (const auto& shdr : shdrs)
	{
		if (!shdr.sh_name)
		{
			//LOG_DEBUG("Skipping nameless section\n");
			continue;
		}

		const char* name = &strSec.at(shdr.sh_name);

		if (g_config.extendedLogging.copy())
		{
			LOG_DEBUG("Section header name %s, address 0x%x, offset 0x%x\n", name, shdr.sh_addr, shdr.sh_offset);
		}

		sections.emplace_back(SectionHdr_t { name, shdr.sh_addr, shdr.sh_offset, shdr.sh_size });
	}

	return true;
}

bool CELFExecutableFile::parseElf64Headers(const Elf64_Ehdr& hdr)
{
	if (sizeof(Elf64_Shdr) < hdr.e_shentsize)
	{
		LOG_CUSTOM(errorFlags, "hdr.e_shentsize < sizeof(Elf_Shdr)!\n");
		return false;
	}

	auto shdrs = std::vector<Elf64_Shdr>();
	shdrs.resize(hdr.e_shnum);

	if (fseek(file, hdr.e_shoff, SEEK_SET) != 0)
	{
		LOG_CUSTOM(errorFlags, "Failed to seek to section headers\n");
		return false;
	}

	if (fread(shdrs.data(), sizeof(Elf64_Shdr), shdrs.size(), file) < shdrs.size())
	{
		LOG_CUSTOM(errorFlags, "Failed to read section headers\n");
		return false;
	}

	const Elf64_Shdr& strHdr = shdrs.at(hdr.e_shstrndx);
	auto strSec = std::vector<char>();
	strSec.resize(strHdr.sh_size);

	if (fseek(file, strHdr.sh_offset, SEEK_SET) != 0)
	{
		LOG_CUSTOM(errorFlags, "Failed to seek to strHdr.sh_offset!\n");
		return false;
	}

	if (fread(strSec.data(), sizeof(unsigned char), strSec.size(), file) < strSec.size())
	{
		LOG_CUSTOM(errorFlags, "Failed to read strHdr!\n");
		return false;
	}

	//LOG_DEBUG("strHdr name %u address 0x%llx\n", strHdr.sh_name, strHdr.sh_offset);

	for (const auto& shdr : shdrs)
	{
		if (!shdr.sh_name)
		{
			//LOG_DEBUG("Skipping nameless section\n");
			continue;
		}

		const char* name = &strSec.at(shdr.sh_name);

		if (g_config.extendedLogging.copy())
		{
			LOG_DEBUG("Section header name %s, address 0x%lx, offset 0x%lx\n", name, shdr.sh_addr, shdr.sh_offset);
		}

		sections.emplace_back(SectionHdr_t { name, shdr.sh_addr, shdr.sh_offset, shdr.sh_size });
	}

	return true;
}

bool CELFExecutableFile::checkMagic()
{
	uint8_t magic[4] { };

	if (fseek(file, 0, SEEK_SET) != 0)
	{
		LOG_CUSTOM(errorFlags, "Failed to seek to magic!\n");
		return false;
	}

	if (fread(magic, sizeof(magic), 1, file) < 1)
	{
		LOG_CUSTOM(errorFlags, "Failed to read magic!\n");
		return false;
	}
	
	if
	(
		magic[EI_MAG0] != ELFMAG0
		|| magic[EI_MAG1] != ELFMAG1
		|| magic[EI_MAG2] != ELFMAG2
		|| magic[EI_MAG3] != ELFMAG3
	)
	{
		return false;
	}

	return true;
}

bool CELFExecutableFile::parseSections()
{
	if (!checkMagic())
	{
		return false;
	}

	LOG_DEBUG("Parsing ELF %s\n", path.filename().c_str());

	if (fseek(file, 0, SEEK_SET) != 0)
	{
		LOG_CUSTOM(errorFlags, "Failed to seek to file beginning!\n");
		return false;
	}

	Elf64_Ehdr hdr64;
	if (fread(&hdr64, sizeof(hdr64), 1, file) < 1)
	{
		LOG_CUSTOM(errorFlags, "Failed to read Elf header!\n");
		return false;
	}

	if (hdr64.e_ident[EI_CLASS] == ELFCLASS32 && hdr64.e_machine == ISA_X86)
	{
		LOG_DEBUG("Parsing as 32 bit file\n");

		//Headers are the same till e_entry. The 64bit version is longer
		//so we can just recast it
		Elf32_Ehdr hdr32 = *reinterpret_cast<Elf32_Ehdr*>(&hdr64);
		return parseElf32Headers(hdr32);
	}
	else if (hdr64.e_ident[EI_CLASS] == ELFCLASS64 && hdr64.e_machine == ISA_AMD64)
	{
		LOG_DEBUG("Parsing as 64 bit file\n");
		return parseElf64Headers(hdr64);
	}
	else
	{
		LOG_CUSTOM(errorFlags, "Unknown ELFCLASS/e_machine %u | %u!\n", hdr64.e_ident[EI_CLASS], hdr64.e_machine);
	}

	return false;
}
