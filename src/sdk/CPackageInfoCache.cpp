#include "CPackageInfoCache.hpp"

#include "../log.hpp"

CPackageInfo* CPackageInfoCache::getPackage(unsigned int index)
{
	if (index >= numPackages)
	{
		return nullptr;
	}

	constexpr uint32_t elemsOffset = 0x14;
	constexpr uint32_t elemSize = 0x18;

	uintptr_t pPkg = vecPackages + elemsOffset + elemSize * index;

	return *reinterpret_cast<CPackageInfo**>(pPkg);
}
