#include "ecspch.h"
#include "AllocatorPolymorphic.h"
#include "../../Dependencies/DirectXTex/DirectXTex/DirectXTex.h"
#include "LinearAllocator.h"
#include "StackAllocator.h"
#include "MemoryArena.h"
#include "MemoryManager.h"
#include "MultipoolAllocator.h"
#include "PoolAllocator.h"
#include "StackAllocator.h"
#include "ResizableLinearAllocator.h"

namespace ECSEngine {

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

	template<typename Allocator>
	bool BelongsToAllocator(void* ECS_RESTRICT _allocator, const void* ECS_RESTRICT buffer) {
		Allocator* allocator = (Allocator*)_allocator;
		return allocator->Belongs(buffer);
	}

#define ECS_JUMP_TABLE(function_name)	function_name<LinearAllocator>, \
										function_name<StackAllocator>, \
										function_name<MultipoolAllocator>, \
										function_name<MemoryManager>, \
										function_name<GlobalMemoryManager>, \
										function_name<MemoryArena>, \
										function_name<ResizableMemoryArena>, \
										function_name<ResizableLinearAllocator>

	AllocateFunction ECS_ALLOCATE_FUNCTIONS[] = {
		ECS_JUMP_TABLE(AllocateFunctionAllocator)
	};

	AllocateSizeFunction ECS_ALLOCATE_SIZE_FUNCTIONS[] = {
		ECS_JUMP_TABLE(AllocateFunctionSizeAllocator)
	};

	AllocateFunction ECS_ALLOCATE_TS_FUNCTIONS[] = {
		ECS_JUMP_TABLE(AllocateFunctionAllocatorTs)
	};

	AllocateSizeFunction ECS_ALLOCATE_SIZE_TS_FUNCTIONS[] = {
		ECS_JUMP_TABLE(AllocateFunctionSizeAllocatorTs)
	};

	DeallocateFunction ECS_DEALLOCATE_FUNCTIONS[] = {
		ECS_JUMP_TABLE(DeallocateFunctionAllocator)
	};

	DeallocateMutableFunction ECS_DEALLOCATE_MUTABLE_FUNCTIONS[] = {
		ECS_JUMP_TABLE(DeallocateMutableFunctionAllocator)
	};

	DeallocateFunction ECS_DEALLOCATE_TS_FUNCTIONS[] = {
		ECS_JUMP_TABLE(DeallocateFunctionAllocatorTs)
	};

	DeallocateMutableFunction ECS_DEALLOCATE_MUTABLE_TS_FUNCTIONS[] = {
		ECS_JUMP_TABLE(DeallocateMutableFunctionAllocatorTs)
	};

	BelongsToAllocatorFunction ECS_BELONGS_TO_ALLOCATOR_FUNCTIONS[] = {
		ECS_JUMP_TABLE(BelongsToAllocator)
	};

#undef ECS_JUMP_TABLE

    void SetInternalImageAllocator(DirectX::ScratchImage* image, AllocatorPolymorphic allocator)
    {
        if (allocator.allocator != nullptr) {
            image->SetAllocator(allocator.allocator, GetAllocateFunction(allocator), GetDeallocateMutableFunction(allocator));
        }
    }

	template<typename Allocator>
	AllocatorPolymorphic GetAllocatorPolymorphic(Allocator* allocator, ECS_ALLOCATION_TYPE allocation_type) {
		ECS_ALLOCATOR_TYPE allocator_type = ECS_ALLOCATOR_LINEAR;

		if constexpr (std::is_same_v<Allocator, LinearAllocator>) {
			allocator_type = ECS_ALLOCATOR_LINEAR;
		}
		else if constexpr (std::is_same_v<Allocator, StackAllocator>) {
			allocator_type = ECS_ALLOCATOR_STACK;
		}
		else if constexpr (std::is_same_v<Allocator, MultipoolAllocator>) {
			allocator_type = ECS_ALLOCATOR_MULTIPOOL;
		}
		else if constexpr (std::is_same_v<Allocator, MemoryManager>) {
			allocator_type = ECS_ALLOCATOR_MANAGER;
		}
		else if constexpr (std::is_same_v<Allocator, GlobalMemoryManager>) {
			allocator_type = ECS_ALLOCATOR_GLOBAL_MANAGER;
		}
		else if constexpr (std::is_same_v<Allocator, MemoryArena>) {
			allocator_type = ECS_ALLOCATOR_ARENA;
		}
		else if constexpr (std::is_same_v<Allocator, ResizableMemoryArena>) {
			allocator_type = ECS_ALLOCATOR_RESIZABLE_ARENA;
		}
		else if constexpr (std::is_same_v<Allocator, ResizableLinearAllocator>) {
			allocator_type = ECS_ALLOCATOR_RESIZABLE_LINEAR;
		}
		else {
			static_assert(false, "Incorrect allocator type for GetAllocatorPolymorphic");
		}

		return { allocator, allocator_type, allocation_type };
	}

	ECS_TEMPLATE_FUNCTION_ALLOCATOR_API(AllocatorPolymorphic, GetAllocatorPolymorphic, ECS_ALLOCATION_TYPE);

}
