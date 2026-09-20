#include "utils.hpp"

#include "log.hpp"

#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include <openssl/sha.h>


double Utils::calculateEntropy(const std::vector<uint8_t>& bytes)
{
	auto countMap = std::unordered_map<uint8_t, size_t>();
	for (const auto& byte : bytes)
	{
		countMap[byte]++;
	}

	double val = 0.0;
	for (const auto& count : countMap)
	{
		double freq = static_cast<double>(count.second) / bytes.size();
		val += freq * log2(freq);
	}

	return -val;
}

int Utils::exec(const std::string& executable, const std::vector<std::string>& args, std::string* stdOut)
{
	return exec(std::vector<std::string> { executable }, args, stdOut);
}

int Utils::exec(const std::vector<std::string>& executableList, const std::vector<std::string>& args, std::string* stdOut)
{
	int pipefd[2];

	if (pipe(pipefd) == -1)
	{
		LOG_ERROR("Failed to create pipe!\n");
		return 1;
	}

	LOG_DEBUG("Created pipe %i : %i\n", pipefd[0], pipefd[1]);

	constexpr static const char* env[] =
	{
		"PATH='/usr/bin:/bin'",
		nullptr
	};

	std::vector<const char*> ppChArgs;
	const auto exeName = std::filesystem::path(executableList.at(0)).filename();

	//First argv should be name of executable
	ppChArgs.emplace_back(exeName.c_str());

	for (const auto& arg : args)
	{
		ppChArgs.emplace_back(arg.c_str());
	}
	ppChArgs.emplace_back(nullptr);

	const pid_t pid = fork();
	if (pid == -1)
	{
		close(pipefd[0]);
		close(pipefd[1]);

		LOG_ERROR("Failed to fork!\n");
		return 1;
	}

	if (pid == 0)
	{
		if (dup2(pipefd[1], STDOUT_FILENO) == -1)
		{
			LOG_ERROR("Failed to dup2!\n");
			exit(1);
		}

		//No need for reading
		close(pipefd[0]);
		close(pipefd[1]);

		for (const auto& exePath : executableList)
		{
			execve(exePath.c_str(), const_cast<char**>(ppChArgs.data()), const_cast<char**>(env));
			LOG_DEBUG("Failed to execv %s!\n", exePath.c_str());
		}

		exit(1);
	}

	//No need for writing
	close(pipefd[1]);

	LOG_DEBUG("Child PID %i\n", pid);

	std::ostringstream bufSS;
	char buf[8192];
	int numRead;

	while((numRead = read(pipefd[0], buf, sizeof(buf))) > 0)
	{
		bufSS << std::string(buf, numRead);
	}

	close(pipefd[0]);

	int status;
	if (waitpid(pid, &status, 0) == -1)
	{
		return 1;
	}

	if (!WIFEXITED(status))
	{
		return 1;
	}

	status = WEXITSTATUS(status);

	LOG_DEBUG("Exit Status: %i\n", status);

	if (stdOut)
	{
		*stdOut = bufSS.str();
	}

	return status;
}

bool Utils::isNumber(const char* str)
{
	const unsigned int len = strlen(str);
	if (len < 1)
	{
		return false;
	}

	for (unsigned int i = 0; i < len; i++)
	{
		const char c = str[i];

		if (!std::isdigit(c))
		{
			return false;
		}
	}

	return true;
}

std::string Utils::getFileSHA256(const char *filePath)
{
	std::ifstream fs(filePath, std::ios::binary);
	if (!fs.is_open())
	{
		//TODO: Read more about error types in C++ :)
		throw std::runtime_error("Unable to read file!");
	}

	const auto bytes = std::vector<unsigned char>(std::istreambuf_iterator(fs), {});
	unsigned char sha256Bytes[SHA256_DIGEST_LENGTH];
	SHA256(bytes.data(), bytes.size(), sha256Bytes);

	std::ostringstream sha256;
	for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
	{
		sha256 << std::hex << std::setw(2) << std::setfill('0') << (int)sha256Bytes[i];
	}

	fs.close();
	return sha256.str();
}

std::vector<std::string> Utils::strsplit(const std::string& str, const char *delimeter)
{
	auto splits = std::vector<std::string>();

	std::regex delimeterRe(delimeter);
	auto it = std::sregex_token_iterator(str.begin(), str.end(), delimeterRe, -1);
	std::sregex_token_iterator end;

	while (it != end)
	{
		splits.emplace_back(*it);
		it++;
	}

	return splits;
}
