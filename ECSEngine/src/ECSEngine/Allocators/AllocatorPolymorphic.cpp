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

	template<typename Allocator>
	void ClearAllocator(void* _allocator) {
		Allocator* allocator = (Allocator*)_allocator;
		allocator->Clear();
	}

	template<typename Allocator>
	void FreeAllocator(void* _allocator) {
		Allocator* allocator = (Allocator*)_allocator;
		allocator->Free();
	}

	// Do nothing for these types that do not support that
	void FreeAllocatorDummy(void* _allocator) {}

	template<typename Allocator>
	const void* GetAllocatorBuffer(void* _allocator) {
		Allocator* allocator = (Allocator*)_allocator;
		return allocator->GetAllocatedBuffer();
	}

	const void* GetAllocatorBufferDummy(void* _allocator) {
		return nullptr;
	}

	template<typename Allocator>
	void FreeAllocatorFromOtherResizable(void* _allocator, AllocatorPolymorphic initial_allocator) {
		Allocator* allocator = (Allocator*)_allocator;
		allocator->Free();
	}

	template<typename Allocator>
	void FreeAllocatorFromOtherWithBuffer(void* _allocator, AllocatorPolymorphic initial_allocator) {
		Allocator* allocator = (Allocator*)_allocator;
		Deallocate(initial_allocator, allocator->GetAllocatedBuffer());
	}

#define ECS_JUMP_TABLE(function_name)	function_name<LinearAllocator>, \
										function_name<StackAllocator>, \
										function_name<MultipoolAllocator>, \
										function_name<MemoryManager>, \
										function_name<GlobalMemoryManager>, \
										function_name<MemoryArena>, \
										function_name<ResizableMemoryArena>, \
										function_name<ResizableLinearAllocator>

#define ECS_JUMP_TABLE_FOR_RESIZABLE(resizable_function, fixed_function)	fixed_function, \
																			fixed_function, \
																			fixed_function, \
																			resizable_function<MemoryManager>, \
																			resizable_function<GlobalMemoryManager>, \
																			fixed_function, \
																			resizable_function<ResizableMemoryArena>, \
																			resizable_function<ResizableLinearAllocator>

#define ECS_JUMP_TABLE_FOR_FIXED(fixed_function, resizable_function)	fixed_function<LinearAllocator>, \
																		fixed_function<StackAllocator>, \
																		fixed_function<MultipoolAllocator>, \
																		resizable_function, \
																		resizable_function, \
																		fixed_function<MemoryArena>, \
																		resizable_function, \
																		resizable_function

#define ECS_JUMP_TABLE_FOR_FIXED_AND_RESIZABLE(fixed_function, resizable_function)	fixed_function<LinearAllocator>, \
																					fixed_function<StackAllocator>, \
																					fixed_function<MultipoolAllocator>, \
																					resizable_function<MemoryManager>, \
																					resizable_function<GlobalMemoryManager>, \
																					fixed_function<MemoryArena>, \
																					resizable_function<ResizableMemoryArena>, \
																					resizable_function<ResizableLinearAllocator>



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

	ClearAllocatorFunction ECS_CLEAR_ALLOCATOR_FUNCTIONS[] = {
		ECS_JUMP_TABLE(ClearAllocator)
	};

	FreeAllocatorFunction ECS_FREE_ALLOCATOR_FUNCTIONS[] = {
		ECS_JUMP_TABLE_FOR_RESIZABLE(FreeAllocator, FreeAllocatorDummy)
	};

	GetAllocatorBufferFunction ECS_GET_ALLOCATOR_BUFFER_FUNCTIONS[] = {
		ECS_JUMP_TABLE_FOR_FIXED(GetAllocatorBuffer, GetAllocatorBufferDummy)
	};

	FreeAllocatorFromFunction ECS_FREE_ALLOCATOR_FROM_FUNCTIONS[] = {
		ECS_JUMP_TABLE_FOR_FIXED_AND_RESIZABLE(FreeAllocatorFromOtherWithBuffer, FreeAllocatorFromOtherResizable)
	};

#undef ECS_JUMP_TABLE

    void SetInternalImageAllocator(DirectX::ScratchImage* image, AllocatorPolymorphic allocator)
    {
        if (allocator.allocator != nullptr) {
            image->SetAllocator(allocator.allocator, GetAllocateFunction(allocator), GetDeallocateMutableFunction(allocator));
        }
    }

}
