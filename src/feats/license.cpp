#include "license.hpp"

#include "../sdk/CProtoBufMsgBase.hpp"
#include "../sdk/CPackageInfoCache.hpp"
#include "../sdk/ELicenseFlags.hpp"
#include "../sdk/ELicenseType.hpp"

#include "../config.hpp"
#include "../globals.hpp"

#include <algorithm>


void License::injectDepots(CPackageInfo* pkg)
{
	if (!pkg)
	{
		return;
	}

	//We wanna modify package 0
	if (pkg->packageId != 0)
	{
		return;
	}

	//Package not cached
	if (!pkg->depotIds.size)
	{
		return;
	}

	auto depots = g_config.addedDepotIds.get();

	for(unsigned int i = 0; i < pkg->depotIds.size; i++)
	{
		const uint32_t depot = *pkg->depotIds.at(i);
		if (depots.contains(depot))
		{
			depots.erase(std::find(depots.begin(), depots.end(), depot));
		}
	}

	if (!depots.size())
	{
		return;
	}

	const auto newSize = pkg->depotIds.size + depots.size();

	uint32_t* newDepots = reinterpret_cast<uint32_t*>(malloc(newSize * sizeof(uint32_t)));
	memcpy(newDepots, pkg->depotIds.memory.base, pkg->depotIds.size * sizeof(uint32_t));

	unsigned int i = 0;
	for(const auto& depot : depots)
	{
		newDepots[i + pkg->depotIds.size] = depot;
		i++;
	}

	uint32_t* oldDepots = pkg->depotIds.memory.base;
	pkg->depotIds.size = newSize;
	pkg->depotIds.memory.base = newDepots;
	free(oldDepots);

	g_pLog->info("Pkg: %u -> %u\n", pkg->packageId, pkg->appIds.size);

	g_pLog->debug("AppIds:\n");
	for(unsigned int i = 0; i < pkg->appIds.size; i++)
	{
		auto app = *pkg->appIds.at(i);
		g_pLog->debug("%u\n", app);
	}
	g_pLog->debug("DepotIds:\n");
	for(unsigned int i = 0; i < pkg->depotIds.size; i++)
	{
		auto depot = *pkg->depotIds.at(i);
		g_pLog->debug("%u\n", depot);
	}
}


void License::recvMsg(CProtoBufMsgBase* msg)
{
	switch(msg->type)
	{
		case 780:
		{
			const auto body = msg->getBody<CMsgClientLicenseList>();
			for(const auto& package : g_config.addedPackageIds.get())
			{
				const auto lic = body->add_licenses();
				lic->set_package_id(package);
			}

			//Merge all licenses into one :)
			for(signed int i = 0; i < body->licenses_size(); i++)
			{
				auto lic = body->mutable_licenses(i);

				g_pLog->once("License for %u with flags %u of type %u\n", lic->package_id(), lic->flags(), lic->license_type());

				lic->set_owner_id(g_currentSteamId);
				lic->set_flags(ELICENSE_FLAGS_NONE);
				lic->set_license_type(ELICENSE_TYPE_SINGLE_PURCHASE);
			}

			break;
		}
	}
}
