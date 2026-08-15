#pragma once

#include "libmem/libmem.h"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>


struct VFTableInfo_t
{
	constexpr static unsigned int NO_INDEX = 0xFFFFFFFF;

	std::string typeName;
	std::string functionName;
	lm_address_t address;
	unsigned int index;

	VFTableInfo_t(const char* typeName, const char* functionName, const unsigned int index = NO_INDEX);
	bool init();

	std::string getPrintName() const;
};

namespace VFTIndexes
{
	namespace CCMInterface
	{
		extern VFTableInfo_t RecvPkt;
	}

	namespace CRemoteClientManager
	{
		extern VFTableInfo_t RecvPkt;
	}

	namespace CClientUnifiedServiceTransport
	{
		extern VFTableInfo_t SendAndRecvMsg;
	}

	namespace CGameInfoDialog
	{
		extern VFTableInfo_t ServerResponded;
	}

	namespace CSteamMatchmakingServers
	{
		extern VFTableInfo_t GetServerDetails;
		extern VFTableInfo_t RequestInternetServerList;
	}

	namespace IClientApps
	{
		extern VFTableInfo_t GetAppData;
		extern VFTableInfo_t GetAppDataSection;
		extern VFTableInfo_t GetAppType;
		extern VFTableInfo_t GetDLCCount;
		extern VFTableInfo_t GetDLCDataByIndex;
		extern VFTableInfo_t RequestAppInfoUpdate;
	}

	namespace IClientAppManager
	{
		extern VFTableInfo_t BCanRemotePlayTogether;
		extern VFTableInfo_t BIsDlcEnabled;
		extern VFTableInfo_t GetAppInstallState;
		extern VFTableInfo_t GetLibraryFolderLabel;
		extern VFTableInfo_t GetLibraryFolderPath;
		extern VFTableInfo_t GetNumLibraryFolders;
		extern VFTableInfo_t GetUpdateInfo;
		extern VFTableInfo_t InstallApp;
		extern VFTableInfo_t IsAppDlcInstalled;
		extern VFTableInfo_t LaunchApp;
		extern VFTableInfo_t UninstallApp;
	}

	namespace IClientCompat
	{
		extern VFTableInfo_t BIsCompatLayerEnabled;
		extern VFTableInfo_t BIsCompatibilityToolEnabled;
		extern VFTableInfo_t GetAvailableCompatToolsForApp;
		extern VFTableInfo_t GetCompatToolDisplayName;
		extern VFTableInfo_t GetCompatToolName;
		extern VFTableInfo_t SpecifyCompatTool;
	}

	namespace IClientConfigStore
	{
		extern VFTableInfo_t SetString;
	}

	namespace IClientFriends
	{
		extern VFTableInfo_t GetFriendGamePlayed;
	}

	namespace IClientEngine
	{
		extern VFTableInfo_t GetClientUser;
	}

	namespace IClientRemoteStorage
	{
		extern VFTableInfo_t IsCloudEnabledForApp;
	}

	namespace IClientUtils
	{
		extern VFTableInfo_t GetAppId;
		extern VFTableInfo_t GetOfflineMode;
	}

	namespace IClientUser
	{
		extern VFTableInfo_t BLoggedOn;
		extern VFTableInfo_t BUpdateAppOwnershipTicket;
		extern VFTableInfo_t GetAppOwnershipTicketExtendedData;
		extern VFTableInfo_t GetEncryptedAppTicket;
		extern VFTableInfo_t GetLegacyCDKey;
		extern VFTableInfo_t GetSteamID;
		extern VFTableInfo_t IsUserSubscribedAppInTicket;
		extern VFTableInfo_t SetLegacyCDKey;
	}

	extern std::vector<VFTableInfo_t*> functions;
	extern std::unordered_map<std::string, std::map<std::string, unsigned int>> tableMap;

	void dump(const std::string& name, const std::map<std::string, unsigned int>& functionMap);
	bool init();
}
