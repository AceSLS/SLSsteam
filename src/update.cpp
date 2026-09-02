#include "update.hpp"

#include "config.hpp"
#include "curl.hpp"
#include "globals.hpp"
#include "log.hpp"
#include "utils.hpp"
#include "version.hpp"

#include <filesystem>
#include <fstream>
#include <map>
#include <map>
#include <string>


std::map<uint64_t, std::unordered_set<std::string>> Updater::clientHashMap = std::map<uint64_t, std::unordered_set<std::string>>();

constexpr static const char* urls[] =
{
	"https://raw.githubusercontent.com/AceSLS/SLSsteam/refs/heads/main/res/updates.yaml",
	"https://cdn.jsdelivr.net/gh/AceSLS/SLSsteam/res/updates.yaml"
};

bool Updater::init()
{
	if (!isEnabled())
	{
		return false;
	}

	std::string data;
	int res;

	bool downloadSuccess = false;

	for (const auto url : urls)
	{
		res = Curl::downloadString(url, data);
		LOG_INFO("Curl Res: %u for %s with len %i\n", res, url, data.size());

		if (res == 0 && data.starts_with("SafeModeHashes:\n")) //User reported empty responses
		{
			downloadSuccess = true;
			break;
		}

		LOG_WARN("Download updates.yaml failed!\n");
	}

	if (!downloadSuccess)
	{
		data = loadFromCache();
		if (data.size() < 1)
		{
			LOG_ERROR("No cached updates.yaml found! Failing\n");
			return false;
		}

		LOG_INFO("Using cached updates.yaml\n");
	}

	LOG_DEBUG("updates.yaml:\n%s\n", data.c_str());

	try
	{
		YAML::Node node = YAML::Load(data);
		for (const auto& sub : node["SafeModeHashes"])
		{
			const uint64_t version = sub.first.as<uint64_t>();
			clientHashMap[version] = std::unordered_set<std::string>();

			LOG_DEBUG("Parsing version %llu\n", version);

			for (const auto& hash : sub.second)
			{
				const auto str = hash.as<std::string>();
				clientHashMap[version].emplace(str);

				LOG_DEBUG("Added %s to SLSsteam version %llu\n", str.c_str(), version);
			}
		}
	}
	catch(...)
	{
		LOG_INFO("Failed to parse updates!\n");
		return false;
	}

	saveToCache(data);
	return true;
}

std::string Updater::getCacheFilePath()
{
	const auto path = g_config.getDir().append(".updates.yaml");
	return path;
}

void Updater::saveToCache(const std::string yaml)
{
	const auto path = Updater::getCacheFilePath();

	std::ofstream stream = std::ofstream(path.c_str());
	stream << yaml;
	stream.close();

	LOG_DEBUG("Cached res/updates.yaml!\n");
}

std::string Updater::loadFromCache()
{
	const auto path = Updater::getCacheFilePath();
	if (!std::filesystem::exists(path))
	{
		return std::string();
	}

	LOG_DEBUG("Loading updates.yaml from disk!\n");

	std::ifstream fstream = std::ifstream(path.c_str());
	std::ostringstream buf;
	buf << fstream.rdbuf();

	fstream.close();
	return buf.str();
}

bool Updater::verifySafeModeHash()
{
	//Don't waste time calculating SHA
	if (!isEnabled())
	{
		return false;
	}

	const auto path = std::filesystem::path(g_modSteamClient->path);

	try
	{
		const std::string sha256 = Utils::getFileSHA256(path.c_str());
		LOG_INFO("steamclient.so hash is %s\n", sha256.c_str());

		if (!clientHashMap.contains(VERSION))
		{
			return false;
		}

		const auto& safeHashes = clientHashMap[VERSION];
		if (safeHashes.contains(sha256))
		{
			return true;
		}

		return false;
	}
	catch(std::runtime_error& err)
	{
		LOG_ERROR("Unable to read steamclient.so hash!\n");
		return false;
	}

	return true;
}

bool Updater::isEnabled()
{
	return g_config.safeMode.copy() || g_config.warnHashMissmatch.copy();
}
