#pragma once
#include "../Allocators/MemoryArena.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	struct ComponentCopyFunctionData {
		// This function data is passed in at registration time
		void* function_data;
		void* destination;
		const void* source;
		MemoryArena* allocator;
		// If this is set, you must also deallocate the existing buffers
		// (Or you can reallocate)
		bool deallocate_previous;
	};

	// The function should copy all fields
	typedef void (*ComponentCopyFunction)(ComponentCopyFunctionData* data);

	struct ComponentDeallocateFunctionData {
		// This function data is passed in at registration time
		void* function_data;
		void* data;
		MemoryArena* allocator;
	};

	// The function should deallocate all fields and optionally invalidate the fields
	// For further calls
	typedef void (*ComponentDeallocateFunction)(ComponentDeallocateFunctionData* data);

	struct SharedComponentCompareFunctionData {
		void* function_data;
		const void* first;
		const void* second;
	};

	typedef bool (*SharedComponentCompareFunction)(SharedComponentCompareFunctionData* data);

	struct ComponentFunctions {
		ECS_INLINE ComponentFunctions() {
			memset(this, 0, sizeof(*this));
		}

		size_t allocator_size;
		ComponentCopyFunction copy_function;
		ComponentDeallocateFunction deallocate_function;
		Copyable* data;
	};

	struct SharedComponentCompareEntry {
		SharedComponentCompareFunction function = nullptr;
		Copyable* data;
		// If this flag is set, the function will use the same data as the copy deallocate
		bool use_copy_deallocate_data = false;
	};
	
}