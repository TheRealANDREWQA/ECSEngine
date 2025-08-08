#pragma once
#include "../Core.h"
#include "AllocatorTypes.h"
#include <malloc.h>
#include "../Utilities/DebugInfo.h"
#include "AllocatorBase.h"

namespace DirectX {
	class ScratchImage;
}

#define ECS_ALIGNED_MALLOC(size, alignment) _aligned_malloc(size, alignment)
#define ECS_ALIGNED_FREE(allocation) _aligned_free(allocation)
#define ECS_ALIGNED_REALLOC(allocation, size, alignment) _aligned_realloc(allocation, size, alignment)

// It will allocate a buffer from the stack if it is bellow
// The threshold, else it will use the allocator. You must deallocate
// This using ECS_FREEA_ALLOCATOR in order to not leak the allocation
#define ECS_MALLOCA_ALLOCATOR(size, stack_size, allocator) ((size) <= (stack_size) ? ECS_STACK_ALLOC(size) : ECSEngine::Allocate(allocator, size))
#define ECS_FREEA_ALLOCATOR(allocation, size, stack_size, allocator) do { if ((size) > (stack_size)) { ECSEngine::Deallocate(allocator, allocation); } } while (0) 

// It will allocate a buffer from the stack if it is bellow
// The threshold, else it will use the allocator. You must deallocate
// This using ECS_FREEA_ALLOCATOR_DEFAULT in order to not leak the allocation
// It uses a default stack value of 64 ECS_KB
#define ECS_MALLOCA_ALLOCATOR_DEFAULT(size, allocator) ECS_MALLOCA_ALLOCATOR(size, ECS_KB * 64, allocator)
#define ECS_FREEA_ALLOCATOR_DEFAULT(allocation, size, allocator) ECS_FREEA_ALLOCATOR(allocation, size, ECS_KB * 64, allocator)

namespace ECSEngine {

	// This uses the same malloc as the MallocAllocator
	ECS_INLINE void* Malloc(size_t size, size_t alignment = alignof(void*)) {
		return ECS_ALIGNED_MALLOC(size, alignment);
	}

	// This is a shortcut, it uses the correct free function to deallocate a block that was allocated
	// With Malloc
	ECS_INLINE void Free(void* buffer) {
		ECS_ALIGNED_FREE(buffer);
	}

	// The counterpart to Malloc and Free
	ECS_INLINE void* Realloc(void* block, size_t new_size, size_t alignment = alignof(void*)) {
		return ECS_ALIGNED_REALLOC(block, new_size, alignment);
	}

	// Dynamic allocation type
	ECS_INLINE void* Allocate(AllocatorPolymorphic allocator, size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (allocator.allocation_type == ECS_ALLOCATION_SINGLE) {
			return allocator.allocator->Allocate(size, alignment, debug_info);
		}
		else {
			return allocator.allocator->AllocateTs(size, alignment, debug_info);
		}
	}

	// Dynamic allocation type
	template<typename T>
	ECS_INLINE T* Allocate(AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
		return (T*)Allocate(allocator, sizeof(T), alignof(T), debug_info);
	}

	ECS_INLINE void ClearAllocator(AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
		allocator.allocator->Clear(debug_info);
	}

	ECS_INLINE void Deallocate(AllocatorPolymorphic allocator, const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (allocator.allocation_type == ECS_ALLOCATION_SINGLE) {
			allocator.allocator->Deallocate(buffer, debug_info);
		}
		else {
			allocator.allocator->DeallocateTs(buffer, debug_info);
		}
	}

	// Dynamic allocation type
	ECS_INLINE bool DeallocateNoAssert(AllocatorPolymorphic allocator, const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (allocator.allocation_type == ECS_ALLOCATION_SINGLE) {
			return allocator.allocator->DeallocateNoAssert(buffer, debug_info);
		}
		else {
			return allocator.allocator->DeallocateNoAssertTs(buffer, debug_info);
		}
	}

	template<bool trigger_error_if_not_found = true>
	ECS_INLINE bool Deallocate(AllocatorPolymorphic allocator, const void* block, DebugInfo debug_info = ECS_DEBUG_INFO) {
		if constexpr (trigger_error_if_not_found) {
			Deallocate(allocator, block, debug_info);
			return true;
		}
		else {
			return DeallocateNoAssert(allocator, block, debug_info);
		}
	}

	// It makes sense only for MemoryManager, ResizableLinearAllocator
	// For all the other types it will do nothing
	ECS_INLINE void FreeAllocator(AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
		allocator.allocator->Free(false, debug_info);
	}

	// Returns the buffer from which the allocator was initialized. It doesn't make sense for the resizable allocators
	// (MemoryManager, ResizableLinearAllocator). It returns nullptr for these
	ECS_INLINE void* GetAllocatorBuffer(AllocatorPolymorphic allocator) {
		return allocator.allocator->GetAllocatedBuffer();
	}

	ECS_INLINE void FreeAllocatorFrom(AllocatorPolymorphic allocator_to_deallocate, AllocatorPolymorphic initial_allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
		allocator_to_deallocate.allocator->FreeFrom(initial_allocator.allocator, initial_allocator.allocation_type == ECS_ALLOCATION_MULTI, debug_info);
	}

	ECS_INLINE void* Reallocate(AllocatorPolymorphic allocator, const void* block, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (allocator.allocation_type == ECS_ALLOCATION_SINGLE) {
			return allocator.allocator->Reallocate(block, new_size, alignment, debug_info);
		}
		else {
			return allocator.allocator->ReallocateTs(block, new_size, alignment, debug_info);
		}
	}

	// This function handles the case ECS_MALLOC_ALLOCATOR is used, which will copy the data directly when the block is reallocated, and using
	// A manual memcpy on its data results in incorrect behavior
	ECSENGINE_API void* ReallocateWithCopy(AllocatorPolymorphic allocator, const void* block, size_t current_copy_size, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO);

	// This function handles the case ECS_MALLOC_ALLOCATOR is used, which will copy the data directly when the block is reallocated, and using
	// A manual memcpy on its data results in incorrect behavior. If the block is nullptr or the capacity is 0, then it will only make a new allocation,
	// Without trying a reallocation
	ECSENGINE_API void* ReallocateWithCopyNonNull(AllocatorPolymorphic allocator, const void* block, size_t capacity, size_t current_copy_size, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO);

	// Returns true if the block was deallocated, else false
	ECS_INLINE bool DeallocateIfBelongs(AllocatorPolymorphic allocator, const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (buffer == nullptr || !allocator.allocator->Belongs(buffer)) {
			return false;
		}
		if (allocator.allocation_type == ECS_ALLOCATION_SINGLE) {
			return allocator.allocator->DeallocateNoAssert(buffer, debug_info);
		}
		else {
			return allocator.allocator->DeallocateNoAssertTs(buffer, debug_info);
		}
	}

	ECS_INLINE bool DeallocateIfBelongsError(AllocatorPolymorphic allocator, const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (buffer == nullptr || !allocator.allocator->Belongs(buffer)) {
			return false;
		}
		if (allocator.allocation_type == ECS_ALLOCATION_SINGLE) {
			allocator.allocator->Deallocate(buffer, debug_info);
		}
		else {
			allocator.allocator->DeallocateTs(buffer, debug_info);
		}
		return true;
	}

	template<bool trigger_error_if_not_found>
	ECS_INLINE bool DeallocateIfBelongs(AllocatorPolymorphic allocator, const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
		if constexpr (trigger_error_if_not_found) {
			return DeallocateIfBelongsError(allocator, buffer, debug_info);
		}
		else {
			return DeallocateIfBelongs(allocator, buffer, debug_info);
		}
	}

	ECS_INLINE bool BelongsToAllocator(AllocatorPolymorphic allocator, const void* buffer) {
		return allocator.allocator->Belongs(buffer);
	}

	ECS_INLINE bool IsAllocatorEmpty(AllocatorPolymorphic allocator) {
		return allocator.allocator->IsEmpty();
	}

	ECS_INLINE void LockAllocator(AllocatorPolymorphic allocator) {
		allocator.allocator->Lock();
	}

	ECS_INLINE void UnlockAllocator(AllocatorPolymorphic allocator) {
		allocator.allocator->Unlock();
	}

	ECS_INLINE size_t GetAllocatorCurrentUsage(AllocatorPolymorphic allocator) {
		return allocator.allocator->GetCurrentUsage();
	}

	ECS_INLINE void ExitAllocatorProfilingMode(AllocatorPolymorphic allocator) {
		allocator.allocator->ExitProfilingMode();
	}

	ECS_INLINE void SetAllocatorProfilingMode(AllocatorPolymorphic allocator, const char* name) {
		allocator.allocator->SetProfilingMode(name);
	}

	ECS_INLINE size_t GetAllocatorRegions(AllocatorPolymorphic allocator, void** region_pointers, size_t* region_size, size_t pointer_capacity) {
		return allocator.allocator->GetRegions(region_pointers, region_size, pointer_capacity);
	}
	
	struct AllocatorMarker {
		size_t marker;
		// This field is needed by the resizable linear allocator
		size_t marker_usage;
	};

	// For allocators that support markers (i.e. linear/resizable linear), it will get the current
	// Allocator marker in order to restore it later on. For the other types of allocators, it doesn't do
	// Anything.
	ECSENGINE_API AllocatorMarker GetAllocatorMarker(AllocatorPolymorphic allocator);

	// Restores a marker that was recorded earlier (with function GetAllocatorMarker()). It doesn't do anything
	// For allocators that do not support markers.
	ECSENGINE_API void RestoreAllocatorMarker(AllocatorPolymorphic allocator, AllocatorMarker marker);

	// Returns the byte size of the allocator itself, (i.e. sizeof(LinearAllocator))
	ECSENGINE_API size_t AllocatorStructureByteSize(ECS_ALLOCATOR_TYPE type);

	// Only linear/stack/multipool/arena are considered base allocator types
	ECSENGINE_API size_t BaseAllocatorByteSize(ECS_ALLOCATOR_TYPE type);

	// The maximum allocation this allocator supports
	ECSENGINE_API size_t BaseAllocatorMaxAllocationSize(CreateBaseAllocatorInfo info);

	ECSENGINE_API size_t BaseAllocatorBufferSize(CreateBaseAllocatorInfo info);

	ECSENGINE_API size_t BaseAllocatorBufferSize(CreateBaseAllocatorInfo info, size_t count);

	// This is mostly intended to be used for profiling. It returns the number of total blocks
	// for multipool allocators. This allocator needs to be a fundamental one (i.e. linear, stack
	// multipool or arena)
	ECSENGINE_API size_t AllocatorPolymorphicBlockCount(AllocatorPolymorphic allocator);

	// Only linear/stack/multipool are considered base allocator types
	// The buffer_allocator is used to make the initial allocator and/or set as backup
	ECSENGINE_API void CreateBaseAllocator(AllocatorPolymorphic buffer_allocator, CreateBaseAllocatorInfo info, void* pointer_to_be_constructed);

	ECSENGINE_API void CreateBaseAllocator(void* buffer, CreateBaseAllocatorInfo info, void* pointer_to_be_constructed);

	ECSENGINE_API void CreateBaseAllocators(AllocatorPolymorphic buffer_allocator, CreateBaseAllocatorInfo info, void* pointers_to_be_constructed, size_t count);

	ECSENGINE_API void CreateBaseAllocators(void* buffer, CreateBaseAllocatorInfo info, void* pointers_to_be_constructed, size_t count);

	// Returns the appropriate allocator polymorphic for the given pointer and type. ECS_ALLOCATOR_TYPE_COUNT is not a valid value
	ECSENGINE_API AllocatorPolymorphic ConvertPointerToAllocatorPolymorphic(const void* pointer, ECS_ALLOCATOR_TYPE type);

	// This function is different from the basic one in the sense that it allows ECS_ALLOCATOR_TYPE_COUNT, in which case
	// It considers that the pointer is a pointer to an existing allocator polymorphic
	ECSENGINE_API AllocatorPolymorphic ConvertPointerToAllocatorPolymorphicEx(const void* pointer, ECS_ALLOCATOR_TYPE type);

	ECSENGINE_API void SetInternalImageAllocator(DirectX::ScratchImage* image, AllocatorPolymorphic allocator);

	// The returned string is a constant literal
	ECSENGINE_API const char* AllocatorTypeName(ECS_ALLOCATOR_TYPE type);

	template<typename T, typename... Args>
	ECS_INLINE T* AllocateAndConstruct(AllocatorPolymorphic allocator, Args... arguments) {
		T* allocation = (T*)Allocate(allocator, sizeof(T), alignof(T));
		if (allocation != nullptr) {
			new (allocation) T(arguments...);
		}
		return allocation;
	}

}