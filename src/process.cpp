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

bool IExecutableFile::hasSteamDRM()
{
	if (!(g_config.smartTickets.copy() & CConfig::k_ESmartTicketsSteamDRM))
	{
		return false;
	}
	//SteamDRM appends .bind section with very high entropy (usually 7.9+)

	const auto& last = sections.at(sections.size() - 1);

	if (last.name == ".bind")
	{
		const auto bytes = readSection(last);
		const double entropy = Utils::calculateEntropy(bytes);

		LOG_DEBUG("%s has entropy of %f\n", last.name.c_str(), entropy);

		if (entropy >= 7.0)
		{
			return true;
		}
	}

	return false;
}

bool IExecutableFile::hasDenuvo()
{
	if (!(g_config.smartTickets.copy() & CConfig::k_ESmartTicketsDenuvo))
	{
		return false;
	}

	static const std::vector<std::string> X_SECS =
	{
		".xtext",
		".xcode",
		//".xdata", //Causes a lot of false positives
		".xpdata",
		".xtls"
	};

	static const std::vector<std::string> CODE_SECS =
	{
		//Start with potentially smaller sections first
		".text",
		".code",

		".xcode",
		".xtext",

		".text1",
	};

	constexpr double MIN_ENTROPY = 6.25;

	unsigned secsFound = 0;
	double codeEntropy = 0.0;

	for (const auto& sec : sections)
	{
		for (const auto& xsec : X_SECS)
		{
			if (sec.name == xsec)
			{
				secsFound++;
				break;
			}
		}

		//Do not waste time reading sections when MIN is already met
		if (codeEntropy >= MIN_ENTROPY)
		{
			continue;
		}

		for (const auto& csec : CODE_SECS)
		{
			if (sec.name != csec)
			{
				continue;
			}

			const auto bytes = readSection(sec);
			const double entropy = Utils::calculateEntropy(bytes);

			if (g_config.extendedLogging.copy())
			{
				LOG_DEBUG("%s entropy is %f\n", sec.name.c_str(), entropy);
			}

			codeEntropy = std::max(codeEntropy, entropy);
		}
	}

	LOG_DEBUG("%u Denuvo sections, entropy %f\n", secsFound, codeEntropy);

	//At least one denuvo section & and code has to encrypted/obfuscated
	return secsFound > 0 && codeEntropy > MIN_ENTROPY;
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
			LOG_CUSTOM(errorFlags, "Failed to read section header %i!\n", i);
			return false;
		}

		char name[SECTION_HEADER_NAME_SIZE];
		strncpy(name, reinterpret_cast<char*>(sectHdr), sizeof(name));

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

	const Elf32_Shdr& strHdr = shdrs[hdr.e_shstrndx];
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

		const char* name = &strSec[shdr.sh_name];

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

	const Elf64_Shdr& strHdr = shdrs[hdr.e_shstrndx];
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

		const char* name = &strSec[shdr.sh_name];

		if (g_config.extendedLogging.copy())
		{
			LOG_DEBUG("Section header name %s, address 0x%llx, offset 0x%llx\n", name, shdr.sh_addr, shdr.sh_offset);
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
		LOG_CUSTOM(errorFlags, "Failed to seek to Magic!\n");
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
		parseElf32Headers(hdr32);
	}
	else if (hdr64.e_ident[EI_CLASS] == ELFCLASS64 && hdr64.e_machine == ISA_AMD64)
	{
		LOG_DEBUG("Parsing as 64 bit file\n");
		parseElf64Headers(hdr64);
	}
	else
	{
		LOG_CUSTOM(errorFlags, "Unknown ELFCLASS/e_machine %u | %u!\n", hdr64.e_ident[EI_CLASS], hdr64.e_machine);
		return false;
	}

	return true;
}

std::filesystem::path Process_t::getPath(const char* fileName)
{
	std::ostringstream pathSS;
	pathSS << "/proc/" << pid << "/" << fileName;
	return pathSS.str();
}

std::string Process_t::readFile(const char* fileName)
{
	const auto path = getPath(fileName);

	auto ifstream = std::ifstream(path);
	if (!ifstream.is_open())
	{
		LOG_ERROR("Failed to read %s!\n", path.c_str());
		return "";
	}

	std::string content = std::string(std::istreambuf_iterator(ifstream), {});
	return content;
}

AppId_t Process_t::getAppIdFromEnv()
{
	auto reAppId = std::regex("SteamAppId=[0-9]+");
	std::smatch appIdMatch;

	if (!std::regex_search(environ, appIdMatch, reAppId))
	{
		LOG_ERROR("No SteamAppId in %s's environment! Using 0\n", exe.filename().c_str());
		return 0;
	}

	reAppId = std::regex("[0-9]+");
	const auto envVar = appIdMatch.str();

	std::regex_search(envVar, appIdMatch, reAppId);
	AppId_t appId = std::stoul(appIdMatch.str());

	LOG_DEBUG("AppId for process %s in 0x%x is %u\n", exe.filename().c_str(), pipeHandle, appId);
	return appId;
}

std::unordered_set<std::filesystem::path> Process_t::getOpenFiles()
{
	//Using a set because we do not want duplicates
	auto files = std::unordered_set<std::filesystem::path>();

	const auto maps = getPath("map_files");
	for (const auto& file : std::filesystem::directory_iterator { maps })
	{
		//Afaik all files should be symlinks. But better safe than sorry
		if (std::filesystem::is_symlink(file))
		{
			const auto path = std::filesystem::read_symlink(file).string();
			files.emplace(path);
		}
		else
		{
			files.emplace(file);
		}

	}

	return files;
}

std::filesystem::path Process_t::getRealExe()
{
	std::filesystem::path linkTarget;

	try
	{
		linkTarget = std::filesystem::read_symlink(getPath("exe"));
	}
	catch (...)
	{
		LOG_ERROR("Failed to follow exe link target for %u!\n", pid);
		return "";
	}

	const auto targetName = linkTarget.filename();

	if (targetName != "wine-preloader" && targetName != "wine64-preloader")
	{
		//Native game
		return linkTarget;
	}

	//Wine does not point to the actual .exe files, so we iterate the open
	//files and pick the one ending with .exe
	const auto files = getOpenFiles();
	for (const auto& path : files)
	{
		if (path.string().ends_with(".exe"))
		{
			return path;
		}
	}

	return linkTarget;
}

bool Process_t::analyse()
{
	const auto startTime = std::chrono::system_clock::now();
	const auto exeFile = IExecutableFile::create(exe);

	if (!exeFile)
	{
		LOG_ERROR("Failed to parse %s!\n", exe.filename().c_str());
		return false;
	}

	steamDRM = exeFile->hasSteamDRM();
	denuvo = exeFile->hasDenuvo();

	//Some games put denuvo in their dynamic link libraries
	//So we check all of them
	for (const auto& file : getOpenFiles())
	{
		const auto executable = IExecutableFile::create(file, ELogLevel::k_ELogLevelDebug);
		if (!executable)
		{
			continue;
		}

		if (!steamDRM)
		{
			steamDRM |= executable->hasSteamDRM();
		}

		if (!denuvo)
		{
			denuvo |= executable->hasDenuvo();
		}
	}

	if (steamDRM)
	{
		LOG_DEBUG("Detected SteamDRM in %s!\n", exe.filename().c_str());
	}

	if (denuvo)
	{
		LOG_DEBUG("Detected Denuvo in %s!\n", exe.filename().c_str());
	}

	const auto endTime = std::chrono::system_clock::now();
	LOG_DEBUG("Analysed %s in %llums\n", exe.filename().c_str(), std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count());
	return true;
}

bool Process_t::init(const pid_t pid, const HSteamPipe pipeHandle)
{
	this->pid = pid;
	this->pipeHandle = pipeHandle;

	const auto serverPipe = g_pSteamEngine->getServerPipe(pipeHandle);
	if (!serverPipe)
	{
		LOG_ERROR("ServerPipe for %p is null!\n", reinterpret_cast<void*>(pipeHandle));
		return false;
	}

	exe = getRealExe();
	if (!exe.string().size())
	{
		return false;
	}

	cmdLine = Utils::strsplit(const_cast<char*>(readFile("cmdline").c_str()), "\0");
	environ = readFile("environ");

	if (!environ.size())
	{
		return false;
	}

	appId = getAppIdFromEnv();
	if (!appId) //Will fail on steam process
	{
		return false;
	}

	if (!g_config.smartTickets.copy())
	{
		return true;
	}

	analyse();

	return true;
}

std::unordered_map<HSteamPipe, Process_t> g_processMap = std::unordered_map<HSteamPipe, Process_t>();
