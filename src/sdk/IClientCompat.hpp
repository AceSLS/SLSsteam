#pragma once

#include "CUtl.hpp"
#include "types.hpp"

#include <cstdint>


SDK_Class IClientCompat
{
public:
	bool isCompatToolEnabled(const AppId_t appId);
	const char* getDisplayName(const char* name);
	void getCompatToolsForApp(const AppId_t appId, CUtlVector<CUtlString>* pVecTools);
	const char* getCompatToolName(const AppId_t appId);

	//Set name to "" to unset
	void specifyCompatTool(const AppId_t appId, const char* name, const char* config, int32_t priority);
};
