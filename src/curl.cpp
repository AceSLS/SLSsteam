#include "curl.hpp"

#include "log.hpp"
#include "utils.hpp"

#include <dlfcn.h>
#include <vector>

#include <curl/curl.h>
#include <curl/easy.h>

static CURL* curl = nullptr;

static size_t writeCallback(const char* content, size_t size, size_t memberSize, std::string* data)
{
	data->append(content, size * memberSize);
	return size * memberSize;
}

int Curl::downloadString(const char* url, std::string& out, const int timeOut)
{
	return downloadString(url, { }, out, timeOut);
}

int Curl::downloadString(const char* url, const std::vector<std::string>& headers, std::string& out, const int timeOut)
{
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeOut);

	for (const auto& hdr : headers)
	{
		curl_easy_setopt(curl, CURLOPT_HEADER, hdr.c_str());
	}

	auto res = curl_easy_perform(curl);

	curl_easy_cleanup(curl);

	return res;
}
