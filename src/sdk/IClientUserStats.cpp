#include "IClientUserStats.hpp"

#include "../memhlp.hpp"
#include "../vftableinfo.hpp"


bool IClientUserStats::getAchievement(const GameId_t gameId, const char* name, bool *pUnlocked, uint32_t* pUnlockTime)
{
	return MemHlp::callVFunc<bool(*)(void*, const GameId_t*, const char*, bool*, uint32_t*)>
	(
		VFTIndexes::IClientUserStats::GetAchievement.index,
		this,
		&gameId,
		name,
		pUnlocked,
		pUnlockTime
	);
}

const char* IClientUserStats::getAchievementDisplayAttribute(const GameId_t gameId, const char* name, const char* attribute)
{
	return MemHlp::callVFunc<const char*(*)(void*, const GameId_t*, const char*, const char*, bool)>
	(
		VFTIndexes::IClientUserStats::GetAchievementDisplayAttribute.index,
		this,
		&gameId,
		name,
		attribute,
		false
	);
}

bool IClientUserStats::getAchievementProgress(const GameId_t gameId, const char* name, float* pMin, float* pCur, float* pMax)
{
	return MemHlp::callVFunc<bool(*)(void*, const GameId_t*, const char*, float*, float*, float*)>
	(
		VFTIndexes::IClientUserStats::GetAchievementProgress.index,
		this,
		&gameId,
		name,
		pMin,
		pCur,
		pMax
	);
}

uint32_t IClientUserStats::getNumAchievements(const GameId_t gameId)
{
	return MemHlp::callVFunc<uint32_t(*)(void*, const GameId_t*)>(VFTIndexes::IClientUserStats::GetNumAchievements.index, this, &gameId);
}

uint32_t IClientUserStats::getNumStats(const GameId_t gameId)
{
	return MemHlp::callVFunc<uint32_t(*)(void*, const GameId_t*)>(VFTIndexes::IClientUserStats::GetNumStats.index, this, &gameId);
}

const char* IClientUserStats::getAchievementName(const GameId_t gameId, const uint32_t index)
{
	return MemHlp::callVFunc<const char*(*)(void*, const GameId_t*, uint32_t)>(VFTIndexes::IClientUserStats::GetAchievementName.index, this, &gameId, index);
}

const char* IClientUserStats::getStatName(const GameId_t gameId, const uint32_t index)
{
	return MemHlp::callVFunc<const char*(*)(void*, const GameId_t*, uint32_t)>(VFTIndexes::IClientUserStats::GetStatName.index, this, &gameId, index);
}
