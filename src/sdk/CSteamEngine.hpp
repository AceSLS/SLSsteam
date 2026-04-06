#pragma once

#include "steam.hpp"

#include <cstdint>


class CPackageInfoCache;
class CUser;

class CSteamEngine
{
public:
	CUser* getUser(const uint32_t index);
	void setAppIdForCurrentPipe(const AppId_t appId);
};

extern CSteamEngine* g_pSteamEngine;
