#pragma once
#include "../Core.h"
#include "AllocatorTypes.h"

namespace ECSEngine {

	ECSENGINE_API void* Allocate(void* allocator, AllocatorType type, size_t size, size_t alignment = 8);

	ECSENGINE_API void* AllocateTs(void* allocator, AllocatorType type, size_t size, size_t alignment = 8);

	ECSENGINE_API void Deallocate(void* allocator, AllocatorType type, const void* buffer);

	ECSENGINE_API void DeallocateTs(void* allocator, AllocatorType type, const void* buffer);

}