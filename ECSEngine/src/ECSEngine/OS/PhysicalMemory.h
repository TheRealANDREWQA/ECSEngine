#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	namespace OS {

		// Call this before using any function from inside this header
		ECSENGINE_API void InitializePhysicalMemoryPageSize();

		ECSENGINE_API size_t GetPhysicalMemoryPageSize();

		// Returns -1 if it fail
		ECSENGINE_API size_t ProcessPhysicalMemoryUsage();

		// This function indicates to the operating system that memory can be taken away from this
		// Process and reused
		ECSENGINE_API void EmptyPhysicalMemory();

		// Returns true if it succeeded, else false. It will remove the pages
		// From the working set (physical memory) of the process
		ECSENGINE_API bool EmptyPhysicalMemory(void* allocation, size_t allocation_size);

		// Returns the amount of physical memory allocated starting from that address
		// Until the specified allocation size
		// This is a very heavy duty function for large amounts of functions
		// (For GB it can takes tens of ms). Do not call this from multiple thredas,
		// It will lead to disastrous performance (internally the OS probably locks
		// the working set to gather information). 
		// Returns -1 in case there was an error
		ECSENGINE_API size_t GetPhysicalMemoryBytesForAllocation(void* allocation, size_t allocation_size);

		// This function will add the PAGE_GUARD for the pages inside the virtual allocation
		// Returns true if it succeeded, else false
		ECSENGINE_API bool GuardPages(void* allocation, size_t allocation_size);

		// This function will remove the PAGE_GUARD for the pages inside the virtual allocation
		// Returns true if it succeeded, else false
		ECSENGINE_API bool UnguardPages(void* allocation, size_t allocation_size);

	}

}