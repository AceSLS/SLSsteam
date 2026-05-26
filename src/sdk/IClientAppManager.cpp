#include "IClientAppManager.hpp"

#include "../memhlp.hpp"
#include "../vftableinfo.hpp"

#include <cstdint>

EAppUpdateError IClientAppManager::installApp(uint32_t appId, int32_t libraryIndex)
{
	return MemHlp::callVFunc<EAppUpdateError(*)(void*, uint32_t, int32_t, uint8_t)>(VFTIndexes::IClientAppManager::InstallApp, this, appId, libraryIndex, 0);
}

EAppState IClientAppManager::getAppInstallState(uint32_t appId)
{
	return MemHlp::callVFunc<EAppState(*)(void*, uint32_t)>(VFTIndexes::IClientAppManager::GetAppInstallState, this, appId);
}

IClientAppManager* g_pClientAppManager;
