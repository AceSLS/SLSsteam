#pragma once

#include "CUtl.hpp"

#include <cstdint>


class CPackageInfo
{
public:
	uint32_t packageId;			//0x0
	uint8_t __pad0x4[0x2C];		//0x4
	void* keyValues;			//0x30
	uint8_t __pad0x34[0x4];		//0x34
	CUtlVector<uint32_t> appIds; //0x38
	CUtlVector<uint32_t> depotIds; //0x48
	CUtlVector<uint32_t> microItems; //0x58
}; //???

//class CPackageInfo
//{
//public:
//	uint32_t packageId;			//0x0
//	uint8_t __pad0x4[0x34];		//0x4
//	uint32_t* pAppIds;			//0x38
//	uint8_t __pad0x3C[0x8];		//0x3C
//	uint32_t numAppIds;			//0x44
//	uint32_t* pDepotIds;		//0x48
//	uint8_t __pad0x4C[0x8];		//0x4C
//	uint32_t numDepots;			//0x54
//};

class CPackageInfoCache
{
public:
	uint8_t __pad0x0[0x24];		//0x0
	uint32_t numPackages;		//0x24
	uint8_t __pad0x28[0x4];		//0x28
	uintptr_t vecPackages;		//0x2C
	
	CPackageInfo* getPackage(unsigned int index);
};
