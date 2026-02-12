#pragma once
#include <cstdint>


struct SteamItemDetails_t
{
public:
	uint64_t handle;
	uint32_t id;
	uint16_t quantity;
	uint16_t flags;
};
