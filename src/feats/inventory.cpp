#include "inventory.hpp"

#include "fakeappid.hpp"

#include "../sdk/IClientInventory.hpp"

#include "../config.hpp"
#include "../log.hpp"


std::unordered_map<AppId_t, std::vector<uint32_t>> Inventory::itemList;

void Inventory::getItemDefinitionIds(uint32_t* pItemsArr, uint32_t count)
{
	const AppId_t appId = FakeAppIds::getRealAppIdForCurrentPipe();

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
	const AppId_t appId = FakeAppIds::getRealAppIdForCurrentPipe();

	auto items = std::vector<uint32_t>();
	if (itemList.contains(appId))
	{
		for(const auto& item :  itemList[appId])
		{
			items.emplace_back(item);
		}
	}

	auto confItems = g_config.inventoryItems.get();

	if(confItems.contains(appId))
	{
		for(const auto& itm : confItems[appId].items)
		{
			bool skip = false;

			for(const auto& itm2 : items)
			{
				if (itm.first == itm2)
				{
					skip = true;
					break;
				}
			}

			if (!skip)
			{
				items.emplace_back(itm.first);
				g_pLog->debug("Injecting item %u from config\n", itm.first);
			}
		}
	}

	*pItemsCount = items.size();

	if (!pItemsSize)
	{
		return;
	}

	//Don't trust pItemSize
	g_pLog->debug("items.size() = %u, pItemSize = %u\n", items.size(), pItemsSize);

	if (items.size() > pItemsSize)
	{

		g_pLog->debug("Aborting inventory injection! items.size() > pItemSize\n");
		return;
	}

	for(unsigned int i = 0; i < items.size(); i++)
	{
		SteamItemDetails_t& itm = pItems[i];
		itm.id = items[i];
		itm.handle = i + 1;
		itm.flags = 0;

		if (confItems.contains(appId))
		{
			const auto configMap = confItems.at(appId);
			if (configMap.items.contains(itm.id))
			{
				itm.quantity = configMap.items.at(itm.id);
			}
		}
		else
		{
			itm.quantity = 1;
		}

		g_pLog->debug("Added %u item %u\n", itm.quantity, items[i]);
	}
}
