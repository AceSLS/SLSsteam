#pragma once

#include "steam.hpp"

#include <cstdint>


class CMsgClientLicenseList;

enum class ECallbackType : uint32_t
{
	LicensesUpdate_t = 0x7d,
	AppOwnershipTicketReceived_t = 0xf907c,
	AppLicensesChanged_t = 0xf90be
};

enum EReleaseState
{
	k_EAppReleaseStateUnknown = 0x0,
	k_EAppReleaseStateUnavailable = 0x1,
	k_EAppReleaseStatePrerelease = 0x2,
	k_EAppReleaseStatePreloadonly = 0x3,
	k_EAppReleaseStateReleased = 0x4,
	k_EAppReleaseStateDisabled = 0x5
};

struct AppOwnershipTicketReceived_t
{
	EResult result;
	AppId_t appId;
};

struct AppLicensesChanged_t
{
	static constexpr unsigned int MAX_APPS_PER_CALLBACK = 0x40;

	bool reloadAll;							//0x0
	bool firstLoad;							//0x1
	uint8_t __pad_0x2[0x2];					//0x2
	uint32_t remainingPackets;				//0x4
	uint32_t count;							//0x8
	AppId_t apps[MAX_APPS_PER_CALLBACK];	//0xC
	uint64_t appsAdded;						//0x10C
}; //0x114

struct AppOwnershipInfo_t {
    int32_t subId;
    int32_t releaseState;
    uint32_t owner;
    int32_t masterSubscriptionAppId;
    uint32_t trialTime;
    uint32_t numLicenses;
    char region[2];
    char field7_0x1A[2];
    uint32_t purchaseTime;
    uint32_t realOwner;
    bool ownsLicense;
    bool licenseExpired;
    bool field12_0x26;
    bool lowViolence;
    bool freeLicense;
    bool regionRestricted;
    bool fromFreeWeekend;
    bool licenseLocked;
    bool licensePending;
    bool retailLicense;
    bool autoGrant;
    bool licensePermanent;
    bool field21_0x30;
    bool field22_0x31;
    bool siteLicense;
    bool field24_0x33;
    bool field25_0x34;
    bool familyShared;
    bool field27_0x36;
    bool field28_0x37;
}; //0x38

struct LicensesUpdated_t
{
	uint8_t __pad0x0;
};

class CUser
{
public:
	bool checkAppOwnership(const AppId_t appId, AppOwnershipInfo_t* pInfo);
	bool isSubscribed(const AppId_t appId);

	bool processLicenseList(const CMsgClientLicenseList* licenses);
	void postCallback(const ECallbackType type, void* pCallback, const uint32_t callbackSize);
	void updateAppOwnershipTicket(const AppId_t appId, void* pTicket, const uint32_t len);
};
