#include "ecspch.h"
#include "AllocatorPolymorphic.h"
#include "LinearAllocator.h"
#include "StackAllocator.h"
#include "MemoryArena.h"
#include "MemoryManager.h"
#include "MultipoolAllocator.h"
#include "PoolAllocator.h"
#include "StackAllocator.h"

namespace ECSEngine {

	using AllocateFunction = void* (*)(void* allocator, size_t size, size_t alignment);
	using DeallocateFunction = void (*)(void* allocator, const void* buffer);

	template<typename Allocator>
	void* AllocateFunctionAllocator(void* _allocator, size_t size, size_t alignment) {
		Allocator* allocator = (Allocator*)_allocator;
		return allocator->Allocate(size, alignment);
	}

	template<typename Allocator>
	void* AllocateFunctionAllocatorTs(void* _allocator, size_t size, size_t alignment) {
		Allocator* allocator = (Allocator*)_allocator;
		return allocator->Allocate_ts(size, alignment);
	}

	template<typename Allocator>
	void DeallocateFunctionAllocator(void* _allocator, const void* buffer) {
		Allocator* allocator = (Allocator*)_allocator;
		allocator->Deallocate(buffer);
	}

	template<typename Allocator>
	void DeallocateFunctionAllocatorTs(void* _allocator, const void* buffer) {
		Allocator* allocator = (Allocator*)_allocator;
		allocator->Deallocate_ts(buffer);
	}


	constexpr AllocateFunction ALLOCATE_FUNCTIONS[] = {
		AllocateFunctionAllocator<LinearAllocator>,
		AllocateFunctionAllocator<StackAllocator>,
		AllocateFunctionAllocator<PoolAllocator>,
		AllocateFunctionAllocator<MultipoolAllocator>,
		AllocateFunctionAllocator<MemoryManager>,
		AllocateFunctionAllocator<GlobalMemoryManager>,
		AllocateFunctionAllocator<MemoryArena>,
		AllocateFunctionAllocator<ResizableMemoryArena>
	};

	constexpr AllocateFunction ALLOCATE_TS_FUNCTIONS[] = {
		AllocateFunctionAllocatorTs<LinearAllocator>,
		AllocateFunctionAllocatorTs<StackAllocator>,
		AllocateFunctionAllocatorTs<PoolAllocator>,
		AllocateFunctionAllocatorTs<MultipoolAllocator>,
		AllocateFunctionAllocatorTs<MemoryManager>,
		AllocateFunctionAllocatorTs<GlobalMemoryManager>,
		AllocateFunctionAllocatorTs<MemoryArena>,
		AllocateFunctionAllocatorTs<ResizableMemoryArena>
	};

	constexpr DeallocateFunction DEALLOCATE_FUNCTIONS[] = {
		DeallocateFunctionAllocator<LinearAllocator>,
		DeallocateFunctionAllocator<StackAllocator>,
		DeallocateFunctionAllocator<PoolAllocator>,
		DeallocateFunctionAllocator<MultipoolAllocator>,
		DeallocateFunctionAllocator<MemoryManager>,
		DeallocateFunctionAllocator<GlobalMemoryManager>,
		DeallocateFunctionAllocator<MemoryArena>,
		DeallocateFunctionAllocator<ResizableMemoryArena>
	};

	constexpr DeallocateFunction DEALLOCATE_TS_FUNCTIONS[] = {
		DeallocateFunctionAllocatorTs<LinearAllocator>,
		DeallocateFunctionAllocatorTs<StackAllocator>,
		DeallocateFunctionAllocatorTs<PoolAllocator>,
		DeallocateFunctionAllocatorTs<MultipoolAllocator>,
		DeallocateFunctionAllocatorTs<MemoryManager>,
		DeallocateFunctionAllocatorTs<GlobalMemoryManager>,
		DeallocateFunctionAllocatorTs<MemoryArena>,
		DeallocateFunctionAllocatorTs<ResizableMemoryArena>
	};

	void* Allocate(void* allocator, AllocatorType type, size_t size, size_t alignment) {
		return ALLOCATE_FUNCTIONS[(unsigned int)type](allocator, size, alignment);
	}

	void* AllocateTs(void* allocator, AllocatorType type, size_t size, size_t alignment) {
		return ALLOCATE_TS_FUNCTIONS[(unsigned int)type](allocator, size, alignment);
	}

	void Deallocate(void* allocator, AllocatorType type, const void* buffer) {
		DEALLOCATE_FUNCTIONS[(unsigned int)type](allocator, buffer);
	}

	void DeallocateTs(void* allocator, AllocatorType type, const void* buffer) {
		DEALLOCATE_TS_FUNCTIONS[(unsigned int)type](allocator, buffer);
	}

}