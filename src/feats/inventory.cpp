#include "inventory.hpp"

#include "fakeappid.hpp"

#include "../sdk/IClientInventory.hpp"

#include "../log.hpp"


std::unordered_map<uint32_t, std::vector<uint32_t>> Inventory::itemList;

void Inventory::getItemDefinitionIds(uint32_t* pItemsArr, uint32_t count)
{
	const uint32_t appId = FakeAppIds::getRealAppIdForCurrentPipe();

	itemList[appId] = std::vector<uint32_t>();
	for(unsigned int i = 0; i < count; i++)
	{
		uint32_t id = pItemsArr[i];
		itemList[appId].emplace_back(id);

		g_pLog->debug("Added %u to %u\n", id, appId);
	}
}

void Inventory::getResultItems(SteamItemDetails_t* pItems, uint32_t pItemsSize, uint32_t* pItemsCount)
{
	const uint32_t appId = FakeAppIds::getRealAppIdForCurrentPipe();

	if (!itemList.contains(appId))
	{
		return;
	}

	auto items = itemList[appId];
	*pItemsCount = items.size();

	if (!pItemsSize)
	{
		return;
	}

	for(unsigned int i = 0; i < pItemsSize; i++)
	{
		SteamItemDetails_t& itm = pItems[i];
		itm.id = items[i];
		itm.quantity = 1;
		itm.handle = 0;
		itm.flags = 0;

		g_pLog->debug("Added item %u\n", items[i]);
	}
}
