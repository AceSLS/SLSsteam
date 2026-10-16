#include "IClientAppManager.hpp"

#include "../memhlp.hpp"
#include "../vftableinfo.hpp"

#include <cstdint>


EAppState IClientAppManager::getAppInstallState(const AppId_t appId)
{
	return MemHlp::callVFunc<EAppState(*)(void*, AppId_t)>(VFTIndexes::IClientAppManager::GetAppInstallState.index, this, appId);
}

steamsize_t IClientAppManager::getLibraryFolderLabel(const uint32_t index, char* pChLabel, const steamsize_t labelSize)
{
	return MemHlp::callVFunc<uint32_t(*)(void*, uint32_t, char*, steamsize_t)>
	(
		VFTIndexes::IClientAppManager::GetLibraryFolderLabel.index,
		this,
		index,
		pChLabel,
		labelSize
	);
}

uint32_t IClientAppManager::getLibraryFolderPath(const uint32_t index, char* pChPath, const steamsize_t pathSize)
{
	return MemHlp::callVFunc<uint32_t(*)(void*, uint32_t, char*, steamsize_t)>
	(
		VFTIndexes::IClientAppManager::GetLibraryFolderPath.index,
		this,
		index,
		pChPath,
		pathSize
	);
}

uint32_t IClientAppManager::getNumLibraryFolders()
{
	return MemHlp::callVFunc<uint32_t(*)(void*)>(VFTIndexes::IClientAppManager::GetNumLibraryFolders.index, this);
}

bool IClientAppManager::installApp(const AppId_t appId, const uint32_t libraryIndex)
{
	return MemHlp::callVFunc<bool(*)(void*, AppId_t, uint32_t, bool)>(VFTIndexes::IClientAppManager::InstallApp.index, this, appId, libraryIndex, false);
}

uint32_t IClientAppManager::uninstallApp(const AppId_t appId)
{
	return MemHlp::callVFunc<uint32_t(*)(void*, AppId_t)>(VFTIndexes::IClientAppManager::UninstallApp.index, this, appId);
}
