#include "license.hpp"

#include "../sdk/CProtoBufMsgBase.hpp"
#include "../sdk/CPackageInfoCache.hpp"
#include "../sdk/CSteamEngine.hpp"
#include "../sdk/CUser.hpp"
#include "../sdk/ELicenseFlags.hpp"
#include "../sdk/ELicenseType.hpp"

#include "../config.hpp"
#include "../globals.hpp"

#include <vector>


void License::getPackageInfo(CPackageInfo* pkg)
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

	if (!g_config.reloadDepots.get())
	{
		return;
	}

	static auto mutex = std::mutex();
	const std::lock_guard lock(mutex);

	static auto originalDepotIds = std::vector<uint32_t>();
	if (!originalDepotIds.size())
	{
		for(unsigned int i = 0; i < pkg->depotIds.size; i++)
		{
			originalDepotIds.emplace_back(*pkg->depotIds.at(i));
		}
	}

	const auto depots = g_config.addedDepotIds.get();
	const auto newSize = originalDepotIds.size() + depots.size();

	uint32_t* newDepots = reinterpret_cast<uint32_t*>(malloc(newSize * sizeof(uint32_t)));
	memcpy(newDepots, originalDepotIds.data(), originalDepotIds.size() * sizeof(uint32_t));

	unsigned int i = 0;
	for(const auto& depot : depots)
	{
		newDepots[i + originalDepotIds.size()] = depot;
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

	g_config.reloadDepots = false;
}

//TODO: Fix crash on dealloc (when Steam exits)
static CMsgClientLicenseList* msgLicenses = nullptr;

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

			msgLicenses = new CMsgClientLicenseList(*body);
			break;
		}
	}
}

void License::runIPCFrame()
{
	std::lock_guard packagesChanged(g_config.packagesChangedMutex);

	if (!g_config.newPackages.size() && !g_config.removedPackages.size())
	{
		return;
	}

	if (!msgLicenses)
	{
		return;
	}

	const auto user = g_pSteamEngine->getUser(0);
	if (!user)
	{
		return;
	}

	const auto licenses = msgLicenses->mutable_licenses();

	for(const auto& pkg : g_config.newPackages)
	{
		auto lic = licenses->Add();
		lic->set_package_id(pkg);
		lic->set_owner_id(g_currentSteamId);
		lic->set_flags(ELICENSE_FLAGS_NONE);
		lic->set_license_type(ELICENSE_TYPE_SINGLE_PURCHASE);
	}

	for(const auto& pkg : g_config.removedPackages)
	{
		for(int i = licenses->size() - 1; i >= 0; i--)
		{
			auto lic = licenses->at(i);
			if (lic.package_id() == pkg && lic.owner_id() == g_currentSteamId)
			{
				licenses->DeleteSubrange(i, 1);
			}
		}
	}

	g_config.newPackages.clear();
	g_config.removedPackages.clear();

	user->processLicenseList(msgLicenses);

	LicensesUpdated_t cb {};
	user->postCallback(ECallbackType::LicensesUpdate_t, &cb, sizeof(cb));
}
