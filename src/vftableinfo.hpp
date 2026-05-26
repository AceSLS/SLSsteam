#pragma once

namespace VFTIndexes
{
	namespace IClientEngine
	{
		constexpr int GetClientUser = 7;
	}

	namespace IClientApps
	{
		constexpr int GetAppData = 0;
		constexpr int GetAppDataSection = 5;
		constexpr int RequestAppInfoUpdate = 7;
		constexpr int GetDLCCount = 8;
		constexpr int GetDLCDataByIndex = 9;
		constexpr int GetAppType = 10;
	}

	namespace IClientAppManager
	{
		constexpr int InstallApp = 0;
		constexpr int UninstallApp = 1;
		constexpr int LaunchApp = 2;
		constexpr int GetAppInstallState = 4;
		constexpr int IsAppDlcInstalled = 9;
		constexpr int BIsDlcEnabled = 11;
		constexpr int GetUpdateInfo = 20;
		constexpr int GetAppOwnershipInfo = 71;
	}

	namespace IClientRemoteStorage
	{
		constexpr int IsCloudEnabledForApp = 24;
	}

	namespace IClientUser
	{
		constexpr int BLoggedOn = 4;
		constexpr int GetSteamID = 10;
	}

	namespace IClientUtils
	{
		constexpr int GetOfflineMode = 17;
		constexpr int GetAppId = 19;
	}
}
