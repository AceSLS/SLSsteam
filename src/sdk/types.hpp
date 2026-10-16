#pragma once

#include <cstdint>


#define SDK_Class class __attribute__((packed)) __attribute__((aligned(1)))
#define SDK_Struct struct __attribute__((packed)) __attribute__((aligned(1)))

typedef uint32_t AppId_t;
typedef uint64_t GameId_t;

typedef uint32_t PackageId_t;

typedef int32_t ENetPacket;

typedef uint32_t HSteamPipe;
typedef uint32_t HSteamUser;

typedef uint32_t steamsize_t;

constexpr uint64_t GAME_TYPE_SHORTCUT = 0x2000000ULL;

constexpr ENetPacket INVALID_NETPACKET_TYPE = -1;
constexpr ENetPacket PROTOBUF_TYPE_MASK = 0x80000000;

typedef uint32_t AppOwnershipFlags_t;

enum EAppOwnershipFlags : uint32_t
{
	k_EAppOwnershipFlagsNone = 0x0,
	k_EAppOwnershipFlagsSubscribed = 0x1,
	k_EAppOwnershipFlagsFree = 0x2,
	k_EAppOwnershipFlagsRegionrestricted = 0x4,
	k_EAppOwnershipFlagsLowViolence = 0x8,
	k_EAppOwnershipFlagsInvalidPlatform = 0x10,
	k_EAppOwnershipFlagsBorrowed = 0x20,
	k_EAppOwnershipFlagsFreeWeekend = 0x40,
	k_EAppOwnershipFlagsRetail = 0x80,
	k_EAppOwnershipFlagsLocked = 0x100,
	k_EAppOwnershipFlagsPending = 0x200,
	k_EAppOwnershipFlagsExpired = 0x400,
	k_EAppOwnershipFlagsPermanent = 0x800,
	k_EAppOwnershipFlagsRecurring = 0x1000,
	k_EAppOwnershipFlagsCanceled = 0x2000,
	k_EAppOwnershipFlagsAutoGrant = 0x4000,
	k_EAppOwnershipFlagsPendingGift = 0x8000,
	//k_EAppOwnershipFlagsUnknown = 0x10000,
	//k_EAppOwnershipFlagsUnknown = 0x20000,
	k_EAppOwnershipFlagsSiteLicense = 0x40000,
	k_EAppOwnershipFlagsLegacyFreeSub = 0x80000,
	k_EAppOwnershipFlagsInvalidOSType = 0x100000,
	k_EAppOwnershipFlagsTimedTrial = 0x200000
};

enum EConfigStore : uint32_t
{
	Invalid = 0,
	Install = 1,
	UserRoaming = 2,
	UserLocal = 3,
	MachineUser = 4,
	Max = 5
};

typedef uint32_t LicenseFlags_t;

enum ELicenseFlags : LicenseFlags_t
{
	k_ELicenseFlagsNone = 0x0,
	k_ELicenseFlagsRenewnextperiod = 0x1,
	k_ELicenseFlagsRenewfailed = 0x2,
	k_ELicenseFlagsPending = 0x4,
	k_ELicenseFlagsExpired = 0x8,
	k_ELicenseFlagsCancelledbyUser = 0x10,
	k_ELicenseFlagsCancelledbyAdmin = 0x20,
	k_ELicenseFlagsLowViolence = 0x40,
	k_ELicenseFlagsLicenseImportedFromSteam2 = 0x80,
	k_ELicenseFlagsForceRunRestriction = 0x100,
	k_ELicenseFlagsRegionRestrictionExpired = 0x200,
	k_ELicenseFlagsCancelledbyFriendlyFraudLock = 0x400,
	k_ELicenseFlagsNotactivated = 0x800,
	k_ELicenseFlagsLocked = 0x1000,
	k_ELicenseFlagsPendingRefund = 0x2000,
	k_ELicenseFlagsBorrowed = 0x4000,
	k_ELicenseFlagsReleaseStateOverride = 0x8000,
	k_ELicenseFlagsAutoGrant = 0x10000,
	k_ELicenseFlagsTimedTrial = 0x20000,
	k_ELicenseFlagsCancelledbyPartner = 0x40000,
	k_ELicenseFlagsNonPermanent = 0x80000,
	k_ELicenseFlagsPreferredOwner = 0x100000,
};

enum class ELicenseType : uint32_t
{
	None = 0x0,
	SinglePurchase = 0x1,
	SinglePurchaseLimitedUse = 0x2,
	RecurringCharge = 0x3,
	Recurringpaymentoption = 0x6,
	LimiteduseDelayedActivation = 0x7
};

enum class EAppReleaseState : uint32_t
{
	Unknown = 0x0,
	Unavailable = 0x1,
	Prerelease = 0x2,
	Preloadonly = 0x3,
	Released = 0x4,
	Disabled = 0x5
};

enum class ERemoteStorageSyncState
{
	Disabled = 0x0,
	Unknown = 0x1,
	Synchronized = 0x2,
	InProgress = 0x3,
	ChangesInCloud = 0x4,
	ChangesLocally = 0x5,
	ChangesInNloudAndLocally = 0x6,
	ConflictingChanges = 0x7,
	NotInitialized = 0x8,
};

typedef uint32_t Result_t;

enum EResult : Result_t
{
	k_EResultNoResult = 0x0,
	k_EResultOK = 0x1,
	k_EResultFailure = 0x2,
	k_EResultNoConnection = 0x3,
	k_EResultUnknown = 0x4,
	k_EResultInvalidPassword = 0x5,
	k_EResultLoggedInElsewhere = 0x6,
	k_EResultInvalidProtocol = 0x7,
	k_EResultInvalidParameter = 0x8,
	k_EResultFileNotFound = 0x9,
	k_EResultBusy = 0xa,
	k_EResultInvalidState = 0xb,
	k_EResultInvalidName = 0xc,
	k_EResultInvalidEmail = 0xd,
	k_EResultDuplicateName = 0xe,
	k_EResultAccessDenied = 0xf,
	k_EResultTimeout = 0x10,
	k_EResultBanned = 0x11,
	k_EResultAccountNotFound = 0x12,
	k_EResultInvalidSteamID = 0x13,
	k_EResultServiceUnavailable = 0x14,
	k_EResultNotLoggedOn = 0x15,
	k_EResultPending = 0x16,
	k_EResultEncryptionFailure = 0x17,
	k_EResultInsufficientPrivilege = 0x18,
	k_EResultLimitexceeded = 0x19,
	k_EResultRequestrevoked = 0x1a,
	k_EResultLicenseexpired = 0x1b,
	k_EResultAlreadyRedeemed = 0x1c,
	k_EResultDuplicatedRequest = 0x1d,
	k_EResultAlreadyOwned = 0x1e,
	k_EResultIPAddressNotFound = 0x1f,
	k_EResultPersistenceFailed = 0x20,
	k_EResultLockingFailed = 0x21,
	k_EResultSessionReplaced = 0x22,
	k_EResultConnectionFailed = 0x23,
	k_EResultHandshakeFailed = 0x24,
	k_EResultIOOperationFailed = 0x25,
	k_EResultDisconnectedByRemoteHost = 0x26,
	k_EResultShoppingCartNotFound = 0x27,
	k_EResultBlocked = 0x28,
	k_EResultIgnored = 0x29,
	k_EResultNomatch = 0x2a,
	k_EResultAccountDisabled = 0x2b,
	k_EResultServiceReadOnly = 0x2c,
	k_EResultAccountNotFeatured = 0x2d,
	k_EResultAdministratorOK = 0x2e,
	k_EResultContentVersion = 0x2f,
	k_EResultTryanotherCM = 0x30,
	k_EResultPasswordrequiredtokicksession = 0x31,
	k_EResultAlreadyLoggedInElsewhere = 0x32,
	k_EResultRequestsuspendedOrpaused = 0x33,
	k_EResultRequesthasbeencanceled = 0x34,
	k_EResultCorruptedorunrecoverabledataerror = 0x35,
	k_EResultNotenoughfreediskspace = 0x36,
	k_EResultRemotecallfailed = 0x37,
	k_EResultPasswordisnotset = 0x38,
	k_EResultExternalAccountisnotlinkedtoaSteamaccount = 0x39,
	k_EResultPSNTicketisinvalid = 0x3a,
	k_EResultExternalAccountlinkedtoanotherSteamaccount = 0x3b,
	k_EResultRemoteFileConflict = 0x3c,
	k_EResultIllegalpassword = 0x3d,
	k_EResultSameaspreviousvalue = 0x3e,
	k_EResultAccountLogonDenied = 0x3f,
	k_EResultCannotUseOldPassword = 0x40,
	k_EResultInvalidLoginAuthCode = 0x41,
	k_EResultAccountLogonDeniednomailsent = 0x42,
	k_EResultHardwarenotcapableofIPT = 0x43,
	k_EResultIPTiniterror = 0x44,
	k_EResultOperationfailedduetoparentalcontrolrestrictionsforcurrentuser = 0x45,
	k_EResultFacebookqueryreturnedanerror = 0x46,
	k_EResultExpiredLoginAuthCode = 0x47,
	k_EResultIPLoginRestrictionFailed = 0x48,
	k_EResultAccountLockedDown = 0x49,
	k_EResultAccountLogonDeniedVerifiedEmailRequired = 0x4a,
	k_EResultNomatchingURL = 0x4b,
	k_EResultBadresponse = 0x4c,
	k_EResultPasswordreentryrequired = 0x4d,
	k_EResultValueisoutofrange = 0x4e,
	k_EResultUnexpectederror = 0x4f,
	k_EResultFeatureDisabled = 0x50,
	k_EResultInvalidCEGSubmission = 0x51,
	k_EResultRestricteddevice = 0x52,
	k_EResultRegionLocked = 0x53,
	k_EResultRateLimitExceeded = 0x54,
	k_EResultAccountlogondeniedneedtwofactorcode = 0x55,
	k_EResultItemorentryhasbeendeleted = 0x56,
	k_EResultToomanylogonattempts = 0x57,
	k_EResultTwofactorcodemismatch = 0x58,
	k_EResultTwofactoractivationcodemismatch = 0x59,
	k_EResultAccountassociatedwithmultipleplayers = 0x5a,
	k_EResultNotModified = 0x5b,
	k_EResultNomobiledeviceavailable = 0x5c,
	k_EResultTimeisoutofsync = 0x5d,
	k_EResultSMScodefailed = 0x5e,
	k_EResultToomanyaccountsaccessthisresource = 0x5f,
	k_EResultToomanychangestothisaccount = 0x60,
	k_EResultToomanychangestothisphonenumber = 0x61,
	k_EResultYoumustrefundthistransactiontowallet = 0x62,
	k_EResultSendingofanemailfailed = 0x63,
	k_EResultPurchasenotyetsettled = 0x64,
	k_EResultNeedscaptcha = 0x65,
	k_EResultGameserverlogintokendenied = 0x66,
	k_EResultGameserverlogintokenownerdenied = 0x67,
	k_EResultInvaliditemtype = 0x68,
	k_EResultIPAddressBanned = 0x69,
	k_EResultGameserverlogintokenexpired = 0x6a,
	k_EResultInsufficientfunds = 0x6b,
	k_EResultToomanypending = 0x6c,
	k_EResultNositelicensesfound = 0x6d,
	k_EResultNetworksendexceeded = 0x6e,
	k_EResultAccountsnotfriends = 0x6f,
	k_EResultLimiteduseraccount = 0x70,
	k_EResultCantremoveitem = 0x71,
	k_EResultAccounthasbeendeleted = 0x72,
	k_EResultAccounthasanexistingusercancelledlicense = 0x73,
	k_EResultDeniedduetocommunitycooldown = 0x74,
	k_EResultNolauncherspecified = 0x75,
	k_EResultMustagreetoSSA = 0x76,
	k_EResultClientnolongersupported = 0x77,
	k_EResultThecurrentSteamrealmdoesnotmatchtherequestedresource = 0x78,
	k_EResultSignaturecheckfailed = 0x79,
	k_EResultFailedtoparseinput = 0x7a,
	k_EResultNoverifiedphonenumber = 0x7b,
	k_EResultInsufficientbatterycharge = 0x7c,
	k_EResultChargerrequired = 0x7d,
	k_EResultCachedcredentialisinvalid = 0x7e,
	k_EResultPhonenumberisVoiceOverIP = 0x7f,
	k_EResultThedatabeingaccessedisnotsupportedbythisAPI = 0x80,
	k_EResultMaximumfamilysizeexceeded = 0x81,
	k_EResultOfflineAppCacheinvalid = 0x82,
	k_EResultRetrylater = 0x83,
};

class CSteamId
{
public:

	constexpr CSteamId()
	{
		steamId64 = 0;
	}

	constexpr CSteamId(uint64_t id)
	{
		steamId64 = id;

		//32 bit accountId passed, fill in rest with defaults
		if (!steamId.accountType)
		{
			steamId64 |= 0x0110000100000000;
			//steamId.accountType = 1;
			//steamId.universe = 1;
			//steamId.__pad0x5[0] = 0;
			//steamId.__pad0x5[1] = 0x10;
		}
	}

	constexpr bool isSet() const
	{
		return accountId();
	}

	constexpr uint32_t accountId() const
	{
		return steamId.accountId;
	}

	constexpr uint32_t accountType() const
	{
		return steamId.accountType;
	}

	constexpr uint32_t universe() const
	{
		return steamId.universe;
	}

	struct SteamId_t
	{
		uint32_t accountId;		//0x0
		uint8_t accountType;	//0x4 - Maybe universe?
		uint8_t __pad0x5[0x2];	//0x5
		uint8_t universe;		//0x7 - Maybe accountType?
	}; //0x8
	
	union
	{
		SteamId_t steamId;
		uint64_t steamId64;
	};
};
