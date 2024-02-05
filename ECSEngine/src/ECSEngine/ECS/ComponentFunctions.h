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
		// If data size is 0, it will reference the data, else it will copy the data
		Stream<void> data;
	};

	struct SharedComponentCompareEntry {
		SharedComponentCompareFunction function = nullptr;
		Stream<void> data = {};
	};
	
}