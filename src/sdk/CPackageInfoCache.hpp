#pragma once

#include "CUtl.hpp"
#include "steam.hpp"

#include <cstdint>


class CPackageInfo
{
public:
	AppId_t packageId;					//0x0
	uint8_t __pad0x4[0x2C];				//0x4
	void* keyValues;					//0x30
	uint8_t __pad0x34[0x4];				//0x34
	CUtlVector<AppId_t> appIds;			//0x38
	CUtlVector<AppId_t> depotIds;		//0x48
	CUtlVector<uint32_t> microItems;	//0x58
}; //???

class CPackageInfoCache
{
public:
	uint8_t __pad0x0[0x24];		//0x0
	uint32_t numPackages;		//0x24
	uint8_t __pad0x28[0x4];		//0x28
	uintptr_t vecPackages;		//0x2C
	
	CPackageInfo* getPackage(unsigned int index);
};
