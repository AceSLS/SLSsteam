#include "IClientCompat.hpp"

#include "CUtl.hpp"
#include "steam.hpp"

#include "../memhlp.hpp"
#include "../vftableinfo.hpp"


bool IClientCompat::isCompatToolEnabled(const AppId_t appId)
{
	return MemHlp::callVFunc<bool(*)(void*, AppId_t)>
	(
		VFTIndexes::IClientCompat::BIsCompatibilityToolEnabled.index,
		this,
		appId
	);
}

const char* IClientCompat::getDisplayName(const char* name)
{
	return MemHlp::callVFunc<const char*(*)(void*, const char*)>
	(
		VFTIndexes::IClientCompat::GetCompatToolDisplayName.index,
		this,
		name
	);
}

void IClientCompat::getCompatToolsForApp(const AppId_t appId, CUtlVector<CUtlString>* pVecTools)
{
	MemHlp::callVFunc<void(*)(void*, CUtlVector<CUtlString>*, AppId_t)>
	(
		VFTIndexes::IClientCompat::GetAvailableCompatToolsForApp.index,
		this,
		pVecTools,
		appId
	);
}

const char* IClientCompat::getCompatToolName(const AppId_t appId)
{
	return MemHlp::callVFunc<const char*(*)(void*, AppId_t)>
	(
		VFTIndexes::IClientCompat::GetCompatToolName.index,
		this,
		appId
	);
}

void IClientCompat::specifyCompatTool(const AppId_t appId, const char* name, const char* config, int32_t priority)
{
	MemHlp::callVFunc<void(*)(void*, AppId_t, const char*, const char*, int32_t)>
	(
		VFTIndexes::IClientCompat::SpecifyCompatTool.index,
		this,
		appId,
		name,
		config,
		priority
	);
}
