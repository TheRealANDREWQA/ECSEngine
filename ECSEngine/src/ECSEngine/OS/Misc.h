#pragma once
#include "../Core.h"
#include "../Utilities/BasicTypes.h"

namespace ECSEngine {

	namespace OS {

		// Local time
		ECSENGINE_API Date GetLocalTime();

		ECSENGINE_API void OSMessageBox(const char* message, const char* title);

		ECS_INLINE size_t Rdtsc() {
			return __rdtsc();
		}

	}

}