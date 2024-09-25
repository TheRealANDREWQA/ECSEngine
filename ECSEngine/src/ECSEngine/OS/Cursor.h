#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	namespace OS {

		// Returns the global cursor position without any window consideration
		// They can be negative
		ECSENGINE_API int2 GetCursorPosition();

		// Returns coordinates relative to the given window handle
		ECSENGINE_API int2 GetCursorPosition(void* window_handle);

		ECSENGINE_API void SetCursorPosition(void* window_handle, uint2 position);

		// The cursor is placed relative to the OS client region
		// Returns the final position relative to the window location
		ECSENGINE_API uint2 SetCursorPositionRelative(void* window_handle, uint2 position);

		// This position includes the non-client area as well
		ECSENGINE_API int2 GetOSWindowPosition(void* window_handle);
		
		// This position includes only the client area
		ECSENGINE_API int2 GetOSWindowPositionClient(void* window_handle);

		// This size includes the non-client area as well
		ECSENGINE_API uint2 GetOSWindowSize(void* window_handle);
		
		// This size includes only the client area
		ECSENGINE_API uint2 GetOSWindowSizeClient(void* window_handle);

		// Confines the cursor to this region, preventing it from going outside the area
		// Set a size of { 0, 0 } to disable clipping
		ECSENGINE_API void ClipCursorToRectangle(int2 position, uint2 size);

	}

}