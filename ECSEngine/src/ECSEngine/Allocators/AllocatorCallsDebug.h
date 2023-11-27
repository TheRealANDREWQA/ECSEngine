#pragma once
#include "../Core.h"
#include "../Utilities/DebugInfo.h"
#include "AllocatorTypes.h"

namespace ECSEngine {

#define ECS_DEBUG_ALLOCATOR_DEFAULT_FILE L"DebugAllocations.txt"

	enum ECS_DEBUG_ALLOCATOR_FUNCTION : unsigned char {
		ECS_DEBUG_ALLOCATOR_ALLOCATE,
		ECS_DEBUG_ALLOCATOR_DEALLOCATE,
		ECS_DEBUG_ALLOCATOR_CLEAR,
		ECS_DEBUG_ALLOCATOR_FREE,
		ECS_DEBUG_ALLOCATOR_REALLOCATE,
		ECS_DEBUG_ALLOCATOR_RETURN_TO_MARKER,
		ECS_DEBUG_ALLOCATOR_FUNCTION_COUNT
	};

	enum ECS_DEBUG_ALLOCATOR_MANAGER_CAPACITY : unsigned char {
		ECS_DEBUG_ALLOCATOR_MANAGER_CAPACITY_LOW, // Enough for hundreds of total calls
		ECS_DEBUG_ALLOCATOR_MANAGER_CAPACITY_MEDIUM, // Enough for thousands of total calls
		ECS_DEBUG_ALLOCATOR_MANAGER_CAPACITY_HIGH // Enough for hundreds of thousands of total calls
	};

	struct TrackedAllocation {
		const void* allocated_pointer;
		// At the moment, this is only used by the Reallocate
		// which fills this value with the value of the returned pointer
		const void* secondary_pointer = nullptr;
		DebugInfo debug_info;
		ECS_DEBUG_ALLOCATOR_FUNCTION function_type;
	};

	struct DebugAllocatorManagerDescriptor {
		ECS_DEBUG_ALLOCATOR_MANAGER_CAPACITY capacity;
		const wchar_t* log_file = nullptr;
		// If this is set to true when the log file is not specified, it will use the default name
		// in the macro ECS_DEBUG_ALLOCATOR_DEFAULT_FILE
		bool enable_global_write_to_file = true;
		// Can override this - this is the maximum number of entries
		// in the ring buffer for an allocator
		unsigned int default_ring_buffer_size = ECS_KB;
	};

	// Can optionally give a filepath to write the state when one of the allocators
	// has its own queue filled
	ECSENGINE_API void DebugAllocatorManagerInitialize(const DebugAllocatorManagerDescriptor* descriptor);

	// Makes an allocator have "infinite" capacity - basically recording all the
	// calls to an allocator
	ECSENGINE_API void DebugAllocatorManagerSetResizable(const void* allocator, ECS_ALLOCATOR_TYPE allocator_type = ECS_ALLOCATOR_TYPE_COUNT);

	// Binds a name to an allocator for easier debugging
	ECSENGINE_API void DebugAllocatorManagerSetName(const void* allocator, const char* name, ECS_ALLOCATOR_TYPE allocator_type = ECS_ALLOCATOR_TYPE_COUNT);

	// Combines into a single action the calls to SetName and SetResizable
	ECSENGINE_API void DebugAllocatorManagerSetResizableAndName(const void* allocator, const char* name, ECS_ALLOCATOR_TYPE allocator_type = ECS_ALLOCATOR_TYPE_COUNT);

	ECSENGINE_API void DebugAllocatorManagerChangeOrAddEntry(
		const void* allocator, 
		const char* name, 
		bool resizable, 
		ECS_ALLOCATOR_TYPE allocator_type = ECS_ALLOCATOR_TYPE_COUNT
	);

	// If the filepath is set to nullptr, it will the one with which it was initialized
	ECSENGINE_API void DebugAllocatorManagerWriteState(const wchar_t* log_file = nullptr);

	ECSENGINE_API void DebugAllocatorManagerAddEntry(const void* allocator, ECS_ALLOCATOR_TYPE allocator_type, const TrackedAllocation* allocation);

	// The returned string is a string literal
	ECSENGINE_API const char* DebugAllocatorFunctionName(ECS_DEBUG_ALLOCATOR_FUNCTION function);

}