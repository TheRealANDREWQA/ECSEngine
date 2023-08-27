#pragma once
#include "../Core.h"

namespace ECSEngine {

	struct DebugInfo {
		ECS_INLINE DebugInfo() : file(nullptr), function(nullptr), line(0) {}
		ECS_INLINE DebugInfo(const char* file, const char* function, int line) : file(file), function(function), line(line) {}

		const char* file;
		const char* function;
		int line;
	};

#define ECS_DEBUG_INFO DebugInfo(ECS_LOCATION) 

}