#pragma once

#include "protobufs/enums.pb.h"

#include "types.hpp"

#include <cstdint>


using EAppType = EProtoAppType;

enum EAppInfoSection : uint32_t
{
	k_EAppInfoSectionUnknown = 0x0,
	k_EAppInfoSectionAll = 0x1,
	k_EAppInfoSectionCommon = 0x2,
	k_EAppInfoSectionExtended = 0x3,
	k_EAppInfoSectionConfig = 0x4,
	k_EAppInfoSectionStats = 0x5,
	k_EAppInfoSectionInstall = 0x6,
	k_EAppInfoSectionDepots = 0x7,
	k_EAppInfoSectionVAC = 0x8,
	k_EAppInfoSectionDRM = 0x9,
	k_EAppInfoSectionUFS = 0xa,
	k_EAppInfoSectionOGG = 0xb,
	k_EAppInfoSectionItems = 0xc,
	k_EAppInfoSectionPolicies = 0xd,
	k_EAppInfoSectionSysreqs = 0xe,
	k_EAppInfoSectionCommunity = 0xf,
	k_EAppInfoSectionStore = 0x10,
	k_EAppInfoSectionLocalization = 0x11,
	k_EAppInfoSectionBroadcastgamedata = 0x12,
	k_EAppInfoSectionComputed = 0x13,
	k_EAppInfoSectionAlbummetadata = 0x14
};

SDK_Struct AppStateInfo_t
{
    EAppReleaseState releaseStuint32_tate;			//0x0
    EAppOwnershipFlags ownershipFlags;	//0x4
    uint8_t __pad0x8[0x4];				//0x8
    uint32_t ownerAccountId;			//0xC
    uint8_t __pad0x10[0x4];				//0x10
    int32_t subscriptionTime;			//0x14
    uint8_t __pad0x18[0x4];				//0x18
    uint32_t realOwner;					//0x1C
    uint32_t subId;						//0x20
    uint8_t __pad0x20[4];				//0x24
};

SDK_Class IClientApps
{
public:
	int32_t getAppData(const AppId_t appId, const char* name, const char* pChOut, const steamsize_t outSize);
	uint32_t getAppDataSection(const AppId_t appId, const EAppInfoSection section, const char* pChOut, const steamsize_t outSize);
	bool requestAppInfoUpdate(const AppId_t* appIds, const steamsize_t numAppIds);
	EAppType getAppType(const AppId_t appId);
};
