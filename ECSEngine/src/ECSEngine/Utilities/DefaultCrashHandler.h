#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	struct World;

	ECSENGINE_API void DefaultCrashHandlerFunction(void* data, const char* error_string);

	// The paths must be absolute paths, not relative ones
	ECSENGINE_API void SetDefaultCrashHandler(World* world, Stream<wchar_t*> module_search_paths);

}