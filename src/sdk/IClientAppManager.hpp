#pragma once

#include "types.hpp"

#include <cstdint>


enum EAppState : uint32_t
{
	k_EAppStateInvalid = 0x0,
	k_EAppStateUninstalled = 0x1,
	k_EAppStateUpdateRequired = 0x2,
	k_EAppStateFullyInstalled = 0x4,
	k_EAppStateUpdateQueued = 0x8,
	k_EAppStateUpdateOptional = 0x10,
	k_EAppStateFilesMissing = 0x20,
	k_EAppStateSharedOnly = 0x40,
	k_EAppStateFilesCorrupt = 0x80,
	k_EAppStateUpdateRunning = 0x100,
	k_EAppStateUpdatePaused = 0x200,
	k_EAppStateUpdateStarted = 0x400,
	k_EAppStateUninstalling = 0x800,
	k_EAppStateBackupRunning = 0x1000,
	k_EAppStateAppRunning = 0x2000,
	k_EAppStateComponentInUse = 0x4000,
	k_EAppStateMovingFolder = 0x8000,
	k_EAppStateTerminating = 0x10000,
	k_EAppStatePrefetchingInfo = 0x20000,
	k_EAppStatePeerServer = 0x40000,
	k_EAppStateUpdatedDisabledbyapp = 0x80000
};

SDK_Struct DepotInfo_t
{
	AppId_t depotId;			//0x0
	AppId_t appId;				//0x4
	uint64_t manifestId;		//0x8
	uint64_t size;				//0x10
	uint8_t __pad0x18[0x4];		//0x18
	bool lcsRequired;			//0x1C
	uint8_t __pad0x1D[0x3];		//0x1D
}; //0x20

SDK_Class IClientAppManager
{
public:
	EAppState getAppInstallState(const AppId_t appId);
	uint32_t getLibraryFolderLabel(const uint32_t index, char* pChLabel, const uint32_t labelSize);
	uint32_t getLibraryFolderPath(const uint32_t index, char* pChPath, const steamsize_t pathSize);
	uint32_t getNumLibraryFolders();
	bool installApp(const AppId_t appId, const uint32_t libraryIndex);
	uint32_t uninstallApp(const AppId_t appId);
};
