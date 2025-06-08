#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	namespace OS {

		// Local time
		ECSENGINE_API Date GetLocalTime();

		ECSENGINE_API void OSMessageBox(const char* message, const char* title);

		// Tries to find the absolute path of an executable (with the extension added) using the standard OS
		// Search route. It will write the path into the given storage and return
		// A view into it. Returns an empty string if the executable could not be found.
		ECSENGINE_API Stream<wchar_t> SearchForExecutable(Stream<wchar_t> executable, CapacityStream<wchar_t>& path_storage);

		ECS_INLINE size_t Rdtsc() {
			return __rdtsc();
		}

	}

}