#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>


struct SteamItemDetails_t;

namespace Inventory
{
	extern std::unordered_map<uint32_t, std::vector<uint32_t>> itemList;

	void getItemDefinitionIds(uint32_t* pItemsArr, uint32_t count);
	void getResultItems(SteamItemDetails_t* pItems, uint32_t pItemsSize, uint32_t* pItemsCount);
};
