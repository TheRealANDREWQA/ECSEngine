#include "ecspch.h"
#include "AllocatorPolymorphic.h"
#include "../../Dependencies/DirectXTex/DirectXTex/DirectXTex.h"
#include "LinearAllocator.h"
#include "StackAllocator.h"
#include "MemoryArena.h"
#include "MemoryManager.h"
#include "MultipoolAllocator.h"
#include "PoolAllocator.h"
#include "ResizableLinearAllocator.h"
#include "MemoryProtectedAllocator.h"
#include "MallocAllocator.h"
#include "../Utilities/PointerUtilities.h"

namespace ECSEngine {

	void* ReallocateWithCopy(AllocatorPolymorphic allocator, const void* block, size_t current_copy_size, size_t new_size, size_t alignment, DebugInfo debug_info)
	{
		if (new_size == 0) {
			// Deallocate the block and return nullptr
			Deallocate(allocator, block, debug_info);
			return nullptr;
		}

		void* reallocation = Reallocate(allocator, block, new_size, alignment, debug_info);
		if (reallocation != block && block != nullptr && allocator.allocator != ECS_MALLOC_ALLOCATOR) {
			memcpy(reallocation, block, min(current_copy_size, new_size));
		}
		return reallocation;
	}

	void* ReallocateWithCopyNonNull(AllocatorPolymorphic allocator, const void* block, size_t capacity, size_t current_copy_size, size_t new_size, size_t alignment, DebugInfo debug_info) {
		if (block != nullptr && capacity > 0) {
			return ReallocateWithCopy(allocator, block, current_copy_size, new_size, alignment, debug_info);
		}
		else {
			return Allocate(allocator, new_size, alignment, debug_info);
		}
	}

	AllocatorMarker GetAllocatorMarker(AllocatorPolymorphic allocator) {
		switch (allocator.allocator->m_allocator_type) {
		case ECS_ALLOCATOR_LINEAR:
		{
			LinearAllocator* linear_allocator = (LinearAllocator*)allocator.allocator;
			return { linear_allocator->GetMarker(), 0 };
		}
		case ECS_ALLOCATOR_RESIZABLE_LINEAR:
		{
			ResizableLinearAllocator* linear_allocator = (ResizableLinearAllocator*)allocator.allocator;
			AllocatorMarker marker;
			linear_allocator->GetMarker(&marker.marker, &marker.marker_usage);
			return marker;
		}
		default:
			return { 0, 0 };
		}

		return { 0,0 };
	}

	void RestoreAllocatorMarker(AllocatorPolymorphic allocator, AllocatorMarker marker) {
		switch (allocator.allocator->m_allocator_type) {
		case ECS_ALLOCATOR_LINEAR:
		{
			LinearAllocator* linear_allocator = (LinearAllocator*)allocator.allocator;
			linear_allocator->ReturnToMarker(marker.marker);
		}
		break;
		case ECS_ALLOCATOR_RESIZABLE_LINEAR:
		{
			ResizableLinearAllocator* linear_allocator = (ResizableLinearAllocator*)allocator.allocator;
			linear_allocator->ReturnToMarker(marker.marker, marker.marker_usage);
		}
		default:
			break;
		}
	}

	size_t AllocatorStructureByteSize(ECS_ALLOCATOR_TYPE type) {
		switch (type) {
		case ECS_ALLOCATOR_LINEAR:
			return sizeof(LinearAllocator);
		case ECS_ALLOCATOR_STACK:
			return sizeof(StackAllocator);
		case ECS_ALLOCATOR_MULTIPOOL:
			return sizeof(MultipoolAllocator);
		case ECS_ALLOCATOR_MANAGER:
			return sizeof(MemoryManager);
		case ECS_ALLOCATOR_ARENA:
			return sizeof(MemoryArena);
		case ECS_ALLOCATOR_RESIZABLE_LINEAR:
			return sizeof(ResizableLinearAllocator);
		case ECS_ALLOCATOR_MEMORY_PROTECTED:
			return sizeof(MemoryProtectedAllocator);
		}

		ECS_ASSERT(false, "Invalid allocator type when getting structure byte size");
		return -1;
	}

	size_t BaseAllocatorByteSize(ECS_ALLOCATOR_TYPE type)
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

	size_t BaseAllocatorMaxAllocationSize(CreateBaseAllocatorInfo info)
	{
		switch (info.allocator_type) {
		case ECS_ALLOCATOR_LINEAR:
			return info.linear_capacity;
		case ECS_ALLOCATOR_STACK:
			return info.stack_capacity;
		case ECS_ALLOCATOR_MULTIPOOL:
			return info.multipool_capacity;
		case ECS_ALLOCATOR_ARENA: {
			CreateBaseAllocatorInfo nested_info;
			nested_info.allocator_type = info.arena_nested_type;
			switch (nested_info.allocator_type) {
			case ECS_ALLOCATOR_LINEAR:
				nested_info.linear_capacity = info.arena_capacity;
				break;
			case ECS_ALLOCATOR_STACK:
				nested_info.stack_capacity = info.arena_capacity;
				break;
			case ECS_ALLOCATOR_MULTIPOOL:
				nested_info.multipool_block_count = info.arena_capacity;
				nested_info.multipool_block_count = info.arena_multipool_block_count;
				break;
			default:
				ECS_ASSERT(false, "Invalid arena nested allocator type");
			}
			return BaseAllocatorMaxAllocationSize(nested_info);
		}
		}

		ECS_ASSERT(false, "Invalid base allocator type when getting max allocation size");
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

	size_t AllocatorPolymorphicBlockCount(AllocatorPolymorphic allocator)
	{
		if (allocator.allocator->m_allocator_type == ECS_ALLOCATOR_MULTIPOOL) {
			const MultipoolAllocator* multipool_allocator = (const MultipoolAllocator*)allocator.allocator;
			return multipool_allocator->GetBlockCount();
		}
		else if (allocator.allocator->m_allocator_type == ECS_ALLOCATOR_ARENA) {
			// For arenas that have multipool allocators as base, we are interested in the total
			// Sum of the block count
			const MemoryArena* arena = (const MemoryArena*)allocator.allocator;
			if (arena->m_base_allocator_type == ECS_ALLOCATOR_MULTIPOOL) {
				size_t count = 0;
				for (unsigned char index = 0; index < arena->m_allocator_count; index++) {
					const MultipoolAllocator* current_allocator = (const MultipoolAllocator*)arena->GetAllocator(index).allocator;
					count += (size_t)current_allocator->GetBlockCount();
				}
				return count;
			}
		}

		return 0;
	}

	void CreateBaseAllocator(AllocatorPolymorphic buffer_allocator, CreateBaseAllocatorInfo info, void* pointer_to_be_constructed)
	{
		void* allocation = Allocate(buffer_allocator, BaseAllocatorBufferSize(info));
		CreateBaseAllocator(allocation, info, pointer_to_be_constructed);
	}

	void CreateBaseAllocator(void* buffer, CreateBaseAllocatorInfo info, void* pointer_to_be_constructed)
	{
		switch (info.allocator_type) {
		case ECS_ALLOCATOR_LINEAR:
		{
			LinearAllocator* allocator = (LinearAllocator*)pointer_to_be_constructed;
			new (allocator) LinearAllocator(buffer, info.linear_capacity);
		}
		break;
		case ECS_ALLOCATOR_STACK:
		{
			StackAllocator* allocator = (StackAllocator*)pointer_to_be_constructed;
			new (allocator) StackAllocator(buffer, info.stack_capacity);
		}
		break;
		case ECS_ALLOCATOR_MULTIPOOL:
		{
			MultipoolAllocator* allocator = (MultipoolAllocator*)pointer_to_be_constructed;
			new (allocator) MultipoolAllocator(buffer, info.multipool_capacity, info.multipool_block_count);
		}
		break;
		case ECS_ALLOCATOR_ARENA:
		{
			MemoryArena* allocator = (MemoryArena*)pointer_to_be_constructed;
			new (allocator) MemoryArena(buffer, info.arena_allocator_count, info);
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
		void* allocation = Allocate(buffer_allocator, allocation_size);
		CreateBaseAllocators(allocation, info, pointers_to_be_constructed, count);
	}

	void CreateBaseAllocators(void* buffer, CreateBaseAllocatorInfo info, void* pointers_to_be_constructed, size_t count)
	{
		switch (info.allocator_type) {
		case ECS_ALLOCATOR_LINEAR:
		{
			LinearAllocator* allocators = (LinearAllocator*)pointers_to_be_constructed;
			for (size_t index = 0; index < count; index++) {
				new (allocators + index) LinearAllocator(buffer, info.linear_capacity);
				buffer = OffsetPointer(buffer, info.linear_capacity);
			}
		}
		break;
		case ECS_ALLOCATOR_STACK:
		{
			StackAllocator* allocators = (StackAllocator*)pointers_to_be_constructed;
			for (size_t index = 0; index < count; index++) {
				new (allocators + index) StackAllocator(buffer, info.stack_capacity);
				buffer = OffsetPointer(buffer, info.stack_capacity);
			}
		}
		break;
		case ECS_ALLOCATOR_MULTIPOOL:
		{
			size_t allocator_size = MultipoolAllocator::MemoryOf(info.multipool_block_count, info.multipool_capacity);
			MultipoolAllocator* allocators = (MultipoolAllocator*)pointers_to_be_constructed;
			for (size_t index = 0; index < count; index++) {
				new (allocators + index) MultipoolAllocator(buffer, info.multipool_capacity, info.multipool_block_count);
				buffer = OffsetPointer(buffer, allocator_size);
			}
		}
		break;
		case ECS_ALLOCATOR_ARENA:
		{
			size_t allocator_size = MemoryArena::MemoryOf(info.arena_allocator_count, info.arena_capacity, info.arena_nested_type);
			MemoryArena* allocators = (MemoryArena*)pointers_to_be_constructed;
			for (size_t index = 0; index < count; index++) {
				new (allocators + index) MemoryArena(buffer, info.arena_allocator_count, info);
				buffer = OffsetPointer(buffer, allocator_size);
			}
		}
		break;
		default:
			ECS_ASSERT(false, "Invalid base allocator type when constructing multiple ones");
		}
	}

	AllocatorPolymorphic ConvertPointerToAllocatorPolymorphic(const void* pointer, ECS_ALLOCATOR_TYPE type) {
		/*switch (type) {
		case ECS_ALLOCATOR_LINEAR:
			return (LinearAllocator*)pointer;
		case ECS_ALLOCATOR_STACK:
			return (StackAllocator*)pointer;
		case ECS_ALLOCATOR_MULTIPOOL:
			return (MultipoolAllocator*)pointer;
		case ECS_ALLOCATOR_ARENA:
			return (MemoryArena*)pointer;
		case ECS_ALLOCATOR_MANAGER:
			return (MemoryManager*)pointer;
		case ECS_ALLOCATOR_RESIZABLE_LINEAR:
			return (ResizableLinearAllocator*)pointer;
		case ECS_ALLOCATOR_MEMORY_PROTECTED:
			return (MemoryProtectedAllocator*)pointer;
		default:
			ECS_ASSERT(false, "Invalid allocator type for pointer conversion to polymorphic allocator");
		}*/

		// No need to type cast the pointers
		return AllocatorPolymorphic((AllocatorBase*)pointer);
	}

	AllocatorPolymorphic ConvertPointerToAllocatorPolymorphicEx(const void* pointer, ECS_ALLOCATOR_TYPE type) {
		if (type == ECS_ALLOCATOR_TYPE_COUNT) {
			return *(AllocatorPolymorphic*)pointer;
		}

		return ConvertPointerToAllocatorPolymorphic(pointer, type);
	}

	static void* DirectX_Allocate(void* allocator, size_t size, size_t alignment) {
		return ((AllocatorBase*)allocator)->Allocate(size, alignment);
	}

	static void DirectX_Deallocate(void* allocator, void* buffer) {
		((AllocatorBase*)allocator)->Deallocate(buffer);
	}

	void SetInternalImageAllocator(DirectX::ScratchImage* image, AllocatorPolymorphic allocator)
    {
        if (allocator.allocator != nullptr) {
            image->SetAllocator(allocator.allocator, DirectX_Allocate, DirectX_Deallocate);
        }
		else {
			image->SetAllocator(nullptr, DirectX::ECSDefaultAllocation, DirectX::ECSDefaultDeallocation);
		}
    }

	const char* ALLOCATOR_NAMES[] = {
		STRING(LinearAllocator),
		STRING(StackAllocator),
		STRING(MultipoolAllocator),
		STRING(MemoryManager),
		STRING(MemoryArena),
		STRING(ResizableLinearAllocator),
		STRING(MemoryProtectedAllocator),
		STRING(MallocAllocator),
		STRING(InterfaceAllocator)
	};

	static_assert(ECS_COUNTOF(ALLOCATOR_NAMES) == ECS_ALLOCATOR_TYPE_COUNT);

	const char* AllocatorTypeName(ECS_ALLOCATOR_TYPE type)
	{
		ECS_ASSERT(type < ECS_ALLOCATOR_TYPE_COUNT);
		return ALLOCATOR_NAMES[type];
	}

	ECS_ALLOCATOR_TYPE AllocatorTypeFromString(const char* string, size_t string_size) {
		for (size_t index = 0; index < ECS_COUNTOF(ALLOCATOR_NAMES); index++) {
			if (strlen(ALLOCATOR_NAMES[index]) == string_size && memcmp(string, ALLOCATOR_NAMES[index], string_size * sizeof(char)) == 0) {
				return (ECS_ALLOCATOR_TYPE)index;
			}
		}
		return ECS_ALLOCATOR_TYPE_COUNT;
	}

	void WriteAllocatorTypeToString(ECS_ALLOCATOR_TYPE type, char* string, unsigned int& string_size, unsigned int string_capacity) {
		unsigned int type_string_size = (unsigned int)strlen(ALLOCATOR_NAMES[type]);
		ECS_ASSERT(type_string_size + string_size <= string_capacity, "Allocator type string does not fit into the given memory!");
		memcpy(string + string_size, ALLOCATOR_NAMES[type], type_string_size * sizeof(char));
		string_size += type_string_size;
	}

	Copyable* Copyable::Copy(AllocatorPolymorphic allocator) const {
		// Allocate the base data
		Copyable* allocation = (Copyable*)Allocate(allocator, sizeof(Copyable) + byte_size);
		// This will correctly copy the virtual table pointer
		memcpy(allocation, this, sizeof(Copyable));
		allocation->CopyImpl(this, allocator);
		return allocation;
	}

	Copyable* Copyable::CopyTo(uintptr_t& ptr) const {
		// We can simulate an allocator as linear one
		// Because the user should have enough pointer
		// Data allocated for us, we set the capacity to maximum 64 bit value
		// But we cannot use LinearAllocator directly, since depending on the way
		// The copy allocator version makes the allocation calls, it might request 
		// a bigger alignment for some time, and the copy size function does not account for that.
		// So, we need to make a custom wrapper around a linear allocator, that only calls with 
		// An alignment of 1.

		// Can't inherit from LinearAllocator, in order to keep it final.
		// Just use a field here and forward to it
		struct LinearAllocatorWrapper final : AllocatorBase {
			ECS_INLINE LinearAllocatorWrapper(void* buffer, size_t capacity) : AllocatorBase(ECS_ALLOCATOR_INTERFACE), allocator(buffer, capacity) {}

			ECS_INLINE virtual void* Allocate(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override {
				return allocator.Allocate(size, alignof(char), debug_info);
			}

			ECS_INLINE virtual bool DeallocateNoAssert(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO) override {
				return allocator.DeallocateNoAssert(block, debug_info);
			}

			ECS_INLINE virtual void* Reallocate(const void* buffer, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) override {
				// Can't deallocate previous buffers, try to allocate a new one
				return Allocate(new_size, alignment, debug_info);
			}

			ECS_INLINE virtual void Clear(DebugInfo debug_info = ECS_DEBUG_INFO) override {
				allocator.Clear(debug_info);
			}

			ECS_INLINE virtual bool Belongs(const void* buffer) const override {
				return allocator.Belongs(buffer);
			}

			ECS_INLINE virtual bool IsEmpty() const override {
				return allocator.IsEmpty();
			}

			ECS_INLINE virtual void Free(bool assert_that_is_standalone = false, DebugInfo debug_info = ECS_DEBUG_INFO) override {
				allocator.Free(assert_that_is_standalone, debug_info);
			}

			ECS_INLINE virtual void FreeFrom(AllocatorBase* backup_allocator, bool multithreaded_deallocation, DebugInfo debug_info = ECS_DEBUG_INFO) override {
				allocator.FreeFrom(backup_allocator, multithreaded_deallocation, debug_info);
			}

			ECS_INLINE virtual size_t GetCurrentUsage() const override {
				return allocator.GetCurrentUsage();
			}

			ECS_INLINE virtual void* GetAllocatedBuffer() const override {
				return allocator.GetAllocatedBuffer();
			}

			ECS_INLINE virtual size_t GetRegions(void** region_start, size_t* region_size, size_t pointer_capacity) const override {
				return allocator.GetRegions(region_start, region_size, pointer_capacity);
			}

			LinearAllocator allocator;
		};

		LinearAllocatorWrapper allocator((void*)ptr, ULLONG_MAX);
		Copyable* copy = Copy(&allocator);
		// Advance the pointer
		ptr += allocator.GetCurrentUsage();
		return copy;
	}

	void CopyableDeallocate(Copyable* copyable, AllocatorPolymorphic allocator) {
		if (copyable != nullptr) {
			copyable->Deallocate(allocator);
			Deallocate(allocator, copyable);
		}
	}

}
