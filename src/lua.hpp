#pragma once

#include "filewatcher.hpp"
#include "log.hpp"

#include "libmem/libmem.h"

#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <luajit-2.1/lua.hpp>
#include "LuaBridge/LuaBridge.h"


extern "C" void* place_lua_hook(const int index, const void* pTarget);
extern "C" void* unpack_user_data(const void* pData);

namespace Lua
{
	typedef uint64_t Address_t;
	typedef void* Ptr_t;

	namespace Callbacks
	{
		constexpr const char* SLSsteam_ConfigLoaded = "SLSsteam::configLoaded";
		constexpr const char* SLSsteam_ConfigLoading = "SLSsteam::configLoading";
		constexpr const char* SLSsteam_Initialized = "SLSsteam::initialized";
		constexpr const char* SLSsteam_LuaReload = "SLSsteam::luaReload";

		constexpr const char* Network_RecvPkt = "Network::recvPkt";
		constexpr const char* Network_SendPkt = "Network::sendPkt";
	}

	extern lua_State* state;
	extern std::recursive_mutex stateMutex;
	extern std::unique_ptr<CFileWatcher> watcher;
	extern std::unordered_map<std::string, std::vector<luabridge::LuaRef>> callbacks;

	void init(const bool fullReload = false);
	void initLuaState();

	bool fixPerms(const std::filesystem::path& path);
	void onFileChange(const std::filesystem::path& path, const int eventMask);
	bool runLua(const std::filesystem::path& path);

	template<typename ...Args>
	unsigned int fireCallback(const char* name, Args... args)
	{
		std::lock_guard guard(stateMutex);

		if (!state)
		{
			return 0;
		}

		if (!callbacks.contains(name))
		{
			return 0;
		}

		unsigned int calls = 0;
		const auto& functions = callbacks.at(name);
		for (const auto& fn : functions)
		{
			try
			{
				fn(args...);
				calls++;
			}
			catch (luabridge::LuaException& exc)
			{
				LOG_ERROR("Failed running lua callback %s\n%s\n", name, exc.what());
			}
		}

		LOG_DEBUG("Fired %u from %u %s lua callbacks\n", calls, functions.size(), name);
		return calls;
	}

	void registerCallback(const std::string& name, luabridge::LuaRef fn);
}
