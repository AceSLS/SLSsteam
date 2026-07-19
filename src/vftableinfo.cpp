#include "vftableinfo.hpp"

#include "config.hpp"
#include "decompiler.hpp"
#include "log.hpp"

#include <sstream>


VFTableInfo_t::VFTableInfo_t(const char* typeName, const char* functionName, const unsigned int index)
	:
	typeName(typeName), functionName(functionName), index(index)
{
	VFTIndexes::functions.emplace_back(this);
}

bool VFTableInfo_t::init()
{
	if (!Decompiler::vftables.contains(typeName))
	{
		g_pLog->debug("%s not found in Decompiler::vftables!\n", typeName.c_str());
		return false;
	}

	if (index == NO_INDEX)
	{
		if (!VFTIndexes::tableMap.contains(typeName))
		{
			VFTIndexes::tableMap[typeName] = Decompiler::parseInterfaceMapBase(typeName.c_str());
		}

		const auto& tbl = VFTIndexes::tableMap.at(typeName);
		if (!tbl.contains(functionName))
		{
			g_pLog->debug("VFunction %s not found!\n", functionName.c_str());
			return false;
		}

		index = tbl.at(functionName);
	}

	auto& vft = Decompiler::vftables[typeName];
	auto& funcs = vft.functions;
	vft.analzye();

	if (index >= funcs.size())
	{
		g_pLog->debug("%s index bigger than vtable size!\n", getPrintName().c_str());
		return false;
	}

	address = funcs.at(index);
	g_pLog->debug("%s at index %u, address %p\n", getPrintName().c_str(), index, address);
	return true;
}

std::string VFTableInfo_t::getPrintName() const
{
	return typeName + "::" + functionName;
}


namespace VFTIndexes
{
	namespace CCMInterface
	{
		VFTableInfo_t RecvPkt
		{
			"12CCMInterface",
			"RecvPkt",
			3
		};
	};

	namespace CClientUnifiedServiceTransport
	{
		VFTableInfo_t SendAndRecvMsg
		{
			"30CClientUnifiedServiceTransport",
			"SendAndRecv",
			5
		};
	}

	namespace CSteamMatchmakingServers
	{
		VFTableInfo_t GetServerDetails
		{
			"24CSteamMatchMakingServers",
			"GetServerDetails",
			7
		};
		VFTableInfo_t RequestInternetServerList
		{
			"24CSteamMatchMakingServers",
			"RequestInternetServerList",
			0
		};
	}

	namespace IClientApps
	{
		VFTableInfo_t GetAppData
		{
			"14IClientAppsMap",
			"GetAppData"
		};
		VFTableInfo_t GetAppDataSection
		{
			"14IClientAppsMap",
			"GetAppDataSection"
		};
		VFTableInfo_t GetAppType
		{
			"14IClientAppsMap",
			"GetAppType"
		};
		VFTableInfo_t GetDLCCount
		{
			"14IClientAppsMap",
			"GetDLCCount"
		};
		VFTableInfo_t GetDLCDataByIndex
		{
			"14IClientAppsMap",
			"BGetDLCDataByIndex"
		};
		VFTableInfo_t RequestAppInfoUpdate
		{
			"14IClientAppsMap",
			"RequestAppInfoUpdate"
		};
	}

	namespace IClientAppManager
	{
		VFTableInfo_t BCanRemotePlayTogether
		{
			"20IClientAppManagerMap",
			"BCanRemotePlayTogether"
		};
		VFTableInfo_t BIsDlcEnabled
		{
			"20IClientAppManagerMap",
			"BIsDlcEnabled"
		};
		VFTableInfo_t GetAppInstallState
		{
			"20IClientAppManagerMap",
			"GetAppInstallState"
		};
		VFTableInfo_t GetUpdateInfo
		{
			"20IClientAppManagerMap",
			"GetUpdateInfo"
		};
		VFTableInfo_t InstallApp
		{
			"20IClientAppManagerMap",
			"InstallApp"
		};
		VFTableInfo_t IsAppDlcInstalled
		{
			"20IClientAppManagerMap",
			"IsAppDlcInstalled"
		};
		VFTableInfo_t LaunchApp
		{
			"20IClientAppManagerMap",
			"LaunchApp"
		};
		VFTableInfo_t UninstallApp
		{
			"20IClientAppManagerMap",
			"UninstallApp"
		};
	}

	//namespace IClientEngine
	//{
	//	VFTableInfo_t GetClientUser
	//	{
	//		//"13IClientEngine",
	//		"12CSteamClient",
	//		"GetClientUser",
	//		7
	//	};
	//}

	namespace IClientInventory
	{
		VFTableInfo_t GetResultItems
		{
			"19IClientInventoryMap",
			"GetResultItems"
		};
		VFTableInfo_t GetItemDefinitionIDs
		{
			"19IClientInventoryMap",
			"GetItemDefinitionIDs"
		};
	}

	namespace IClientRemoteStorage
	{
		VFTableInfo_t IsCloudEnabledForApp
		{
			"23IClientRemoteStorageMap",
			"IsCloudEnabledForApp"
		};
	}

	namespace IClientUtils
	{
		VFTableInfo_t GetAppId
		{
			"15IClientUtilsMap",
			"GetAppID"
		};
		VFTableInfo_t GetOfflineMode
		{
			"15IClientUtilsMap",
			"GetOfflineMode"
		};
	}

	namespace IClientUser
	{
		VFTableInfo_t BLoggedOn
		{
			"14IClientUserMap",
			"BLoggedOn"
		};
		VFTableInfo_t BUpdateAppOwnershipTicket
		{
			"14IClientUserMap",
			"BUpdateAppOwnershipTicket"
		};
		VFTableInfo_t GetAppOwnershipTicketExtendedData
		{
			"14IClientUserMap",
			"GetAppOwnershipTicketExtendedData"
		};
		VFTableInfo_t GetSteamID
		{
			"14IClientUserMap",
			"GetSteamID"
		};
		VFTableInfo_t IsUserSubscribedAppInTicket
		{
			"14IClientUserMap",
			"IsUserSubscribedAppInTicket"
		};
		VFTableInfo_t RequiresLegacyCDKey
		{
			"14IClientUserMap",
			"RequiresLegacyCDKey"
		};
	}

	std::vector<VFTableInfo_t*> functions;
	std::unordered_map<std::string, std::map<std::string, unsigned int>> tableMap;
}

void VFTIndexes::dump(const std::string& name, const std::map<std::string, unsigned int>& functionMap)
{
	std::ostringstream ss;

	ss << "\n" << name << "\n{";

	for(const auto& kv : functionMap)
	{
		ss << "\n\t" << kv.first << " = " << kv.second;
	}

	ss << "\n};";

	g_pLog->info("Dump %s\n", ss.str().c_str());
}

bool VFTIndexes::init()
{
	for(const auto& fn : functions)
	{
		if (!fn->init())
		{
			return false;
		}
	}

	if (g_config.dumpInterfaceMaps.get())
	{
		for(const auto& tbl : tableMap)
		{
			dump(tbl.first, tbl.second);
		}
	}

	return true;
}
