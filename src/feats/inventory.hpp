#pragma once

#include "../sdk/steam.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>


struct SteamItemDetails_t;

namespace Inventory
{
	extern std::unordered_map<AppId_t, std::vector<uint32_t>> itemList;

	void getItemDefinitionIds(uint32_t* pItemsArr, uint32_t count);
	void getResultItems(SteamItemDetails_t* pItems, uint32_t pItemsSize, uint32_t* pItemsCount);
};
