#pragma once
#include "../Core.h"

namespace ECSEngine {

	/*
		This file exists primarly to be included in Components.h headers that
		Want to add an inline method to validate their asset pointers
	*/

	// This is the limit of assets which can be randomized on asset database load
#define ECS_ASSET_RANDOMIZED_ASSET_LIMIT 10'000

	ECS_INLINE bool IsAssetPointerValid(const void* pointer) {
		return (size_t)pointer >= ECS_ASSET_RANDOMIZED_ASSET_LIMIT;
	}

}