#pragma once

#include <cstdint>
#include <string>
#include <vector>


namespace Utils
{
	double calculateEntropy(const std::vector<uint8_t>& bytes);
	int exec(const std::string& executable, const std::vector<std::string>& args, std::string* stdOut);
	int exec(const std::vector<std::string>& executableList, const std::vector<std::string>& args, std::string* stdOut);
	bool isNumber(const char* str);
	std::string getFileSHA256(const char* filePath);
	std::vector<std::string> strsplit(char* str, const char* delimeter);

	template<typename T>
	bool tryConvertToNumber(const char* str, T& out)
	{
		if (!isNumber(str))
		{
			return false;
		}

		if constexpr (std::is_same_v<T, int32_t>)
		{
			out = std::stoi(str);
		}

		else if constexpr (std::is_same_v<T, uint32_t>)
		{
			out = std::stoul(str);
		}

		else if constexpr (std::is_same_v<T, int64_t>)
		{
			out = std::stoll(str);
		}

		else if constexpr (std::is_same_v<T, uint64_t>)
		{
			out = std::stoull(str);
		}

		else
		{
			return false;
		}

		return true;
	}
}
