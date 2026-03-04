#include "license.hpp"

#include "../sdk/CProtoBufMsgBase.hpp"
#include "../sdk/EResult.hpp"

#include "../config.hpp"


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
			break;
		}

		case 5439:
		{
			const auto body = msg->getBody<CMsgClientGetDepotDecryptionKeyResponse>();
			if (body->eresult() == ERESULT_OK)
			{
				break;
			}

			const auto& keys = g_config.depotKeys.get();
			if (!keys.contains(body->depot_id()))
			{
				break;
			}

			auto cpy = CMsgClientGetDepotDecryptionKeyResponse(*body);

			cpy.set_eresult(ERESULT_OK);
			cpy.set_depot_encryption_key(keys.at(body->depot_id()));

			body->ParseFromString(cpy.SerializeAsString());
			break;
		}

	}
}
