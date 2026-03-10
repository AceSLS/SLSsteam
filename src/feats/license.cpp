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
	}
}
