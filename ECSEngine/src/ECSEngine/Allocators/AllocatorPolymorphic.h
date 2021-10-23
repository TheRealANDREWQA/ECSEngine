#pragma once
#include "../Core.h"
#include "AllocatorTypes.h"

namespace ECSEngine {

	// Single threaded
	ECSENGINE_API void* Allocate(void* allocator, AllocatorType type, size_t size, size_t alignment = 8);

	// Thread safe
	ECSENGINE_API void* AllocateTs(void* allocator, AllocatorType type, size_t size, size_t alignment = 8);

	// Dynamic allocation type
	ECSENGINE_API void* Allocate(void* allocator, AllocatorType allocator_type, AllocationType allocation_type, size_t size, size_t alignment = 8);

	// Dynamic allocation type
	ECSENGINE_API void* Allocate(AllocatorPolymorphic allocator, size_t size, size_t alignment = 8);

	// Single threaded
	ECSENGINE_API void Deallocate(void* ECS_RESTRICT allocator, AllocatorType type, const void* ECS_RESTRICT buffer);

	// Thread safe
	ECSENGINE_API void DeallocateTs(void* ECS_RESTRICT allocator, AllocatorType type, const void* ECS_RESTRICT buffer);

	// Dynamic allocation type
	ECSENGINE_API void Deallocate(void* ECS_RESTRICT allocator, AllocatorType allocator_type, const void* ECS_RESTRICT buffer, AllocationType allocationType);

	// Dynamic allocation type
	ECSENGINE_API void Deallocate(AllocatorPolymorphic allocator, const void* ECS_RESTRICT buffer);

}