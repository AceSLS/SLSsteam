#pragma once

#include <string>
#include <vector>


namespace Curl
{
	constexpr int DEFAULT_TIMEOUT = 15;

	struct Request_t
	{
		const char*& url;
		const std::vector<std::string>& headers;
		std::string& out;
		const int& timeOut;
		int& result;
	};

	int downloadString(const char* url, std::string& out, const int timeOut = DEFAULT_TIMEOUT, const bool async = true);
	int downloadString(const char* url, const std::vector<std::string>& headers, std::string& out, const int timeOut = DEFAULT_TIMEOUT, const bool async = true);
}
