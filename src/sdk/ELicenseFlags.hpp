#pragma once

enum ELicenseFlags
{
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
