#include "IClientApps.hpp"

#include "../memhlp.hpp"
#include "../vftableinfo.hpp"

#include <cstdint>

int32_t IClientApps::getAppData(uint32_t appId, const char* name, const char* pChOut, uint32_t outSize)
{
	return MemHlp::callVFunc<uint32_t(*)(void*, uint32_t, const char*, const char*, uint32_t)>
	(
		 VFTIndexes::IClientApps::GetAppData,
		 this,
		 appId,
		 name,
		 pChOut,
		 outSize
	);
}

int32_t IClientApps::getAppDataSection(uint32_t appId, EAppInfoSection section, const char* pChOut, uint32_t outSize)
{
	return MemHlp::callVFunc<int32_t(*)(void*, uint32_t, uint32_t, const char*, uint32_t, uint8_t)>
	(
		 VFTIndexes::IClientApps::GetAppDataSection,
		 this,
		 appId,
		 section,
		 pChOut,
		 outSize,
		 1
	);
}

bool IClientApps::requestAppInfoUpdate(uint32_t appId)
{
	return MemHlp::callVFunc<bool(*)(void*, const uint32_t*, int32_t)>(VFTIndexes::IClientApps::RequestAppInfoUpdate, this, &appId, 1);
}

EAppType IClientApps::getAppType(uint32_t appId)
{
	return MemHlp::callVFunc<EAppType(*)(void*, uint32_t)>(VFTIndexes::IClientApps::GetAppType, this, appId);
}

IClientApps* g_pClientApps;
