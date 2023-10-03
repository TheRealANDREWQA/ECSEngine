#pragma once
#include "../Core.h"

namespace ECSEngine {

	extern bool ECS_GLOBAL_ASSERT_CRASH;
	// If this is set to true, before __debugbreak when not crashing
	// It will write the debug allocator call file
	extern bool ECS_GLOBAL_ASSERT_WRITE_DEBUG_ALLOCATOR_CALLS;

	ECSENGINE_API void Assert(bool condition, const char* filename, unsigned int line, const char* error_message = nullptr);

#define ECS_ASSERT(condition, ...) Assert(condition, __FILE__, __LINE__, __VA_ARGS__);
	

}