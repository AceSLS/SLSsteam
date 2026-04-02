#include "license.hpp"

#include "../sdk/CProtoBufMsgBase.hpp"
#include "../sdk/ELicenseFlags.hpp"
#include "../sdk/ELicenseType.hpp"

#include "../config.hpp"
#include "../globals.hpp"


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
