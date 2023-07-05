#pragma once
#include "../Core.h"
#include "AllocatorTypes.h"
#include <malloc.h>

namespace DirectX {
	class ScratchImage;
}

namespace ECSEngine {

	typedef void* (*AllocateFunction)(void* allocator, size_t size, size_t alignment);

	// Some functions require that the interface does not expose the alignment
	typedef void* (*AllocateSizeFunction)(void* allocator, size_t size);

	typedef void (*DeallocateFunction)(void* allocator, const void* buffer);

	// Some functions require that the interface has the buffer as non const variable
	typedef void (*DeallocateMutableFunction)(void* allocator, void* buffer);

	typedef bool (*BelongsToAllocatorFunction)(void* allocator, const void* buffer);

	typedef bool (*DeallocateIfBelongsFunction)(void* allocator, const void* buffer);

	typedef void (*ClearAllocatorFunction)(void* allocator);

	// Only for those suitable, like memory manager, global memory manager or resizable memory arena
	typedef void (*FreeAllocatorFunction)(void* allocator);

	// Returns the allocated buffer for the given allocator.
	typedef const void* (*GetAllocatorBufferFunction)(void* allocator);

	typedef void (*FreeAllocatorFromFunction)(void* allocator_to_deallocate, AllocatorPolymorphic initial_allocator);

	typedef void* (*ReallocateFunction)(void* allocator, const void* block, size_t size, size_t alignment);

	ECSENGINE_API extern AllocateFunction ECS_ALLOCATE_FUNCTIONS[];

	ECSENGINE_API extern AllocateSizeFunction ECS_ALLOCATE_SIZE_FUNCTIONS[];

	ECSENGINE_API extern AllocateFunction ECS_ALLOCATE_TS_FUNCTIONS[];

	ECSENGINE_API extern AllocateSizeFunction ECS_ALLOCATE_SIZE_TS_FUNCTIONS[];

	ECSENGINE_API extern DeallocateFunction ECS_DEALLOCATE_FUNCTIONS[];

	ECSENGINE_API extern DeallocateMutableFunction ECS_DEALLOCATE_MUTABLE_FUNCTIONS[];

	ECSENGINE_API extern DeallocateFunction ECS_DEALLOCATE_TS_FUNCTIONS[];

	ECSENGINE_API extern DeallocateMutableFunction ECS_DEALLOCATE_MUTABLE_TS_FUNCTIONS[];

	ECSENGINE_API extern BelongsToAllocatorFunction ECS_BELONGS_TO_ALLOCATOR_FUNCTIONS[];

	ECSENGINE_API extern DeallocateIfBelongsFunction ECS_DEALLOCATE_IF_BELONGS_FUNCTIONS[];

	ECSENGINE_API extern ClearAllocatorFunction ECS_CLEAR_ALLOCATOR_FUNCTIONS[];

	ECSENGINE_API extern FreeAllocatorFunction ECS_FREE_ALLOCATOR_FUNCTIONS[];

	ECSENGINE_API extern GetAllocatorBufferFunction ECS_GET_ALLOCATOR_BUFFER_FUNCTIONS[];

	ECSENGINE_API extern FreeAllocatorFromFunction ECS_FREE_ALLOCATOR_FROM_FUNCTIONS[];

	ECSENGINE_API extern ReallocateFunction ECS_REALLOCATE_FUNCTIONS[];

	ECSENGINE_API extern ReallocateFunction ECS_REALLOCATE_TS_FUNCTIONS[];

	// Single threaded
	ECS_INLINE void* Allocate(void* allocator, ECS_ALLOCATOR_TYPE type, size_t size, size_t alignment = 8) {
		return ECS_ALLOCATE_FUNCTIONS[(unsigned int)type](allocator, size, alignment);
	}

	// Thread safe
	ECS_INLINE void* AllocateTs(void* allocator, ECS_ALLOCATOR_TYPE type, size_t size, size_t alignment = 8) {
		return ECS_ALLOCATE_TS_FUNCTIONS[(unsigned int)type](allocator, size, alignment);
	}

	// Dynamic allocation type
	ECS_INLINE void* Allocate(void* allocator, ECS_ALLOCATOR_TYPE allocator_type, ECS_ALLOCATION_TYPE allocation_type, size_t size, size_t alignment = 8) {
		if (allocation_type == ECS_ALLOCATION_SINGLE) {
			return Allocate(allocator, allocator_type, size, alignment);
		}
		else {
			return AllocateTs(allocator, allocator_type, size, alignment);
		}
	}

	// Dynamic allocation type
	ECS_INLINE void* Allocate(AllocatorPolymorphic allocator, size_t size, size_t alignment = 8) {
		return Allocate(allocator.allocator, allocator.allocator_type, allocator.allocation_type, size, alignment);
	}

	// Dynamic allocation type
	ECS_INLINE void* AllocateEx(AllocatorPolymorphic allocator, size_t size) {
		if (allocator.allocator == nullptr) {
			return malloc(size);
		}
		else {
			return Allocate(allocator, size);
		}
	}

	ECS_INLINE void ClearAllocator(AllocatorPolymorphic allocator) {
		ECS_CLEAR_ALLOCATOR_FUNCTIONS[allocator.allocator_type](allocator.allocator);
	}

	// Single threaded
	ECS_INLINE void Deallocate(void* ECS_RESTRICT allocator, ECS_ALLOCATOR_TYPE type, const void* ECS_RESTRICT buffer) {
		ECS_DEALLOCATE_FUNCTIONS[(unsigned int)type](allocator, buffer);
	}

	// Thread safe
	ECS_INLINE void DeallocateTs(void* ECS_RESTRICT allocator, ECS_ALLOCATOR_TYPE type, const void* ECS_RESTRICT buffer) {
		ECS_DEALLOCATE_TS_FUNCTIONS[(unsigned int)type](allocator, buffer);
	}

	// Dynamic allocation type
	ECS_INLINE void Deallocate(void* ECS_RESTRICT allocator, ECS_ALLOCATOR_TYPE allocator_type, const void* ECS_RESTRICT buffer, ECS_ALLOCATION_TYPE allocation_type) {
		if (allocation_type == ECS_ALLOCATION_SINGLE) {
			Deallocate(allocator, allocator_type, buffer);
		}
		else {
			DeallocateTs(allocator, allocator_type, buffer);
		}
	}

	// Dynamic allocation type
	ECS_INLINE void Deallocate(AllocatorPolymorphic allocator, const void* ECS_RESTRICT buffer) {
		Deallocate(allocator.allocator, allocator.allocator_type, buffer, allocator.allocation_type);
	}

	// Dynamic allocation type - if allocator.allocator is nullptr then uses malloc
	ECS_INLINE void DeallocateEx(AllocatorPolymorphic allocator, void* buffer) {
		if (allocator.allocator == nullptr) {
			free(buffer);
		}
		else {
			Deallocate(allocator, buffer);
		}
	}
	
	// It makes sense only for MemoryManager, GlobalMemoryManager, ResizableMemoryArena, ResizableLinearAllocator
	// For all the other types it will do nothing
	ECS_INLINE void FreeAllocator(AllocatorPolymorphic allocator) {
		ECS_FREE_ALLOCATOR_FUNCTIONS[allocator.allocation_type](allocator.allocator);
	}

	// Returns the buffer from which the allocator was initialized. It doesn't make sense for the resizable allocators
	// (MemoryManager, GlobalMemoryManager, ResizableMemoryArena, ResizableLinearAllocator). It returns nullptr for these
	ECS_INLINE const void* GetAllocatorBuffer(AllocatorPolymorphic allocator) {
		return ECS_GET_ALLOCATOR_BUFFER_FUNCTIONS[allocator.allocation_type](allocator.allocator);
	}

	// It finds the suitable to deallocate the memory allocated from the initial allocator
	ECS_INLINE void FreeAllocatorFrom(AllocatorPolymorphic allocator_to_deallocate, AllocatorPolymorphic initial_allocator) {
		ECS_FREE_ALLOCATOR_FROM_FUNCTIONS[allocator_to_deallocate.allocator_type](allocator_to_deallocate.allocator, initial_allocator);
	}

	ECS_INLINE void* Reallocate(AllocatorPolymorphic allocator, const void* block, size_t new_size, size_t alignment = 8) {
		if (allocator.allocation_type == ECS_ALLOCATION_SINGLE) {
			return ECS_REALLOCATE_FUNCTIONS[allocator.allocator_type](allocator.allocator, block, new_size, alignment);
		}
		else {
			return ECS_REALLOCATE_TS_FUNCTIONS[allocator.allocator_type](allocator.allocator, block, new_size, alignment);
		}
	}

	ECS_INLINE AllocateFunction GetAllocateFunction(ECS_ALLOCATOR_TYPE type, ECS_ALLOCATION_TYPE allocation_type = ECS_ALLOCATION_SINGLE) {
		if (allocation_type == ECS_ALLOCATION_TYPE::ECS_ALLOCATION_SINGLE) {
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
		if (allocation_type == ECS_ALLOCATION_TYPE::ECS_ALLOCATION_SINGLE) {
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
		if (allocation_type == ECS_ALLOCATION_TYPE::ECS_ALLOCATION_SINGLE) {
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
		if (allocation_type == ECS_ALLOCATION_TYPE::ECS_ALLOCATION_SINGLE) {
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
	ECS_INLINE bool DeallocateIfBelongs(AllocatorPolymorphic allocator, const void* buffer) {
		return ECS_DEALLOCATE_IF_BELONGS_FUNCTIONS[(unsigned int)allocator.allocator_type](allocator.allocator, buffer);
	}

	struct LinearAllocator;
	struct StackAllocator;
	struct MultipoolAllocator;
	struct MemoryManager;
	struct GlobalMemoryManager;
	struct MemoryArena;
	struct ResizableMemoryArena;
	struct ResizableLinearAllocator;

	template<typename Allocator>
	ECS_INLINE AllocatorPolymorphic GetAllocatorPolymorphic(Allocator* allocator, ECS_ALLOCATION_TYPE allocation_type = ECS_ALLOCATION_SINGLE) {
		ECS_ALLOCATOR_TYPE allocator_type = ECS_ALLOCATOR_LINEAR;

		if constexpr (std::is_same_v<std::remove_const_t<Allocator>, LinearAllocator>) {
			allocator_type = ECS_ALLOCATOR_LINEAR;
		}
		else if constexpr (std::is_same_v<std::remove_const_t<Allocator>, StackAllocator>) {
			allocator_type = ECS_ALLOCATOR_STACK;
		}
		else if constexpr (std::is_same_v<std::remove_const_t<Allocator>, MultipoolAllocator>) {
			allocator_type = ECS_ALLOCATOR_MULTIPOOL;
		}
		else if constexpr (std::is_same_v<std::remove_const_t<Allocator>, MemoryManager>) {
			allocator_type = ECS_ALLOCATOR_MANAGER;
		}
		else if constexpr (std::is_same_v<std::remove_const_t<Allocator>, GlobalMemoryManager>) {
			allocator_type = ECS_ALLOCATOR_GLOBAL_MANAGER;
		}
		else if constexpr (std::is_same_v<std::remove_const_t<Allocator>, MemoryArena>) {
			allocator_type = ECS_ALLOCATOR_ARENA;
		}
		else if constexpr (std::is_same_v<std::remove_const_t<Allocator>, ResizableMemoryArena>) {
			allocator_type = ECS_ALLOCATOR_RESIZABLE_ARENA;
		}
		else if constexpr (std::is_same_v<std::remove_const_t<Allocator>, ResizableLinearAllocator>) {
			allocator_type = ECS_ALLOCATOR_RESIZABLE_LINEAR;
		}
		else {
			static_assert(false, "Incorrect allocator type for GetAllocatorPolymorphic");
		}

		return { (void*)allocator, allocator_type, allocation_type };
	}

	ECSENGINE_API void SetInternalImageAllocator(DirectX::ScratchImage* image, AllocatorPolymorphic allocator);

}