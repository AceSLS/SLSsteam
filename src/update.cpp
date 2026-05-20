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

bool Updater::init()
{
	std::string data;
	int res = Curl::getString("https://raw.githubusercontent.com/AceSLS/SLSsteam/refs/heads/main/res/updates.yaml", data);
	g_pLog->info("Curl Res: %u\n", res);

	if(res != 0)
	{
		data = loadFromCache();
		if(data.size() < 1)
		{
			return false;
		}

		g_pLog->info("Using cached updates.yaml\n");
	}

	g_pLog->debug("updates.yaml:\n%s\n", data.c_str());

	try
	{
		YAML::Node node = YAML::Load(data);
		for (const auto& sub : node["SafeModeHashes"])
		{
			uint64_t version = sub.first.as<uint64_t>();
			clientHashMap[version] = std::unordered_set<std::string>();

			g_pLog->debug("Parsing version %llu\n", version);

			for(const auto& hash : sub.second)
			{
				auto str = hash.as<std::string>();
				clientHashMap[version].emplace(str);

				g_pLog->debug("Added %s to SLSsteam version %llu\n", str.c_str(), version);
			}
		}
	}
	catch(...)
	{
		g_pLog->info("Failed to parse updates!\n");
		return false;
	}

	saveToCache(data);
	return true;
}

std::string Updater::getCacheFilePath()
{
	auto path = g_config.getDir().append("/.updates.yaml");
	return path;
}

void Updater::saveToCache(std::string yaml)
{
	auto path = Updater::getCacheFilePath();

	std::ofstream stream = std::ofstream(path.c_str());
	stream << yaml;
	stream.close();

	g_pLog->debug("Cached res/updates.yaml!\n");
}

std::string Updater::loadFromCache()
{
	auto path = Updater::getCacheFilePath();
	if (!std::filesystem::exists(path))
	{
		return std::string();
	}

	g_pLog->debug("Loading updates.ymal from disk!\n");

	std::ifstream fstream = std::ifstream(path.c_str());
	std::stringstream buf;
	buf << fstream.rdbuf();

	fstream.close();
	return buf.str();
}

bool Updater::verifySafeModeHash()
{
	auto path = std::filesystem::path(g_modSteamClient.path);

	try
	{
		std::string sha256 = Utils::getFileSHA256(path.c_str());
		g_pLog->info("steamclient.so hash is %s\n", sha256.c_str());

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
		g_pLog->debug("Unable to read steamclient.so hash!\n");
		return false;
	}

	return true;
}
