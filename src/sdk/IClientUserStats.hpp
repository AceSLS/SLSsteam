#pragma once

#include "types.hpp"

#include <cstdint>


class IClientUserStats
{
public:
	bool getAchievement(const GameId_t gameId, const char* name, bool *pUnlocked, uint32_t* pUnlockTime);
	const char* getAchievementDisplayAttribute(const GameId_t gameId, const char* name, const char* attribute);
	uint32_t getNumAchievements(const GameId_t gameId);
	uint32_t getNumStats(const GameId_t gameId); //Defect?
	const char* getAchievementName(const GameId_t gameId, const uint32_t index);
	const char* getStatName(const GameId_t gameId, const uint32_t index); //Defect?
};
