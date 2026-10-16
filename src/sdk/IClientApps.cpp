#include "IClientApps.hpp"

#include "../memhlp.hpp"
#include "../vftableinfo.hpp"

#include <cstdint>


int32_t IClientApps::getAppData(const AppId_t appId, const char* name, const char* pChOut, steamsize_t outSize)
{
	return MemHlp::callVFunc<uint32_t(*)(void*, AppId_t, const char*, const char*, steamsize_t)>
	(
		 VFTIndexes::IClientApps::GetAppData.index,
		 this,
		 appId,
		 name,
		 pChOut,
		 outSize
	);
}

uint32_t IClientApps::getAppDataSection(const AppId_t appId, const EAppInfoSection section, const char* pChOut, const steamsize_t outSize)
{
	return MemHlp::callVFunc<uint32_t(*)(void*, AppId_t, EAppInfoSection, const char*, steamsize_t, uint8_t)>
	(
		 VFTIndexes::IClientApps::GetAppDataSection.index,
		 this,
		 appId,
		 section,
		 pChOut,
		 outSize,
		 1
	);
}

bool IClientApps::requestAppInfoUpdate(const AppId_t* appIds, const steamsize_t numAppIds)
{
	return MemHlp::callVFunc<bool(*)(void*, const AppId_t*, steamsize_t)>
	(
		VFTIndexes::IClientApps::RequestAppInfoUpdate.index,
		this,
		appIds,
		numAppIds
	);
}

EAppType IClientApps::getAppType(const AppId_t appId)
{
	return MemHlp::callVFunc<EAppType(*)(void*, AppId_t)>(VFTIndexes::IClientApps::GetAppType.index, this, appId);
}
