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
#define ECS_MALLOCA_ALLOCATOR(size, stack_size, allocator) ((size) <= (stack_size) ? ECS_STACK_ALLOC(size) : ECSEngine::AllocateEx(allocator, size))
#define ECS_FREEA_ALLOCATOR(allocation, size, stack_size, allocator) do { if ((size) > (stack_size)) { ECSEngine::DeallocateEx(allocator, allocation); } } while (0) 

// It will allocate a buffer from the stack if it is bellow
// The threshold, else it will use the allocator. You must deallocate
// This using ECS_FREEA_ALLOCATOR in order to not leak the allocation
// It uses a default stack value of 64 ECS_KB
#define ECS_MALLOCA_ALLOCATOR_DEFAULT(size, allocator) ECS_MALLOCA_ALLOCATOR(size, ECS_KB * 64, allocator)
#define ECS_FREEA_ALLOCATOR_DEFAULT(allocation, size, allocator) ECS_FREEA_ALLOCATOR(allocation, size, ECS_KB * 64, allocator)

namespace ECSEngine {

	typedef void* (*AllocateFunction)(void* allocator, size_t size, size_t alignment, DebugInfo debug_info);

	// Some functions require that the interface does not expose the alignment
	typedef void* (*AllocateSizeFunction)(void* allocator, size_t size);

	typedef void (*DeallocateFunction)(void* allocator, const void* buffer, DebugInfo debug_info);

	// Some functions require that the interface has the buffer as non const variable
	typedef void (*DeallocateMutableFunction)(void* allocator, void* buffer);

	typedef bool (*DeallocateNoAssertFunction)(void* allocator, const void* buffer, DebugInfo debug_info);

	typedef bool (*BelongsToAllocatorFunction)(void* allocator, const void* buffer);

	typedef bool (*DeallocateIfBelongsFunction)(void* allocator, const void* buffer, DebugInfo debug_info);

	typedef void (*ClearAllocatorFunction)(void* allocator, DebugInfo debug_info);

	// Only for those suitable, like memory manager, global memory manager or resizable memory arena
	typedef void (*FreeAllocatorFunction)(void* allocator, DebugInfo debug_info);

	// Returns the allocated buffer for the given allocator.
	typedef void* (*GetAllocatorBufferFunction)(void* allocator);

	typedef void (*FreeAllocatorFromFunction)(void* allocator_to_deallocate, AllocatorPolymorphic initial_allocator);

	typedef void* (*ReallocateFunction)(void* allocator, const void* block, size_t size, size_t alignment, DebugInfo debug_info);

	typedef bool (*IsAllocatorEmptyFunction)(const void* allocator);

	// The current amount of bytes in use from the allocator
	typedef size_t (*GetAllocatorCurrentUsageFunction)(const void* allocator);

	typedef size_t (*GetAllocatorRegionsFunction)(const void* allocator, void** region_pointers, size_t* region_size, size_t pointer_capacity);

	ECSENGINE_API extern AllocateFunction ECS_ALLOCATE_FUNCTIONS[];

	ECSENGINE_API extern AllocateSizeFunction ECS_ALLOCATE_SIZE_FUNCTIONS[];

	ECSENGINE_API extern AllocateFunction ECS_ALLOCATE_TS_FUNCTIONS[];

	ECSENGINE_API extern AllocateSizeFunction ECS_ALLOCATE_SIZE_TS_FUNCTIONS[];

	ECSENGINE_API extern DeallocateFunction ECS_DEALLOCATE_FUNCTIONS[];

	ECSENGINE_API extern DeallocateNoAssertFunction ECS_DEALLOCATE_NO_ASSERT_FUNCTIONS[];

	ECSENGINE_API extern DeallocateMutableFunction ECS_DEALLOCATE_MUTABLE_FUNCTIONS[];

	ECSENGINE_API extern DeallocateFunction ECS_DEALLOCATE_TS_FUNCTIONS[];

	ECSENGINE_API extern DeallocateMutableFunction ECS_DEALLOCATE_MUTABLE_TS_FUNCTIONS[];

	ECSENGINE_API extern DeallocateNoAssertFunction ECS_DEALLOCATE_TS_NO_ASSERT_FUNCTIONS[];

	ECSENGINE_API extern BelongsToAllocatorFunction ECS_BELONGS_TO_ALLOCATOR_FUNCTIONS[];

	ECSENGINE_API extern DeallocateIfBelongsFunction ECS_DEALLOCATE_IF_BELONGS_FUNCTIONS[];

	ECSENGINE_API extern DeallocateIfBelongsFunction ECS_DEALLOCATE_IF_BELONGS_WITH_ERROR_FUNCTIONS[];

	ECSENGINE_API extern ClearAllocatorFunction ECS_CLEAR_ALLOCATOR_FUNCTIONS[];

	ECSENGINE_API extern FreeAllocatorFunction ECS_FREE_ALLOCATOR_FUNCTIONS[];

	ECSENGINE_API extern GetAllocatorBufferFunction ECS_GET_ALLOCATOR_BUFFER_FUNCTIONS[];

	ECSENGINE_API extern FreeAllocatorFromFunction ECS_FREE_ALLOCATOR_FROM_FUNCTIONS[];

	ECSENGINE_API extern ReallocateFunction ECS_REALLOCATE_FUNCTIONS[];

	ECSENGINE_API extern ReallocateFunction ECS_REALLOCATE_TS_FUNCTIONS[];

	ECSENGINE_API extern IsAllocatorEmptyFunction ECS_IS_ALLOCATOR_EMPTY_FUNCTIONS[];

	ECSENGINE_API extern GetAllocatorCurrentUsageFunction ECS_ALLOCATOR_CURRENT_USAGE_FUNCTIONS[];

	ECSENGINE_API extern GetAllocatorRegionsFunction ECS_ALLOCATOR_GET_REGIONS_FUNCTIONS[];

	// Single threaded
	ECS_INLINE void* Allocate(void* allocator, ECS_ALLOCATOR_TYPE type, size_t size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO) {
		return ECS_ALLOCATE_FUNCTIONS[(unsigned int)type](allocator, size, alignment, debug_info);
	}

	// Thread safe
	ECS_INLINE void* AllocateTs(void* allocator, ECS_ALLOCATOR_TYPE type, size_t size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO) {
		return ECS_ALLOCATE_TS_FUNCTIONS[(unsigned int)type](allocator, size, alignment, debug_info);
	}

	// Dynamic allocation type
	ECS_INLINE void* Allocate(
		void* allocator, 
		ECS_ALLOCATOR_TYPE allocator_type, 
		ECS_ALLOCATION_TYPE allocation_type, 
		size_t size, 
		size_t alignment = 8, 
		DebugInfo debug_info = ECS_DEBUG_INFO
	) {
		if (allocation_type == ECS_ALLOCATION_SINGLE) {
			return Allocate(allocator, allocator_type, size, alignment, debug_info);
		}
		else {
			return AllocateTs(allocator, allocator_type, size, alignment, debug_info);
		}
	}

	// Dynamic allocation type
	ECS_INLINE void* Allocate(AllocatorPolymorphic allocator, size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) {
		return Allocate(allocator.allocator, allocator.allocator_type, allocator.allocation_type, size, alignment, debug_info);
	}

	// Dynamic allocation type. This uses ECS_ALIGNED_MALLOC for the nullptr case! Can't deallocate with free!
	ECS_INLINE void* AllocateEx(AllocatorPolymorphic allocator, size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (allocator.allocator == nullptr) {
			return ECS_ALIGNED_MALLOC(size, alignment);
		}
		else {
			return Allocate(allocator, size, alignment, debug_info);
		}
	}

	// This uses the same malloc as the one from AllocateEx
	ECS_INLINE void* Malloc(size_t size, size_t alignment = alignof(void*)) {
		return ECS_ALIGNED_MALLOC(size, alignment);
	}

	// This uses ECS_ALIGNED_MALLOC for the nullptr case!Can't deallocate with free!
	ECS_INLINE void* AllocateTsEx(AllocatorPolymorphic allocator, size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (allocator.allocator == nullptr) {
			return Malloc(size, alignment);
		}
		else {
			return AllocateTs(allocator.allocator, allocator.allocator_type, size, alignment, debug_info);
		}
	}

	ECS_INLINE void ClearAllocator(AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
		ECS_CLEAR_ALLOCATOR_FUNCTIONS[allocator.allocator_type](allocator.allocator, debug_info);
	}

	// Single threaded
	ECS_INLINE void Deallocate(void* allocator, ECS_ALLOCATOR_TYPE type, const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
		ECS_DEALLOCATE_FUNCTIONS[(unsigned int)type](allocator, buffer, debug_info);
	}

	// Thread safe
	ECS_INLINE void DeallocateTs(void* allocator, ECS_ALLOCATOR_TYPE type, const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
		ECS_DEALLOCATE_TS_FUNCTIONS[(unsigned int)type](allocator, buffer, debug_info);
	}

	// Single threaded - doesn't assert if the block is not found
	ECS_INLINE bool DeallocateNoAssert(void* allocator, ECS_ALLOCATOR_TYPE type, const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
		return ECS_DEALLOCATE_NO_ASSERT_FUNCTIONS[(unsigned int)type](allocator, buffer, debug_info);
	}

	// Thread safe - doesn't assert if the block is not found
	ECS_INLINE bool DeallocateTsNoAssert(void* allocator, ECS_ALLOCATOR_TYPE type, const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
		return ECS_DEALLOCATE_TS_NO_ASSERT_FUNCTIONS[(unsigned int)type](allocator, buffer, debug_info);
	}

	// Dynamic allocation type
	ECS_INLINE void Deallocate(
		void* allocator, 
		ECS_ALLOCATOR_TYPE allocator_type, 
		const void* buffer, 
		ECS_ALLOCATION_TYPE allocation_type, 
		DebugInfo debug_info = ECS_DEBUG_INFO
	) {
		if (allocation_type == ECS_ALLOCATION_SINGLE) {
			Deallocate(allocator, allocator_type, buffer, debug_info);
		}
		else {
			DeallocateTs(allocator, allocator_type, buffer, debug_info);
		}
	}

	// Dynamic allocation type
	ECS_INLINE bool DeallocateNoAssert(
		void* allocator, 
		ECS_ALLOCATOR_TYPE allocator_type, 
		const void* buffer, 
		ECS_ALLOCATION_TYPE allocation_type,
		DebugInfo debug_info = ECS_DEBUG_INFO
	) {
		if (allocation_type == ECS_ALLOCATION_SINGLE) {
			DeallocateNoAssert(allocator, allocator_type, buffer, debug_info);
			return true;
		}
		else {
			return DeallocateTsNoAssert(allocator, allocator_type, buffer, debug_info);
		}
	}

	ECS_INLINE void Deallocate(AllocatorPolymorphic allocator, const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
		Deallocate(allocator.allocator, allocator.allocator_type, buffer, allocator.allocation_type, debug_info);
	}

	// Dynamic allocation type
	ECS_INLINE bool DeallocateNoAssert(AllocatorPolymorphic allocator, const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
		return DeallocateNoAssert(allocator.allocator, allocator.allocator_type, buffer, allocator.allocation_type, debug_info);
	}

	// This is a shortcut, it uses the correct free function to deallocate a block that was allocated
	// With AllocateEx when the allocator was nullptr
	ECS_INLINE void Free(void* buffer) {
		ECS_ALIGNED_FREE(buffer);
	}

	// Dynamic allocation type - if allocator.allocator is nullptr, then uses ECS_ALIGNED_FREE
	ECS_INLINE void DeallocateEx(AllocatorPolymorphic allocator, void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (allocator.allocator == nullptr) {
			Free(buffer);
		}
		else {
			Deallocate(allocator, buffer, debug_info);
		}
	}

	// If allocator.allocator is nullptr, then uses ECS_ALIGNED_FREE
	ECS_INLINE void DeallocateTsEx(AllocatorPolymorphic allocator, void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (allocator.allocator == nullptr) {
			ECS_ALIGNED_FREE(buffer);
		}
		else {
			DeallocateTs(allocator.allocator, allocator.allocator_type, buffer, debug_info);
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

	template<bool trigger_error_if_not_found = true>
	ECS_INLINE bool DeallocateTs(AllocatorPolymorphic allocator, const void* block, DebugInfo debug_info = ECS_DEBUG_INFO) {
		if constexpr (trigger_error_if_not_found) {
			DeallocateTs(allocator.allocator, allocator.allocator_type, block, debug_info);
			return true;
		}
		else {
			return DeallocateTsNoAssert(allocator.allocator, allocator.allocator_type, block, debug_info);
		}
	}
	
	// It makes sense only for MemoryManager, ResizableLinearAllocator
	// For all the other types it will do nothing
	ECS_INLINE void FreeAllocator(AllocatorPolymorphic allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
		ECS_FREE_ALLOCATOR_FUNCTIONS[allocator.allocator_type](allocator.allocator, debug_info);
	}

	// Returns the buffer from which the allocator was initialized. It doesn't make sense for the resizable allocators
	// (MemoryManager, ResizableLinearAllocator). It returns nullptr for these
	ECS_INLINE void* GetAllocatorBuffer(AllocatorPolymorphic allocator) {
		return ECS_GET_ALLOCATOR_BUFFER_FUNCTIONS[allocator.allocator_type](allocator.allocator);
	}

	// It finds the suitable to deallocate the memory allocated from the initial allocator
	ECS_INLINE void FreeAllocatorFrom(AllocatorPolymorphic allocator_to_deallocate, AllocatorPolymorphic initial_allocator) {
		ECS_FREE_ALLOCATOR_FROM_FUNCTIONS[allocator_to_deallocate.allocator_type](allocator_to_deallocate.allocator, initial_allocator);
	}

	ECS_INLINE void* Reallocate(AllocatorPolymorphic allocator, const void* block, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (allocator.allocation_type == ECS_ALLOCATION_SINGLE) {
			return ECS_REALLOCATE_FUNCTIONS[allocator.allocator_type](allocator.allocator, block, new_size, alignment, debug_info);
		}
		else {
			return ECS_REALLOCATE_TS_FUNCTIONS[allocator.allocator_type](allocator.allocator, block, new_size, alignment, debug_info);
		}
	}

	// Uses the same realloc function as ReallocateEx
	ECS_INLINE void* Realloc(void* block, size_t new_size, size_t alignment = alignof(void*)) {
		return ECS_ALIGNED_REALLOC(block, new_size, alignment);
	}

	// Uses ECS_ALIGNED_REALLOC for allocator.allocator. To deallocate you need to use ECS_ALIGNED_FREE!
	ECS_INLINE void* ReallocateEx(AllocatorPolymorphic allocator, void* block, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (allocator.allocator == nullptr) {
			return Realloc(block, new_size, alignment);
		}
		else {
			return Reallocate(allocator, block, new_size, alignment, debug_info);
		}
	}

	ECS_INLINE AllocateFunction GetAllocateFunction(ECS_ALLOCATOR_TYPE type, ECS_ALLOCATION_TYPE allocation_type = ECS_ALLOCATION_SINGLE) {
		if (allocation_type == ECS_ALLOCATION_SINGLE) {
			return ECS_ALLOCATE_FUNCTIONS[(unsigned int)type];
		}
		else {
			return ECS_ALLOCATE_TS_FUNCTIONS[(unsigned int)type];
		}
	}

	ECS_INLINE AllocateFunction GetAllocateFunction(AllocatorPolymorphic allocator) {
		return GetAllocateFunction(allocator.allocator_type, allocator.allocation_type);
	}

	ECS_INLINE AllocateSizeFunction GetAllocateSizeFunction(ECS_ALLOCATOR_TYPE type, ECS_ALLOCATION_TYPE allocation_type = ECS_ALLOCATION_SINGLE) {
		if (allocation_type == ECS_ALLOCATION_SINGLE) {
			return ECS_ALLOCATE_SIZE_FUNCTIONS[(unsigned int)type];
		}
		else {
			return ECS_ALLOCATE_SIZE_TS_FUNCTIONS[(unsigned int)type];
		}
	}

	ECS_INLINE AllocateSizeFunction GetAllocateSizeFunction(AllocatorPolymorphic allocator) {
		return GetAllocateSizeFunction(allocator.allocator_type, allocator.allocation_type);
	}

	ECS_INLINE DeallocateFunction GetDeallocateFunction(ECS_ALLOCATOR_TYPE type, ECS_ALLOCATION_TYPE allocation_type = ECS_ALLOCATION_SINGLE) {
		if (allocation_type == ECS_ALLOCATION_SINGLE) {
			return ECS_DEALLOCATE_FUNCTIONS[(unsigned int)type];
		}
		else {
			return ECS_DEALLOCATE_TS_FUNCTIONS[(unsigned int)type];
		}
	}

	ECS_INLINE DeallocateFunction GetDeallocateFunction(AllocatorPolymorphic allocator) {
		return GetDeallocateFunction(allocator.allocator_type, allocator.allocation_type);
	}

	ECS_INLINE DeallocateMutableFunction GetDeallocateMutableFunction(ECS_ALLOCATOR_TYPE type, ECS_ALLOCATION_TYPE allocation_type = ECS_ALLOCATION_SINGLE) {
		if (allocation_type == ECS_ALLOCATION_SINGLE) {
			return ECS_DEALLOCATE_MUTABLE_FUNCTIONS[(unsigned int)type];
		}
		else {
			return ECS_DEALLOCATE_MUTABLE_TS_FUNCTIONS[(unsigned int)type];
		}
	}

	ECS_INLINE DeallocateMutableFunction GetDeallocateMutableFunction(AllocatorPolymorphic allocator) {
		return GetDeallocateMutableFunction(allocator.allocator_type, allocator.allocation_type);
	}

	ECS_INLINE BelongsToAllocatorFunction GetBelongsToAllocatorFunction(ECS_ALLOCATOR_TYPE type) {
		return ECS_BELONGS_TO_ALLOCATOR_FUNCTIONS[(unsigned int)type];
	}

	ECS_INLINE bool BelongsToAllocator(AllocatorPolymorphic allocator, const void* buffer) {
		return ECS_BELONGS_TO_ALLOCATOR_FUNCTIONS[(unsigned int)allocator.allocator_type](allocator.allocator, buffer);
	}

	// Returns true if the block was deallocated, else false
	ECS_INLINE bool DeallocateIfBelongs(AllocatorPolymorphic allocator, const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (buffer == nullptr) {
			return false;
		}
		return ECS_DEALLOCATE_IF_BELONGS_FUNCTIONS[(unsigned int)allocator.allocator_type](allocator.allocator, buffer, debug_info);
	}

	ECS_INLINE bool DeallocateIfBelongsError(AllocatorPolymorphic allocator, const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (buffer == nullptr) {
			return false;
		}
		return ECS_DEALLOCATE_IF_BELONGS_WITH_ERROR_FUNCTIONS[(unsigned int)allocator.allocator_type](allocator.allocator, buffer, debug_info);
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
	
	ECS_INLINE bool IsAllocatorEmpty(AllocatorPolymorphic allocator) {
		return ECS_IS_ALLOCATOR_EMPTY_FUNCTIONS[allocator.allocator_type](allocator.allocator);
	}

	ECS_INLINE void LockAllocator(AllocatorPolymorphic allocator) {
		AllocatorBase* base_allocator = (AllocatorBase*)allocator.allocator;
		base_allocator->Lock();
	}

	ECS_INLINE void UnlockAllocator(AllocatorPolymorphic allocator) {
		AllocatorBase* base_allocator = (AllocatorBase*)allocator.allocator;
		base_allocator->Unlock();
	}

	ECS_INLINE size_t GetAllocatorCurrentUsage(AllocatorPolymorphic allocator) {
		return ECS_ALLOCATOR_CURRENT_USAGE_FUNCTIONS[allocator.allocator_type](allocator.allocator);
	}

	ECS_INLINE void ExitAllocatorProfilingMode(AllocatorPolymorphic allocator) {
		AllocatorBase* base_allocator = (AllocatorBase*)allocator.allocator;
		base_allocator->ExitProfilingMode();
	}

	ECS_INLINE void SetAllocatorProfilingMode(AllocatorPolymorphic allocator, const char* name) {
		AllocatorBase* base_allocator = (AllocatorBase*)allocator.allocator;
		base_allocator->SetProfilingMode(name);
	}

	ECS_INLINE size_t GetAllocatorRegions(AllocatorPolymorphic allocator, void** region_pointers, size_t* region_size, size_t pointer_capacity) {
		return ECS_ALLOCATOR_GET_REGIONS_FUNCTIONS[allocator.allocator_type](allocator.allocator, region_pointers, region_size, pointer_capacity);
	}

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

	struct LinearAllocator;
	struct StackAllocator;
	struct MultipoolAllocator;
	struct MemoryManager;
	struct MemoryArena;
	struct ResizableLinearAllocator;
	struct MemoryProtectedAllocator;

	ECSENGINE_API void SetInternalImageAllocator(DirectX::ScratchImage* image, AllocatorPolymorphic allocator);

	// The returned string is a constant literal
	ECSENGINE_API const char* AllocatorTypeName(ECS_ALLOCATOR_TYPE type);

	template<typename T, typename... Args>
	ECS_INLINE T* AllocateAndConstruct(AllocatorPolymorphic allocator, Args... arguments) {
		T* allocation = (T*)AllocateEx(allocator, sizeof(T), alignof(T));
		if (allocation != nullptr) {
			new (allocation) T(arguments...);
		}
		return allocation;
	}

}