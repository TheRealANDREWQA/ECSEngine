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
#include "../Utilities/PointerUtilities.h"

namespace ECSEngine {

	template<typename Allocator>
	void* AllocateFunctionAllocator(void* _allocator, size_t size, size_t alignment, DebugInfo debug_info) {
		Allocator* allocator = (Allocator*)_allocator;
		return allocator->Allocate(size, alignment, debug_info);
	}

	template<typename Allocator>
	void* AllocateFunctionSizeAllocator(void* _allocator, size_t size) {
		Allocator* allocator = (Allocator*)_allocator;
		return allocator->Allocate(size, 8);
	}

	template<typename Allocator>
	void* AllocateFunctionAllocatorTs(void* _allocator, size_t size, size_t alignment, DebugInfo debug_info) {
		Allocator* allocator = (Allocator*)_allocator;
		return allocator->Allocate_ts(size, alignment, debug_info);
	}

	template<typename Allocator>
	void* AllocateFunctionSizeAllocatorTs(void* _allocator, size_t size) {
		Allocator* allocator = (Allocator*)_allocator;
		return allocator->Allocate_ts(size, 8);
	}

	template<typename Allocator>
	void DeallocateFunctionAllocator(void* _allocator, const void* buffer, DebugInfo debug_info) {
		Allocator* allocator = (Allocator*)_allocator;
		allocator->Deallocate(buffer, debug_info);
	}

	template<typename Allocator>
	void DeallocateMutableFunctionAllocator(void* _allocator, void* buffer) {
		Allocator* allocator = (Allocator*)_allocator;
		allocator->Deallocate(buffer);
	}

	template<typename Allocator>
	void DeallocateFunctionAllocatorTs(void* _allocator, const void* buffer, DebugInfo debug_info) {
		Allocator* allocator = (Allocator*)_allocator;
		allocator->Deallocate_ts(buffer, debug_info);
	}

	template<typename Allocator>
	bool DeallocateNoAssertFunctionAllocator(void* _allocator, const void* buffer, DebugInfo debug_info) {
		Allocator* allocator = (Allocator*)_allocator;
		return allocator->Deallocate<false>(buffer, debug_info);
	}

	template<typename Allocator>
	bool DeallocateTsNoAssertFunctionAllocator(void* _allocator, const void* buffer, DebugInfo debug_info) {
		Allocator* allocator = (Allocator*)_allocator;
		return allocator->Deallocate_ts<false>(buffer, debug_info);
	}

	template<typename Allocator>
	void DeallocateMutableFunctionAllocatorTs(void* _allocator, void* buffer) {
		Allocator* allocator = (Allocator*)_allocator;
		allocator->Deallocate_ts(buffer);
	}

	template<typename Allocator>
	bool BelongsToAllocator(void* _allocator, const void* buffer) {
		Allocator* allocator = (Allocator*)_allocator;
		return allocator->Belongs(buffer);
	}

	template<typename Allocator>
	bool DeallocateIfBelongsAllocator(void* _allocator, const void* buffer, DebugInfo debug_info) {
		Allocator* allocator = (Allocator*)_allocator;
		if (buffer != nullptr && allocator->Belongs(buffer)) {
			if constexpr (std::is_same_v<Allocator, LinearAllocator> || std::is_same_v<Allocator, StackAllocator>
				|| std::is_same_v<Allocator, ResizableLinearAllocator>) {
				// These are special cases of deallocation and for them it doesn't make sense - so handle it here
				allocator->Deallocate(buffer, debug_info);
				return true;
			}
			else {
				return allocator->Deallocate<false>(buffer, debug_info);
			}
		}
		return false;
	}

	template<typename Allocator>
	bool DeallocateIfBelongsErrorAllocator(void* _allocator, const void* buffer, DebugInfo debug_info) {
		Allocator* allocator = (Allocator*)_allocator;
		if (allocator->Belongs(buffer)) {
			if constexpr (std::is_same_v<Allocator, LinearAllocator> || std::is_same_v<Allocator, StackAllocator>
				|| std::is_same_v<Allocator, ResizableLinearAllocator>) {
				// These are special cases of deallocation and for them it doesn't make sense - so handle it here
				allocator->Deallocate(buffer, debug_info);
				return true;
			}
			else {
				return allocator->Deallocate(buffer, debug_info);
			}
		}
	}

	template<typename Allocator>
	void ClearAllocator(void* _allocator, DebugInfo debug_info) {
		Allocator* allocator = (Allocator*)_allocator;
		allocator->Clear(debug_info);
	}

	template<typename Allocator>
	void FreeAllocator(void* _allocator, DebugInfo debug_info) {
		Allocator* allocator = (Allocator*)_allocator;
		allocator->Free(debug_info);
	}

	// Do nothing for these types that do not support that
	void FreeAllocatorDummy(void* _allocator, DebugInfo debug_info) {}

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
		// The GetAllocatedBuffer returns a const void*, but free wants void* ...
		DeallocateEx(initial_allocator, (void*)allocator->GetAllocatedBuffer());
	}

	template<typename Allocator>
	void* ReallocateFunctionAllocator(void* _allocator, const void* block, size_t new_size, size_t alignment, DebugInfo debug_info) {
		Allocator* allocator = (Allocator*)_allocator;
		if constexpr (std::is_same_v<Allocator, LinearAllocator> || std::is_same_v<Allocator, StackAllocator>
			|| std::is_same_v<Allocator, ResizableLinearAllocator>) {
			return allocator->Allocate(new_size, alignment, debug_info);
		}
		else {
			return allocator->Reallocate(block, new_size, alignment, debug_info);
		}
	}

	template<typename Allocator>
	void* ReallocateFunctionAllocatorTs(void* _allocator, const void* block, size_t new_size, size_t alignment, DebugInfo debug_info) {
		Allocator* allocator = (Allocator*)_allocator;
		if constexpr (std::is_same_v<Allocator, LinearAllocator> || std::is_same_v<Allocator, StackAllocator>
			|| std::is_same_v<Allocator, ResizableLinearAllocator>) {
			return allocator->Allocate_ts(new_size, alignment, debug_info);
		}
		else {
			return allocator->Reallocate_ts(block, new_size, alignment, debug_info);
		}
	}

	template<typename Allocator>
	bool IsAllocatorEmptyFunctionAllocator(const void* _allocator) {
		const Allocator* allocator = (const Allocator*)_allocator;
		return allocator->IsEmpty();
	}

	template<typename Allocator>
	void LockAllocatorFunctionAllocator(void* _allocator) {
		Allocator* allocator = (Allocator*)_allocator;
		allocator->Lock();
	}

	template<typename Allocator>
	void UnlockAllocatorFunctionAllocator(void* _allocator) {
		Allocator* allocator = (Allocator*)_allocator;
		allocator->Unlock();
	}

#define ECS_JUMP_TABLE(function_name)	function_name<LinearAllocator>, \
										function_name<StackAllocator>, \
										function_name<MultipoolAllocator>, \
										function_name<MemoryManager>, \
										function_name<MemoryArena>, \
										function_name<ResizableLinearAllocator>

#define ECS_JUMP_TABLE_FOR_RESIZABLE(resizable_function, fixed_function)	fixed_function, \
																			fixed_function, \
																			fixed_function, \
																			resizable_function<MemoryManager>, \
																			fixed_function, \
																			resizable_function<ResizableLinearAllocator>

#define ECS_JUMP_TABLE_FOR_FIXED(fixed_function, resizable_function)	fixed_function<LinearAllocator>, \
																		fixed_function<StackAllocator>, \
																		fixed_function<MultipoolAllocator>, \
																		resizable_function, \
																		fixed_function<MemoryArena>, \
																		resizable_function

#define ECS_JUMP_TABLE_FOR_FIXED_AND_RESIZABLE(fixed_function, resizable_function)	fixed_function<LinearAllocator>, \
																					fixed_function<StackAllocator>, \
																					fixed_function<MultipoolAllocator>, \
																					resizable_function<MemoryManager>, \
																					fixed_function<MemoryArena>, \
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

	DeallocateNoAssertFunction ECS_DEALLOCATE_NO_ASSERT_FUNCTIONS[] = {
		ECS_JUMP_TABLE(DeallocateNoAssertFunctionAllocator)
	};

	DeallocateMutableFunction ECS_DEALLOCATE_MUTABLE_FUNCTIONS[] = {
		ECS_JUMP_TABLE(DeallocateMutableFunctionAllocator)
	};

	DeallocateFunction ECS_DEALLOCATE_TS_FUNCTIONS[] = {
		ECS_JUMP_TABLE(DeallocateFunctionAllocatorTs)
	};

	DeallocateNoAssertFunction ECS_DEALLOCATE_TS_NO_ASSERT_FUNCTIONS[] = {
		ECS_JUMP_TABLE(DeallocateTsNoAssertFunctionAllocator)
	};

	DeallocateMutableFunction ECS_DEALLOCATE_MUTABLE_TS_FUNCTIONS[] = {
		ECS_JUMP_TABLE(DeallocateMutableFunctionAllocatorTs)
	};

	BelongsToAllocatorFunction ECS_BELONGS_TO_ALLOCATOR_FUNCTIONS[] = {
		ECS_JUMP_TABLE(BelongsToAllocator)
	};

	DeallocateIfBelongsFunction ECS_DEALLOCATE_IF_BELONGS_FUNCTIONS[] = {
		ECS_JUMP_TABLE(DeallocateIfBelongsAllocator)
	};

	DeallocateIfBelongsFunction ECS_DEALLOCATE_IF_BELONGS_WITH_ERROR_FUNCTIONS[] = {
		ECS_JUMP_TABLE(DeallocateIfBelongsErrorAllocator)
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

	ReallocateFunction ECS_REALLOCATE_FUNCTIONS[] = {
		ECS_JUMP_TABLE(ReallocateFunctionAllocator)
	};

	ReallocateFunction ECS_REALLOCATE_TS_FUNCTIONS[] = {
		ECS_JUMP_TABLE(ReallocateFunctionAllocatorTs)
	};

	IsAllocatorEmptyFunction ECS_IS_ALLOCATOR_EMPTY_FUNCTIONS[] = {
		ECS_JUMP_TABLE(IsAllocatorEmptyFunctionAllocator)
	};

	LockAllocatorFunction ECS_LOCK_ALLOCATOR_FUNCTIONS[] = {
		ECS_JUMP_TABLE(LockAllocatorFunctionAllocator)
	};

	UnlockAllocatorFunction ECS_UNLOCK_ALLOCATOR_FUNCTIONS[] = {
		ECS_JUMP_TABLE(UnlockAllocatorFunctionAllocator)
	};

	size_t BaseAllocatorSize(ECS_ALLOCATOR_TYPE type)
	{
		switch (type) {
		case ECS_ALLOCATOR_LINEAR:
			return sizeof(LinearAllocator);
		case ECS_ALLOCATOR_STACK:
			return sizeof(StackAllocator);
		case ECS_ALLOCATOR_MULTIPOOL:
			return sizeof(MultipoolAllocator);
		case ECS_ALLOCATOR_ARENA:
			return sizeof(MemoryArena);
		}

		ECS_ASSERT(false, "Invalid base allocator type when getting the size");
		return -1;
	}

	size_t BaseAllocatorBufferSize(CreateBaseAllocatorInfo info)
	{
		switch (info.allocator_type) {
		case ECS_ALLOCATOR_LINEAR:
			return info.linear_capacity;
		case ECS_ALLOCATOR_STACK:
			return info.stack_capacity;
		case ECS_ALLOCATOR_MULTIPOOL:
			return MultipoolAllocator::MemoryOf(info.multipool_block_count, info.multipool_capacity);
		case ECS_ALLOCATOR_ARENA:
			return MemoryArena::MemoryOf(info.arena_allocator_count, info.arena_capacity, info.arena_nested_type);
		}

		ECS_ASSERT(false, "Invalid base allocator type when getting buffer allocation size");
		return -1;
	}

	size_t BaseAllocatorBufferSize(CreateBaseAllocatorInfo info, size_t count)
	{
		return BaseAllocatorBufferSize(info) * count;
	}

	void CreateBaseAllocator(AllocatorPolymorphic buffer_allocator, CreateBaseAllocatorInfo info, void* pointer_to_be_constructed)
	{
		void* allocation = AllocateEx(buffer_allocator, BaseAllocatorBufferSize(info));
		CreateBaseAllocator(allocation, info, pointer_to_be_constructed);
	}

	void CreateBaseAllocator(void* buffer, CreateBaseAllocatorInfo info, void* pointer_to_be_constructed)
	{
		switch (info.allocator_type) {
		case ECS_ALLOCATOR_LINEAR:
		{
			LinearAllocator* allocator = (LinearAllocator*)pointer_to_be_constructed;
			*allocator = LinearAllocator(buffer, info.linear_capacity);
		}
		break;
		case ECS_ALLOCATOR_STACK:
		{
			StackAllocator* allocator = (StackAllocator*)pointer_to_be_constructed;
			*allocator = StackAllocator(buffer, info.stack_capacity);
		}
		break;
		case ECS_ALLOCATOR_MULTIPOOL:
		{
			MultipoolAllocator* allocator = (MultipoolAllocator*)pointer_to_be_constructed;
			*allocator = MultipoolAllocator(buffer, info.multipool_capacity, info.multipool_block_count);
		}
		break;
		case ECS_ALLOCATOR_ARENA:
		{
			MemoryArena* allocator = (MemoryArena*)pointer_to_be_constructed;
			*allocator = MemoryArena(buffer, info.arena_allocator_count, info);
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid base allocator when constructing one");
		}
	}

	void CreateBaseAllocators(AllocatorPolymorphic buffer_allocator, CreateBaseAllocatorInfo info, void* pointers_to_be_constructed, size_t count)
	{
		// Add alignof(void*) such that we align each buffer to a maximal fundamental type boundary
		size_t allocation_size = BaseAllocatorBufferSize(info, count);
		void* allocation = AllocateEx(buffer_allocator, allocation_size);
		CreateBaseAllocators(allocation, info, pointers_to_be_constructed, count);
	}

	void CreateBaseAllocators(void* buffer, CreateBaseAllocatorInfo info, void* pointers_to_be_constructed, size_t count)
	{
		switch (info.allocator_type) {
		case ECS_ALLOCATOR_LINEAR:
		{
			LinearAllocator* allocators = (LinearAllocator*)pointers_to_be_constructed;
			for (size_t index = 0; index < count; index++) {
				allocators[index] = LinearAllocator(buffer, info.linear_capacity);
				buffer = OffsetPointer(buffer, info.linear_capacity);
			}
		}
		break;
		case ECS_ALLOCATOR_STACK:
		{
			StackAllocator* allocators = (StackAllocator*)pointers_to_be_constructed;
			for (size_t index = 0; index < count; index++) {
				allocators[index] = StackAllocator(buffer, info.stack_capacity);
				buffer = OffsetPointer(buffer, info.stack_capacity);
			}
		}
		break;
		case ECS_ALLOCATOR_MULTIPOOL:
		{
			size_t allocator_size = MultipoolAllocator::MemoryOf(info.multipool_block_count, info.multipool_capacity);
			MultipoolAllocator* allocators = (MultipoolAllocator*)pointers_to_be_constructed;
			for (size_t index = 0; index < count; index++) {
				allocators[index] = MultipoolAllocator(buffer, info.multipool_capacity, info.multipool_block_count);
				buffer = OffsetPointer(buffer, allocator_size);
			}
		}
		break;
		case ECS_ALLOCATOR_ARENA:
		{
			size_t allocator_size = MemoryArena::MemoryOf(info.arena_allocator_count, info.arena_capacity, info.arena_nested_type);
			MemoryArena* allocators = (MemoryArena*)pointers_to_be_constructed;
			for (size_t index = 0; index < count; index++) {
				allocators[index] = MemoryArena(buffer, info.arena_allocator_count, info);
				buffer = OffsetPointer(buffer, allocator_size);
			}
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid base allocator type when constructing multiple ones");
		}
	}

	template<typename Allocator>
	void* DirectXTexAllocator(void* _allocator, size_t size, size_t alignment) {
		Allocator* allocator = (Allocator*)_allocator;
		return allocator->Allocate(size, alignment);
	}

	DirectX::Tex_AllocateFunction DIRECTX_ALLOCATE_FUNCTIONS[] = {
		ECS_JUMP_TABLE(DirectXTexAllocator)
	};

	static_assert(std::size(DIRECTX_ALLOCATE_FUNCTIONS) == ECS_ALLOCATOR_TYPE_COUNT);

	void SetInternalImageAllocator(DirectX::ScratchImage* image, AllocatorPolymorphic allocator)
    {
        if (allocator.allocator != nullptr) {
			ECS_ASSERT(allocator.allocator_type < ECS_ALLOCATOR_TYPE_COUNT);
            image->SetAllocator(allocator.allocator, DIRECTX_ALLOCATE_FUNCTIONS[allocator.allocator_type], GetDeallocateMutableFunction(allocator));
        }
		else {
			image->SetAllocator(nullptr, DirectX::ECSDefaultAllocation, DirectX::ECSDefaultDeallocation);
		}
    }

	const char* ALLOCATOR_NAMES[] = {
		"Linear",
		"Stack",
		"Multipool",
		"Manager",
		"Arena",
		"ResizableLinear"
	};

	static_assert(std::size(ALLOCATOR_NAMES) == ECS_ALLOCATOR_TYPE_COUNT);

	const char* AllocatorTypeName(ECS_ALLOCATOR_TYPE type)
	{
		ECS_ASSERT(type < ECS_ALLOCATOR_TYPE_COUNT);
		return ALLOCATOR_NAMES[type];
	}

}
