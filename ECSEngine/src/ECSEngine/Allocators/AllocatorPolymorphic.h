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

	ECSENGINE_API extern AllocateFunction ECS_ALLOCATE_FUNCTIONS[];

	ECSENGINE_API extern AllocateSizeFunction ECS_ALLOCATE_SIZE_FUNCTIONS[];

	ECSENGINE_API extern AllocateFunction ECS_ALLOCATE_TS_FUNCTIONS[];

	ECSENGINE_API extern AllocateSizeFunction ECS_ALLOCATE_SIZE_TS_FUNCTIONS[];

	ECSENGINE_API extern DeallocateFunction ECS_DEALLOCATE_FUNCTIONS[];

	ECSENGINE_API extern DeallocateMutableFunction ECS_DEALLOCATE_MUTABLE_FUNCTIONS[];

	ECSENGINE_API extern DeallocateFunction ECS_DEALLOCATE_TS_FUNCTIONS[];

	ECSENGINE_API extern DeallocateMutableFunction ECS_DEALLOCATE_MUTABLE_TS_FUNCTIONS[];

	ECSENGINE_API extern BelongsToAllocatorFunction ECS_BELONGS_TO_ALLOCATOR_FUNCTIONS[];

	// Single threaded
	inline void* Allocate(void* allocator, ECS_ALLOCATOR_TYPE type, size_t size, size_t alignment = 8) {
		return ECS_ALLOCATE_FUNCTIONS[(unsigned int)type](allocator, size, alignment);
	}

	// Thread safe
	inline void* AllocateTs(void* allocator, ECS_ALLOCATOR_TYPE type, size_t size, size_t alignment = 8) {
		return ECS_ALLOCATE_TS_FUNCTIONS[(unsigned int)type](allocator, size, alignment);
	}

	// Dynamic allocation type
	inline void* Allocate(void* allocator, ECS_ALLOCATOR_TYPE allocator_type, ECS_ALLOCATION_TYPE allocation_type, size_t size, size_t alignment = 8) {
		if (allocation_type == ECS_ALLOCATION_TYPE::ECS_ALLOCATION_SINGLE) {
			return Allocate(allocator, allocator_type, size, alignment);
		}
		else {
			return AllocateTs(allocator, allocator_type, size, alignment);
		}
	}

	// Dynamic allocation type
	inline void* Allocate(AllocatorPolymorphic allocator, size_t size, size_t alignment = 8) {
		return Allocate(allocator.allocator, allocator.allocator_type, allocator.allocation_type, size, alignment);
	}

	// Dynamic allocation type
	inline void* AllocateEx(AllocatorPolymorphic allocator, size_t size) {
		if (allocator.allocator == nullptr) {
			return malloc(size);
		}
		else {
			return Allocate(allocator, size);
		}
	}

	inline AllocateFunction GetAllocateFunction(ECS_ALLOCATOR_TYPE type, ECS_ALLOCATION_TYPE allocation_type = ECS_ALLOCATION_TYPE::ECS_ALLOCATION_SINGLE) {
		if (allocation_type == ECS_ALLOCATION_TYPE::ECS_ALLOCATION_SINGLE) {
			return ECS_ALLOCATE_FUNCTIONS[(unsigned int)type];
		}
		else {
			return ECS_ALLOCATE_TS_FUNCTIONS[(unsigned int)type];
		}
	}

	inline AllocateFunction GetAllocateFunction(AllocatorPolymorphic allocator) {
		return GetAllocateFunction(allocator.allocator_type, allocator.allocation_type);
	}

	inline AllocateSizeFunction GetAllocateSizeFunction(ECS_ALLOCATOR_TYPE type, ECS_ALLOCATION_TYPE allocation_type = ECS_ALLOCATION_TYPE::ECS_ALLOCATION_SINGLE) {
		if (allocation_type == ECS_ALLOCATION_TYPE::ECS_ALLOCATION_SINGLE) {
			return ECS_ALLOCATE_SIZE_FUNCTIONS[(unsigned int)type];
		}
		else {
			return ECS_ALLOCATE_SIZE_TS_FUNCTIONS[(unsigned int)type];
		}
	}

	inline AllocateSizeFunction GetAllocateSizeFunction(AllocatorPolymorphic allocator) {
		return GetAllocateSizeFunction(allocator.allocator_type, allocator.allocation_type);
	}

	// Single threaded
	inline void Deallocate(void* ECS_RESTRICT allocator, ECS_ALLOCATOR_TYPE type, const void* ECS_RESTRICT buffer) {
		ECS_DEALLOCATE_FUNCTIONS[(unsigned int)type](allocator, buffer);
	}

	// Thread safe
	inline void DeallocateTs(void* ECS_RESTRICT allocator, ECS_ALLOCATOR_TYPE type, const void* ECS_RESTRICT buffer) {
		ECS_DEALLOCATE_TS_FUNCTIONS[(unsigned int)type](allocator, buffer);
	}

	// Dynamic allocation type
	inline void Deallocate(void* ECS_RESTRICT allocator, ECS_ALLOCATOR_TYPE allocator_type, const void* ECS_RESTRICT buffer, ECS_ALLOCATION_TYPE allocation_type) {
		if (allocation_type == ECS_ALLOCATION_TYPE::ECS_ALLOCATION_SINGLE) {
			Deallocate(allocator, allocator_type, buffer);
		}
		else {
			DeallocateTs(allocator, allocator_type, buffer);
		}
	}

	// Dynamic allocation type
	inline void Deallocate(AllocatorPolymorphic allocator, const void* ECS_RESTRICT buffer) {
		Deallocate(allocator.allocator, allocator.allocator_type, buffer, allocator.allocation_type);
	}

	// Dynamic allocation type - if allocator.allocator is nullptr then uses malloc
	inline void DeallocateEx(AllocatorPolymorphic allocator, void* buffer) {
		if (allocator.allocator == nullptr) {
			free(buffer);
		}
		else {
			Deallocate(allocator, buffer);
		}
	}

	inline DeallocateFunction GetDeallocateFunction(ECS_ALLOCATOR_TYPE type, ECS_ALLOCATION_TYPE allocation_type = ECS_ALLOCATION_SINGLE) {
		if (allocation_type == ECS_ALLOCATION_TYPE::ECS_ALLOCATION_SINGLE) {
			return ECS_DEALLOCATE_FUNCTIONS[(unsigned int)type];
		}
		else {
			return ECS_DEALLOCATE_TS_FUNCTIONS[(unsigned int)type];
		}
	}

	inline DeallocateFunction GetDeallocateFunction(AllocatorPolymorphic allocator) {
		return GetDeallocateFunction(allocator.allocator_type, allocator.allocation_type);
	}

	inline DeallocateMutableFunction GetDeallocateMutableFunction(ECS_ALLOCATOR_TYPE type, ECS_ALLOCATION_TYPE allocation_type = ECS_ALLOCATION_TYPE::ECS_ALLOCATION_SINGLE) {
		if (allocation_type == ECS_ALLOCATION_TYPE::ECS_ALLOCATION_SINGLE) {
			return ECS_DEALLOCATE_MUTABLE_FUNCTIONS[(unsigned int)type];
		}
		else {
			return ECS_DEALLOCATE_MUTABLE_TS_FUNCTIONS[(unsigned int)type];
		}
	}

	inline DeallocateMutableFunction GetDeallocateMutableFunction(AllocatorPolymorphic allocator) {
		return GetDeallocateMutableFunction(allocator.allocator_type, allocator.allocation_type);
	}

	inline BelongsToAllocatorFunction GetBelongsToAllocatorFunction(ECS_ALLOCATOR_TYPE type) {
		return ECS_BELONGS_TO_ALLOCATOR_FUNCTIONS[(unsigned int)type];
	}

	inline bool BelongsToAllocator(AllocatorPolymorphic allocator, const void* buffer) {
		return ECS_BELONGS_TO_ALLOCATOR_FUNCTIONS[(unsigned int)allocator.allocator_type](allocator.allocator, buffer);
	}

	// Returns true if the block was deallocated, else false
	inline bool DeallocateIfBelongs(AllocatorPolymorphic allocator, const void* buffer) {
		if (BelongsToAllocator(allocator, buffer)) {
			Deallocate(allocator, buffer);
			return true;
		}
		return false;
	}

	template<typename Allocator>
	ECSENGINE_API AllocatorPolymorphic GetAllocatorPolymorphic(Allocator* allocator, ECS_ALLOCATION_TYPE allocation_type = ECS_ALLOCATION_SINGLE);

	ECSENGINE_API void SetInternalImageAllocator(DirectX::ScratchImage* image, AllocatorPolymorphic allocator);

}