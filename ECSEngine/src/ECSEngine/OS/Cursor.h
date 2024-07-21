#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	namespace OS {

		ECSENGINE_API uint2 GetCursorPosition(void* window_handle);

		ECSENGINE_API void SetCursorPosition(void* window_handle, uint2 position);

		// The cursor is placed relative to the OS client region
		// Returns the final position relative to the window location
		ECSENGINE_API uint2 SetCursorPositionRelative(void* window_handle, uint2 position);

		ECSENGINE_API uint2 GetOSWindowPosition(void* window_handle);

		ECSENGINE_API uint2 GetOSWindowSize(void* window_handle);

	}

}