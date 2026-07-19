#pragma once

enum EAppOwnershipFlags
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
