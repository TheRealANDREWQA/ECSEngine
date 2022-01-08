#pragma once
#include "../Core.h"
#include "AllocatorTypes.h"
#include "LinearAllocator.h"
#include "StackAllocator.h"
#include "MemoryArena.h"
#include "MemoryManager.h"
#include "MultipoolAllocator.h"
#include "PoolAllocator.h"
#include "StackAllocator.h"

namespace DirectX {
	class ScratchImage;
}

namespace ECSEngine {

	using AllocateFunction = void* (*)(void* allocator, size_t size, size_t alignment);
	// Some functions require that the interface does not expose the alignment
	using AllocateSizeFunction = void* (*)(void* allocator, size_t size);
	using DeallocateFunction = void (*)(void* allocator, const void* buffer);
	// Some functions require that the interface has the buffer as non const variable
	using DeallocateMutableFunction = void (*)(void* allocator, void* buffer);

	template<typename Allocator>
	void* AllocateFunctionAllocator(void* _allocator, size_t size, size_t alignment) {
		Allocator* allocator = (Allocator*)_allocator;
		return allocator->Allocate(size, alignment);
	}

	template<typename Allocator>
	void* AllocateFunctionSizeAllocator(void* _allocator, size_t size) {
		Allocator* allocator = (Allocator*)_allocator;
		return allocator->Allocate(size, 8);
	}

	template<typename Allocator>
	void* AllocateFunctionAllocatorTs(void* _allocator, size_t size, size_t alignment) {
		Allocator* allocator = (Allocator*)_allocator;
		return allocator->Allocate_ts(size, alignment);
	}

	template<typename Allocator>
	void* AllocateFunctionSizeAllocatorTs(void* _allocator, size_t size) {
		Allocator* allocator = (Allocator*)_allocator;
		return allocator->Allocate_ts(size, 8);
	}

	template<typename Allocator>
	void DeallocateFunctionAllocator(void* ECS_RESTRICT _allocator, const void* ECS_RESTRICT buffer) {
		Allocator* allocator = (Allocator*)_allocator;
		allocator->Deallocate(buffer);
	}

	template<typename Allocator>
	void DeallocateMutableFunctionAllocator(void* ECS_RESTRICT _allocator, void* ECS_RESTRICT buffer) {
		Allocator* allocator = (Allocator*)_allocator;
		allocator->Deallocate(buffer);
	}

	template<typename Allocator>
	void DeallocateFunctionAllocatorTs(void* ECS_RESTRICT _allocator, const void* ECS_RESTRICT buffer) {
		Allocator* allocator = (Allocator*)_allocator;
		allocator->Deallocate_ts(buffer);
	}

	template<typename Allocator>
	void DeallocateMutableFunctionAllocatorTs(void* ECS_RESTRICT _allocator, void* ECS_RESTRICT buffer) {
		Allocator* allocator = (Allocator*)_allocator;
		allocator->Deallocate_ts(buffer);
	}


	constexpr AllocateFunction ECS_ALLOCATE_FUNCTIONS[] = {
		AllocateFunctionAllocator<LinearAllocator>,
		AllocateFunctionAllocator<StackAllocator>,
		AllocateFunctionAllocator<PoolAllocator>,
		AllocateFunctionAllocator<MultipoolAllocator>,
		AllocateFunctionAllocator<MemoryManager>,
		AllocateFunctionAllocator<GlobalMemoryManager>,
		AllocateFunctionAllocator<MemoryArena>,
		AllocateFunctionAllocator<ResizableMemoryArena>
	};

	constexpr AllocateSizeFunction ECS_ALLOCATE_SIZE_FUNCTIONS[] = {
		AllocateFunctionSizeAllocator<LinearAllocator>,
		AllocateFunctionSizeAllocator<StackAllocator>,
		AllocateFunctionSizeAllocator<PoolAllocator>,
		AllocateFunctionSizeAllocator<MultipoolAllocator>,
		AllocateFunctionSizeAllocator<MemoryManager>,
		AllocateFunctionSizeAllocator<GlobalMemoryManager>,
		AllocateFunctionSizeAllocator<MemoryArena>,
		AllocateFunctionSizeAllocator<ResizableMemoryArena>
	};

	constexpr AllocateFunction ECS_ALLOCATE_TS_FUNCTIONS[] = {
		AllocateFunctionAllocatorTs<LinearAllocator>,
		AllocateFunctionAllocatorTs<StackAllocator>,
		AllocateFunctionAllocatorTs<PoolAllocator>,
		AllocateFunctionAllocatorTs<MultipoolAllocator>,
		AllocateFunctionAllocatorTs<MemoryManager>,
		AllocateFunctionAllocatorTs<GlobalMemoryManager>,
		AllocateFunctionAllocatorTs<MemoryArena>,
		AllocateFunctionAllocatorTs<ResizableMemoryArena>
	};

	constexpr AllocateSizeFunction ECS_ALLOCATE_SIZE_TS_FUNCTIONS[] = {
		AllocateFunctionSizeAllocatorTs<LinearAllocator>,
		AllocateFunctionSizeAllocatorTs<StackAllocator>,
		AllocateFunctionSizeAllocatorTs<PoolAllocator>,
		AllocateFunctionSizeAllocatorTs<MultipoolAllocator>,
		AllocateFunctionSizeAllocatorTs<MemoryManager>,
		AllocateFunctionSizeAllocatorTs<GlobalMemoryManager>,
		AllocateFunctionSizeAllocatorTs<MemoryArena>,
		AllocateFunctionSizeAllocatorTs<ResizableMemoryArena>
	};

	constexpr DeallocateFunction ECS_DEALLOCATE_FUNCTIONS[] = {
		DeallocateFunctionAllocator<LinearAllocator>,
		DeallocateFunctionAllocator<StackAllocator>,
		DeallocateFunctionAllocator<PoolAllocator>,
		DeallocateFunctionAllocator<MultipoolAllocator>,
		DeallocateFunctionAllocator<MemoryManager>,
		DeallocateFunctionAllocator<GlobalMemoryManager>,
		DeallocateFunctionAllocator<MemoryArena>,
		DeallocateFunctionAllocator<ResizableMemoryArena>
	};

	constexpr DeallocateMutableFunction ECS_DEALLOCATE_MUTABLE_FUNCTIONS[] = {
		DeallocateMutableFunctionAllocator<LinearAllocator>,
		DeallocateMutableFunctionAllocator<StackAllocator>,
		DeallocateMutableFunctionAllocator<PoolAllocator>,
		DeallocateMutableFunctionAllocator<MultipoolAllocator>,
		DeallocateMutableFunctionAllocator<MemoryManager>,
		DeallocateMutableFunctionAllocator<GlobalMemoryManager>,
		DeallocateMutableFunctionAllocator<MemoryArena>,
		DeallocateMutableFunctionAllocator<ResizableMemoryArena>
	};

	constexpr DeallocateFunction ECS_DEALLOCATE_TS_FUNCTIONS[] = {
		DeallocateFunctionAllocatorTs<LinearAllocator>,
		DeallocateFunctionAllocatorTs<StackAllocator>,
		DeallocateFunctionAllocatorTs<PoolAllocator>,
		DeallocateFunctionAllocatorTs<MultipoolAllocator>,
		DeallocateFunctionAllocatorTs<MemoryManager>,
		DeallocateFunctionAllocatorTs<GlobalMemoryManager>,
		DeallocateFunctionAllocatorTs<MemoryArena>,
		DeallocateFunctionAllocatorTs<ResizableMemoryArena>
	};

	constexpr DeallocateMutableFunction ECS_DEALLOCATE_MUTABLE_TS_FUNCTIONS[] = {
		DeallocateMutableFunctionAllocatorTs<LinearAllocator>,
		DeallocateMutableFunctionAllocatorTs<StackAllocator>,
		DeallocateMutableFunctionAllocatorTs<PoolAllocator>,
		DeallocateMutableFunctionAllocatorTs<MultipoolAllocator>,
		DeallocateMutableFunctionAllocatorTs<MemoryManager>,
		DeallocateMutableFunctionAllocatorTs<GlobalMemoryManager>,
		DeallocateMutableFunctionAllocatorTs<MemoryArena>,
		DeallocateMutableFunctionAllocatorTs<ResizableMemoryArena>
	};

	// Single threaded
	inline void* Allocate(void* allocator, AllocatorType type, size_t size, size_t alignment = 8) {
		return ECS_ALLOCATE_FUNCTIONS[(unsigned int)type](allocator, size, alignment);
	}

	// Thread safe
	inline void* AllocateTs(void* allocator, AllocatorType type, size_t size, size_t alignment = 8) {
		return ECS_ALLOCATE_TS_FUNCTIONS[(unsigned int)type](allocator, size, alignment);
	}

	// Dynamic allocation type
	inline void* Allocate(void* allocator, AllocatorType allocator_type, AllocationType allocation_type, size_t size, size_t alignment = 8) {
		if (allocation_type == AllocationType::SingleThreaded) {
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

	inline AllocateFunction GetAllocateFunction(AllocatorType type, AllocationType allocation_type = AllocationType::SingleThreaded) {
		if (allocation_type == AllocationType::SingleThreaded) {
			return ECS_ALLOCATE_FUNCTIONS[(unsigned int)type];
		}
		else {
			return ECS_ALLOCATE_TS_FUNCTIONS[(unsigned int)type];
		}
	}

	inline AllocateFunction GetAllocateFunction(AllocatorPolymorphic allocator) {
		return GetAllocateFunction(allocator.allocator_type, allocator.allocation_type);
	}

	inline AllocateSizeFunction GetAllocateSizeFunction(AllocatorType type, AllocationType allocation_type = AllocationType::SingleThreaded) {
		if (allocation_type == AllocationType::SingleThreaded) {
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
	inline void Deallocate(void* ECS_RESTRICT allocator, AllocatorType type, const void* ECS_RESTRICT buffer) {
		ECS_DEALLOCATE_FUNCTIONS[(unsigned int)type](allocator, buffer);
	}

	// Thread safe
	inline void DeallocateTs(void* ECS_RESTRICT allocator, AllocatorType type, const void* ECS_RESTRICT buffer) {
		ECS_DEALLOCATE_TS_FUNCTIONS[(unsigned int)type](allocator, buffer);
	}

	// Dynamic allocation type
	inline void Deallocate(void* ECS_RESTRICT allocator, AllocatorType allocator_type, const void* ECS_RESTRICT buffer, AllocationType allocation_type) {
		if (allocation_type == AllocationType::SingleThreaded) {
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

	inline DeallocateFunction GetDeallocateFunction(AllocatorType type, AllocationType allocation_type = AllocationType::SingleThreaded) {
		if (allocation_type == AllocationType::SingleThreaded) {
			return ECS_DEALLOCATE_FUNCTIONS[(unsigned int)type];
		}
		else {
			return ECS_DEALLOCATE_TS_FUNCTIONS[(unsigned int)type];
		}
	}

	inline DeallocateFunction GetDeallocateFunction(AllocatorPolymorphic allocator) {
		return GetDeallocateFunction(allocator.allocator_type, allocator.allocation_type);
	}

	inline DeallocateMutableFunction GetDeallocateMutableFunction(AllocatorType type, AllocationType allocation_type = AllocationType::SingleThreaded) {
		if (allocation_type == AllocationType::SingleThreaded) {
			return ECS_DEALLOCATE_MUTABLE_FUNCTIONS[(unsigned int)type];
		}
		else {
			return ECS_DEALLOCATE_MUTABLE_TS_FUNCTIONS[(unsigned int)type];
		}
	}

	inline DeallocateMutableFunction GetDeallocateMutableFunction(AllocatorPolymorphic allocator) {
		return GetDeallocateMutableFunction(allocator.allocator_type, allocator.allocation_type);
	}

	template<typename Allocator>
	AllocatorPolymorphic GetAllocatorPolymorphic(Allocator* allocator, AllocationType allocation_type = AllocationType::SingleThreaded) {
		AllocatorType allocator_type = AllocatorType::LinearAllocator;
		if constexpr (std::is_same_v<Allocator, LinearAllocator>) {
			allocator_type = AllocatorType::LinearAllocator;
		}
		else if constexpr (std::is_same_v<Allocator, StackAllocator>) {
			allocator_type = AllocatorType::StackAllocator;
		}
		else if constexpr (std::is_same_v<Allocator, PoolAllocator>) {
			allocator_type = AllocatorType::PoolAllocator;
		}
		else if constexpr (std::is_same_v<Allocator, MultipoolAllocator>) {
			allocator_type = AllocatorType::MultipoolAllocator;
		}
		else if constexpr (std::is_same_v<Allocator, MemoryManager>) {
			allocator_type = AllocatorType::MemoryManager;
		}
		else if constexpr (std::is_same_v<Allocator, GlobalMemoryManager>) {
			allocator_type = AllocatorType::GlobalMemoryManager;
		}
		else if constexpr (std::is_same_v<Allocator, MemoryArena>) {
			allocator_type = AllocatorType::MemoryArena;
		}
		else if constexpr (std::is_same_v<Allocator, ResizableMemoryArena>) {
			allocator_type = AllocatorType::ResizableMemoryArena;
		}

		return { allocator, allocator_type, allocation_type };
	}

	ECSENGINE_API void SetInternalImageAllocator(DirectX::ScratchImage* image, AllocatorPolymorphic allocator);

}