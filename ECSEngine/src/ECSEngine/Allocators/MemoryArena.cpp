#include "ecspch.h"
#include "MemoryArena.h"
#include "../Utilities/Assert.h"
#include "AllocatorCallsDebug.h"
#include "AllocatorPolymorphic.h"
#include "../Utilities/PointerUtilities.h"
#include "MultipoolAllocator.h"
#include "../Profiling/AllocatorProfilingGlobal.h"

namespace ECSEngine {

	static void Init(MemoryArena* arena, void* buffer, size_t allocator_count, CreateBaseAllocatorInfo base_info) {
		ECS_ASSERT(allocator_count < UCHAR_MAX);

		ECS_ALLOCATOR_TYPE nested_type = base_info.allocator_type == ECS_ALLOCATOR_ARENA ? base_info.arena_nested_type : base_info.allocator_type;
		size_t allocator_size = BaseAllocatorByteSize(nested_type) * allocator_count;
		size_t total_allocation_size = base_info.allocator_type == ECS_ALLOCATOR_ARENA ? 
			arena->MemoryOf(allocator_count, base_info.arena_capacity, base_info.arena_nested_type) : 
			arena->MemoryOf(allocator_count, base_info);
		arena->m_data_buffer = OffsetPointer(buffer, allocator_size);

		CreateBaseAllocatorInfo nested_info;
		if (base_info.allocator_type == ECS_ALLOCATOR_ARENA) {
			nested_info.allocator_type = base_info.arena_nested_type;
			switch (nested_info.allocator_type) {
			case ECS_ALLOCATOR_LINEAR:
				nested_info.linear_capacity = base_info.arena_capacity / base_info.arena_allocator_count;
				break;
			case ECS_ALLOCATOR_STACK:
				nested_info.stack_capacity = base_info.arena_capacity / base_info.arena_allocator_count;
				break;
			case ECS_ALLOCATOR_MULTIPOOL:
				nested_info.multipool_block_count = base_info.arena_multipool_block_count;
				nested_info.multipool_capacity = MultipoolAllocator::CapacityFromFixedSize(
					base_info.arena_capacity / base_info.arena_allocator_count, 
					base_info.arena_multipool_block_count
				);
				break;
			default:
				ECS_ASSERT(false, "Invalid arena nested allocator type");
			}
		}
		else {
			nested_info = base_info;
		}

		CreateBaseAllocators(arena->m_data_buffer, nested_info, buffer, allocator_count);
		arena->m_allocator_count = allocator_count;
		arena->m_lock.Clear();
		arena->m_allocators = buffer;
		arena->m_base_allocator_type = nested_type;
		arena->m_size_per_allocator = (total_allocation_size - allocator_size) / allocator_count;
		arena->m_current_index = 0;
		arena->m_debug_mode = false;
		arena->m_profiling_mode = false;
		arena->m_base_allocator_byte_size = allocator_size / allocator_count;
	}

	template<bool thread_safe, bool disable_debug_info>
	void* AllocateImpl(MemoryArena* arena, size_t size, size_t alignment, DebugInfo debug_info) {
		// In thread safe mode, increment the index under lock
		// TODO: Is it worth to make the increment atomic and then perform a modulo?
		size_t current_index = arena->m_current_index;
		if constexpr (thread_safe) {
			arena->Lock();
			arena->m_current_index = arena->m_current_index + 1 == arena->m_allocator_count ? 0 : arena->m_current_index + 1;
			current_index = arena->m_current_index;
			arena->Unlock();
		}
		
		auto loop = [&](size_t index, size_t bound) {
			while (index < bound) {
				AllocatorPolymorphic current_allocator = arena->GetAllocator(index);
				if constexpr (thread_safe) {
					current_allocator.allocation_type = ECS_ALLOCATION_MULTI;
				}
				void* allocation = ECSEngine::Allocate(current_allocator, size, alignment);
				if (allocation != nullptr) {
					if constexpr (!disable_debug_info) {
						if (arena->m_debug_mode) {
							TrackedAllocation tracked;
							tracked.allocated_pointer = allocation;
							tracked.debug_info = debug_info;
							tracked.function_type = ECS_DEBUG_ALLOCATOR_ALLOCATE;
							DebugAllocatorManagerAddEntry(arena, ECS_ALLOCATOR_ARENA, &tracked);
						}
						if (arena->m_profiling_mode) {
							AllocatorProfilingAddAllocation(arena, arena->GetCurrentUsage(), AllocatorPolymorphicBlockCount(arena));
						}
					}
					return allocation;
				}
				index++;
			}
			return (void*)nullptr;
		};

		// Firstly try all allocators starting from the current index to the last one
		// And the search from the beginning to the current index
		void* allocation = loop(current_index, arena->m_allocator_count);
		if (allocation) {
			return allocation;
		}

		return loop(0, current_index);
	}

	template<bool thread_safe, bool trigger_error_if_not_found>
	bool DeallocateImpl(MemoryArena* arena, const void* block, DebugInfo debug_info) {
		size_t arena_index = GetAllocatorIndex(arena, block);
		AllocatorPolymorphic current_allocator = arena->GetAllocator(arena_index);
		if constexpr (thread_safe) {
			current_allocator.allocation_type = ECS_ALLOCATION_MULTI;
		}
		bool was_deallocated = ECSEngine::Deallocate<trigger_error_if_not_found>(current_allocator, block);
		if (was_deallocated) {
			if (arena->m_debug_mode) {
				TrackedAllocation tracked;
				tracked.allocated_pointer = block;
				tracked.debug_info = debug_info;
				tracked.function_type = ECS_DEBUG_ALLOCATOR_DEALLOCATE;
				DebugAllocatorManagerAddEntry(arena, ECS_ALLOCATOR_ARENA, &tracked);
			}
			if (arena->m_profiling_mode) {
				AllocatorProfilingAddDeallocation(arena);
			}
		}
		return was_deallocated;
	}

	template<bool thread_safe>
	void* ReallocateImpl(MemoryArena* arena, const void* block, size_t new_size, size_t alignment, DebugInfo debug_info) {
		size_t index = 0;
		void* reallocation = nullptr;
		for (; index < arena->m_allocator_count; index++) {
			AllocatorPolymorphic current_allocator = arena->GetAllocator(index);
			if constexpr (thread_safe) {
				LockAllocator(current_allocator);
			}
			if (BelongsToAllocator(current_allocator, block)) {
				reallocation = ECSEngine::Reallocate(current_allocator, block, new_size, alignment);
				if constexpr (thread_safe) {
					UnlockAllocator(current_allocator);
				}
				if (reallocation == nullptr) {
					// Deallocate this block and try to reallocate
					// We need to release the lock on this arena, then allocate, and then deallocate
					// multithreaded (if thread safe is specified
					reallocation = AllocateImpl<thread_safe, true>(arena, new_size, alignment, debug_info);
					if constexpr (thread_safe) {
						current_allocator.allocation_type = ECS_ALLOCATION_MULTI;
					}
					ECSEngine::Deallocate(current_allocator, block);
				}
				break;
			}
			if constexpr (thread_safe) {
				UnlockAllocator(current_allocator);
			}
		}

		ECS_ASSERT(index < arena->m_allocator_count, "Invalid reallocation for memory arena");

		if (arena->m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = block;
			tracked.secondary_pointer = reallocation;
			tracked.debug_info = debug_info;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_REALLOCATE;
			DebugAllocatorManagerAddEntry(arena, ECS_ALLOCATOR_ARENA, &tracked);
		}
		if (arena->m_profiling_mode) {
			AllocatorProfilingAddDeallocation(arena);
			AllocatorProfilingAddAllocation(arena, arena->GetCurrentUsage(), AllocatorPolymorphicBlockCount(arena));
		}

		return reallocation;
	}

	MemoryArena::MemoryArena(void* buffer, size_t allocator_count, CreateBaseAllocatorInfo base_info)
	{
		Init(this, buffer, allocator_count, base_info);
	}

	MemoryArena::MemoryArena(AllocatorPolymorphic buffer_allocator, size_t allocator_count, CreateBaseAllocatorInfo base_info)
	{
		ECS_ASSERT(allocator_count < UCHAR_MAX);

		size_t allocation_size = MemoryOf(allocator_count, base_info);
		void* allocation = AllocateEx(buffer_allocator, allocation_size);
		Init(this, allocation, allocator_count, base_info);
	}

	MemoryArena::MemoryArena(void* buffer, size_t allocator_count, size_t arena_capacity, size_t blocks_per_allocator)
	{
		CreateBaseAllocatorInfo info;
		info.allocator_type = ECS_ALLOCATOR_ARENA;
		info.arena_capacity = arena_capacity;
		info.arena_nested_type = ECS_ALLOCATOR_MULTIPOOL;
		info.arena_allocator_count = allocator_count;
		info.arena_multipool_block_count = blocks_per_allocator;
		Init(this, buffer, allocator_count, info);
	}

	void* MemoryArena::Allocate(size_t size, size_t alignment, DebugInfo debug_info) {
		return AllocateImpl<false, false>(this, size, alignment, debug_info);
	}

	bool MemoryArena::Belongs(const void* buffer) const
	{
		uintptr_t ptr = (uintptr_t)buffer;
		return ptr >= (uintptr_t)m_data_buffer && ptr < (uintptr_t)m_data_buffer + m_size_per_allocator * m_allocator_count;
	}

	void MemoryArena::Clear(DebugInfo debug_info)
	{
		for (size_t index = 0; index < m_allocator_count; index++) {
			ClearAllocator(GetAllocator(index));
		}
	}

	bool MemoryArena::IsEmpty() const
	{
		for (size_t index = 0; index < m_allocator_count; index++) {
			if (!IsAllocatorEmpty(GetAllocator(index))) {
				return false;
			}
		}
		return true;
	}

	AllocatorPolymorphic MemoryArena::GetAllocator(size_t index) const
	{
		return { OffsetPointer(m_allocators, index * m_base_allocator_byte_size), m_base_allocator_type, ECS_ALLOCATION_SINGLE };
	}

	size_t MemoryArena::GetCurrentUsage() const
	{
		size_t total_usage = 0;
		for (unsigned char index = 0; index < m_allocator_count; index++) {
			total_usage += GetAllocatorCurrentUsage(GetAllocator(index));
		}
		return total_usage;
	}

	size_t GetAllocatorIndex(const MemoryArena* arena, const void* block) {
		uintptr_t block_reinterpretation = (uintptr_t)block;
		size_t offset = block_reinterpretation - (uintptr_t)arena->m_data_buffer;
		return offset / arena->m_size_per_allocator;
	}

	template<bool trigger_error_if_not_found>
	bool MemoryArena::Deallocate(const void* block, DebugInfo debug_info)
	{
		return DeallocateImpl<false, trigger_error_if_not_found>(this, block, debug_info);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, MemoryArena::Deallocate, const void*, DebugInfo);

	void* MemoryArena::Reallocate(const void* block, size_t new_size, size_t alignment, DebugInfo debug_info)
	{
		return ReallocateImpl<false>(this, block, new_size, alignment, debug_info);
	}

	void MemoryArena::ExitDebugMode()
	{
		m_debug_mode = false;
	}

	void MemoryArena::ExitProfilingMode()
	{
		m_profiling_mode = false;
	}

	void MemoryArena::SetDebugMode(const char* name, bool resizable)
	{
		m_debug_mode = true;
		DebugAllocatorManagerChangeOrAddEntry(this, name, resizable, ECS_ALLOCATOR_ARENA);
	}

	void MemoryArena::SetProfilingMode(const char* name)
	{
		m_profiling_mode = true;
		AllocatorProfilingAddEntry(this, ECS_ALLOCATOR_ARENA, name);
	}

	size_t MemoryArena::GetAllocatedRegions(void** region_start, size_t* region_size, size_t pointer_capacity) const
	{
		if (pointer_capacity >= 1) {
			*region_start = GetAllocatedBuffer();
			*region_size = InitialArenaCapacity();
		}
		return 1;
	}

	void* MemoryArena::Allocate_ts(size_t size, size_t alignment, DebugInfo debug_info) {
		return AllocateImpl<true, false>(this, size, alignment, debug_info);
	}

	template<bool trigger_error_if_not_found>
	bool MemoryArena::Deallocate_ts(const void* block, DebugInfo debug_info) {
		return DeallocateImpl<true, trigger_error_if_not_found>(this, block, debug_info);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, MemoryArena::Deallocate_ts, const void*, DebugInfo);

	void* MemoryArena::Reallocate_ts(const void* block, size_t new_size, size_t alignment, DebugInfo debug_info)
	{
		return ReallocateImpl<true>(this, block, new_size, alignment, debug_info);
	}

	size_t MemoryArena::MemoryOf(size_t allocator_count, CreateBaseAllocatorInfo info)
	{
		return allocator_count * (BaseAllocatorBufferSize(info) + BaseAllocatorByteSize(info.allocator_type));
	}

	size_t MemoryArena::MemoryOf(size_t allocator_count, size_t total_capacity, ECS_ALLOCATOR_TYPE allocator_type)
	{
		return allocator_count * BaseAllocatorByteSize(allocator_type) + total_capacity;
	}

}