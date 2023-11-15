#pragma once
#include "../Core.h"

namespace ECSEngine {

	namespace OS {

		// Returns an allocation from the system virtual allocation where you can specify if the memory is
		// to be commited or not
		ECSENGINE_API void* VirtualAllocation(size_t size);

		// The pointer must be obtained from VirtualAllocation
		ECSENGINE_API void VirtualDeallocation(void* block);

		// Returns true if it succeeded, else false. The page enter read-write mode
		ECSENGINE_API bool DisableVirtualWriteProtection(void* allocation);

		// Returns true if it succeeded, else false. The page enter read only mode
		ECSENGINE_API bool EnableVirtualWriteProtection(void* allocation);

	}

}