#pragma once

enum ELicenseType
{
	k_ELicenseFlagsNone = 0x0,
	k_ELicenseTypeNone = 0x0,
	k_ELicenseTypeSinglePurchase = 0x1,
	k_ELicenseTypeSinglePurchaseLimitedUse = 0x2,
	k_ELicenseTypeRecurringCharge = 0x3,
	//k_ELicenseTypeUnknown = 0x4,
	//k_ELicenseTypeUnknown = 0x5,
	k_ELicenseTypeRecurringpaymentoption = 0x6,
	k_ELicenseTypeLimiteduseDelayedActivation = 0x7
};
