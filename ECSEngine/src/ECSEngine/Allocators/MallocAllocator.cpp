#include "ecspch.h"
#include "MallocAllocator.h"
#include "AllocatorPolymorphic.h"

namespace ECSEngine {

	/*
		For this allocator, the debug info cannot be used to track the allocations. If we need that,
		we will see how to implement it. At the moment, it is not required.
	*/

	void* MallocAllocator::Allocate(size_t size, size_t alignment, DebugInfo debug_info) {
		return Malloc(size, alignment);
	}
	
	void* MallocAllocator::AllocateTs(size_t size, size_t alignment, DebugInfo debug_info) {
		// Malloc is already thread safe
		return Malloc(size, alignment);
	}
	
	bool MallocAllocator::DeallocateNoAssert(const void* buffer, DebugInfo debug_info) {
		// For this allocator, no assert doesn't make sense, since we can't tell if it belongs to malloc
		// Or not, so we will have to simply assume that it belongs
		ECSEngine::Free((void*)buffer);
		return true;
	}
	
	bool MallocAllocator::DeallocateNoAssertTs(const void* buffer, DebugInfo debug_info) {
		// Same as above, Free is already thread-safe
		ECSEngine::Free((void*)buffer);
		return true;
	}
	
	void* MallocAllocator::Reallocate(const void* buffer, size_t new_size, size_t alignment, DebugInfo debug_info) {
		return ECSEngine::Realloc((void*)buffer, new_size, alignment);
	}
	
	void* MallocAllocator::ReallocateTs(const void* buffer, size_t new_size, size_t alignment, DebugInfo debug_info) {
		// Same as above, already thread-safe
		return ECSEngine::Realloc((void*)buffer, new_size, alignment);
	}
	
	// This operation doesn't make sense
	void MallocAllocator::Clear(DebugInfo debug_info) {}
	
	void MallocAllocator::Free(bool assert_that_is_standalone, DebugInfo debug_info) {
		// This operation doesn't make sense. Make sure it is not called
		ECS_ASSERT(false, "MallocAllocator cannot be freed!");
	}
	
	void MallocAllocator::FreeFrom(AllocatorBase* backup_allocator, DebugInfo debug_info) {
		ECS_ASSERT(false, "MallocAllocator cannot be freed from another allocator!");
	}
	
	bool MallocAllocator::Belongs(const void* buffer) const {
		// Can't determine if this is true or not, assert that it is not called
		ECS_ASSERT(false, "MallocAllocator cannot verify if a buffer belongs to it or not!");
		return true;
	}
	
	void* MallocAllocator::GetAllocatedBuffer() const {
		// Doesn't make sense
		ECS_ASSERT(false, "MallocAllocator cannot retrieve allocator buffer!");
		return nullptr;
	}

	bool MallocAllocator::IsEmpty() const {
		// Doesn't make sense
		ECS_ASSERT(false, "MallocAllocatr cannot verify if it is empty or not!");
		return false;
	}
	
	size_t MallocAllocator::GetCurrentUsage() const {
		// Doesn't make sense
		ECS_ASSERT(false, "MallocAllocator cannot retrieve current usage!");
		return 0;
	}
	
	size_t MallocAllocator::GetRegions(void** region_pointers, size_t* region_size, size_t pointer_capacity) const {
		// Doesn't make sense
		ECS_ASSERT(false, "MallocAllocator cannot retrieve allocated regions!");
		return 0;
	}

	static MallocAllocator global_allocator;
	AllocatorBase* ECS_MALLOC_ALLOCATOR = &global_allocator;

}