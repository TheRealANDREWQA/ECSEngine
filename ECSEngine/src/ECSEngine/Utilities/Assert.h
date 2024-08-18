#pragma once
#include "../Core.h"

/*
	At the moment, the reason to keep assertions is that we might choose to strip them out of the
	Client Executable at some point in the future. At the moment, assertions and crashes behave identically
	When assertions are diverted to the crash handler and it might seem like redundant code to keep around
	The current decision is to keep these around in case such a decision is taken
*/

namespace ECSEngine {

	ECSENGINE_API extern bool ECS_GLOBAL_ASSERT_CRASH;
	// If this is set to true, before __debugbreak when not crashing
	// It will write the debug allocator call file
	ECSENGINE_API extern bool ECS_GLOBAL_ASSERT_WRITE_DEBUG_ALLOCATOR_CALLS;

	// This assert can be diverted to a crash
	ECSENGINE_API void Assert(bool condition, const char* filename, const char* function, unsigned int line, const char* error_message = nullptr);

	// This assert cannot be diverted to a crash
	ECSENGINE_API void HardAssert(bool condition, const char* filename, const char* function, unsigned int line, const char* error_message = nullptr);

	// This type of assert can be diverted to a crash
#define ECS_ASSERT(condition, ...) ECSEngine::Assert(condition, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);
#define ECS_ASSERT_FORMAT(condition, ...) if (!(condition)) { /* Use a pre-check such that the formatting parameters are evaluated only if the branch is taken */\
	ECS_FORMAT_TEMP_STRING(__message, __VA_ARGS__);  \
	ECSEngine::Assert(condition, __FILE__, __FUNCTION__, __LINE__, __message.buffer); \
}
	
	// This type of assert cannot be diverted to a crash
#define ECS_HARD_ASSERT(condition, ...) ECSEngine::HardAssert(condition, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__);
#define ECS_HARD_ASSERT_FORMAT(condition, ...) if (!(condition)) { /* Use a pre-check such that the formatting parameters are evaluated only if the branch is taken */ \
		ECS_FORMAT_TEMP_STRING(__message, __VA_ARGS__); \
		ECSEngine::HardAssert(condition, __FILE__, __FUNCTION__, __LINE, __message.buffer); \
	}

}