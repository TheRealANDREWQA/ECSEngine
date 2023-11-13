#pragma once
#include "../Core.h"

namespace ECSEngine {

	enum ECS_BYTE_UNIT_TYPE : unsigned char {
		// This is used for the conversion to appropriate unit
		ECS_BYTE_TO_BYTE,
		ECS_BYTE_TO_KB,
		ECS_BYTE_TO_MB,
		ECS_BYTE_TO_GB,
		ECS_BYTE_TO_TB
	};

	ECSENGINE_API size_t ConvertToByteUnit(size_t bytes, ECS_BYTE_UNIT_TYPE type);

	// It finds the most suitable byte unit and give back the unit alongside the value
	ECSENGINE_API size_t ConvertToAppropriateByteUnit(size_t bytes, ECS_BYTE_UNIT_TYPE* type);

}