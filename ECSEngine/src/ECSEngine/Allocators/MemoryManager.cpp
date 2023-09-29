#include "ecspch.h"
#include "MemoryManager.h"
#include "../Utilities/Assert.h"
#include "../Utilities/Function.h"
#include "AllocatorCallsDebug.h"
#include "AllocatorPolymorphic.h"

#define ECS_MEMORY_MANAGER_SIZE 8

namespace ECSEngine {

	template<bool thread_safe, bool trigger_error_if_not_found>
	bool DeallocateImpl(MemoryManager* memory_manager, const void* block, DebugInfo debug_info) {
		size_t allocator_count = memory_manager->m_allocator_count;
		for (size_t index = 0; index < allocator_count; index++) {
			AllocatorPolymorphic current_allocator = memory_manager->GetAllocator(index);
			if (ECSEngine::BelongsToAllocator(current_allocator, block)) {
				bool was_deallocated;
				if constexpr (thread_safe) {
					was_deallocated = ECSEngine::DeallocateTs<trigger_error_if_not_found>(current_allocator, block);
				}
				else {
					was_deallocated = ECSEngine::Deallocate<trigger_error_if_not_found>(current_allocator, block);
				}

				if constexpr (!thread_safe) {
					// We cannot perform the removal in multithreaded context
					// Because it will update the m_allocator_count which would
					// Not be visible to other threads
					if (index > 0 && ECSEngine::IsAllocatorEmpty(current_allocator)) {
						FreeAllocatorFrom(current_allocator, memory_manager->m_backup);
						Stream<void> untyped_allocator = { memory_manager->m_allocators, memory_manager->m_allocator_count };
						untyped_allocator.RemoveSwapBack(index, memory_manager->m_base_allocator_byte_size);
						memory_manager->m_allocator_count--;
					}
				}

				if (was_deallocated && memory_manager->m_debug_mode) {
					TrackedAllocation tracked;
					tracked.allocated_pointer = block;
					tracked.debug_info = debug_info;
					tracked.function_type = ECS_DEBUG_ALLOCATOR_DEALLOCATE;
					DebugAllocatorManagerAddEntry(memory_manager, ECS_ALLOCATOR_MANAGER, &tracked);
				}
				return was_deallocated;
			}
		}
		if constexpr (trigger_error_if_not_found) {
			ECS_ASSERT(false, "Invalid deallocate block for Memory Manager");
		}
		return false;
	}

	template<bool thread_safe, bool disable_debug_info>
	void* AllocateImpl(MemoryManager* memory_manager, size_t size, size_t alignment, DebugInfo debug_info) {
		size_t allocator_count = memory_manager->m_allocator_count;
		for (size_t index = 0; index < allocator_count; index++) {
			AllocatorPolymorphic current_allocator = memory_manager->GetAllocator(index);
			void* allocation;
			if constexpr (thread_safe) {
				allocation = ECSEngine::AllocateTs(current_allocator.allocator, current_allocator.allocator_type, size, alignment);
			}
			else {
				allocation = ECSEngine::Allocate(current_allocator, size, alignment);
			}

			if (allocation != nullptr) {
				if constexpr (!disable_debug_info) {
					if (memory_manager->m_debug_mode) {
						TrackedAllocation tracked;
						tracked.allocated_pointer = allocation;
						tracked.debug_info = debug_info;
						tracked.function_type = ECS_DEBUG_ALLOCATOR_ALLOCATE;
						DebugAllocatorManagerAddEntry(memory_manager, ECS_ALLOCATOR_MANAGER, &tracked);
					}
				}
				return allocation;
			}
		}
		
		bool was_acquired = true;
		if constexpr (thread_safe) {
			was_acquired = memory_manager->m_spin_lock.try_lock();
		}
		if (was_acquired) {
			memory_manager->CreateAllocator(memory_manager->m_backup_info);
			if constexpr (thread_safe) {
				memory_manager->m_spin_lock.unlock();
			}
		}
		else {
			if constexpr (thread_safe) {
				memory_manager->m_spin_lock.wait_locked();
			}
		}
		AllocatorPolymorphic last_allocator = memory_manager->GetAllocator(allocator_count);
		void* allocation;
		if constexpr (thread_safe) {
			allocation = ECSEngine::AllocateTs(last_allocator.allocator, last_allocator.allocator_type, size, alignment);
		}
		else {
			allocation = ECSEngine::Allocate(last_allocator, size, alignment);
		}

		if constexpr (!disable_debug_info) {
			if (memory_manager->m_debug_mode) {
				TrackedAllocation tracked;
				tracked.allocated_pointer = allocation;
				tracked.debug_info = debug_info;
				tracked.function_type = ECS_DEBUG_ALLOCATOR_ALLOCATE;
				DebugAllocatorManagerAddEntry(memory_manager, ECS_ALLOCATOR_MANAGER, &tracked);
			}
		}
		return allocation;
	}

	template<bool thread_safe>
	void* ReallocateImpl(MemoryManager* memory_manager, const void* block, size_t new_size, size_t alignment, DebugInfo debug_info) {
		size_t allocator_count = memory_manager->m_allocator_count;
		for (size_t index = 0; index < allocator_count; index++) {
			AllocatorPolymorphic current_allocator = memory_manager->GetAllocator(index);
			if (ECSEngine::BelongsToAllocator(current_allocator, block)) {
				void* reallocation;
				if constexpr (thread_safe) {
					current_allocator.allocation_type = ECS_ALLOCATION_MULTI;
				}
				reallocation = ECSEngine::Reallocate(current_allocator, block, new_size, alignment);

				if (reallocation == nullptr) {
					// Allocate using the general function and deallocate the block from the current allocator
					reallocation = AllocateImpl<thread_safe, true>(memory_manager, new_size, alignment, debug_info);
					ECSEngine::Deallocate(current_allocator, block);
				}

				if (memory_manager->m_debug_mode) {
					TrackedAllocation tracked;
					tracked.allocated_pointer = block;
					tracked.secondary_pointer = reallocation;
					tracked.debug_info = debug_info;
					tracked.function_type = ECS_DEBUG_ALLOCATOR_REALLOCATE;
					DebugAllocatorManagerAddEntry(memory_manager, ECS_ALLOCATOR_MANAGER, &tracked);
				}

				return reallocation;
			}
		}
		ECS_ASSERT(false, "Reallocation not found on MemoryManager");
		return nullptr;
	}

	template<bool thread_safe>
	bool DeallocateIfBelongsImpl(MemoryManager* memory_manager, const void* block, DebugInfo debug_info) {
		size_t allocator_count = memory_manager->m_allocator_count;
		for (size_t index = 0; index < allocator_count; index++) {
			AllocatorPolymorphic current_allocator = memory_manager->GetAllocator(index);
			if (ECSEngine::BelongsToAllocator(current_allocator, block)) {
				bool was_deallocated;
				if constexpr (thread_safe) {
					current_allocator.allocation_type = ECS_ALLOCATION_MULTI;
				}
				was_deallocated = ECSEngine::DeallocateNoAssert(current_allocator, block);

				if constexpr (!thread_safe) {
					// We cannot perform the removal in multithreaded context
					// Because it will update the m_allocator_count which would
					// Not be visible to other threads
					if (index > 0 && ECSEngine::IsAllocatorEmpty(current_allocator)) {
						FreeAllocatorFrom(current_allocator, memory_manager->m_backup);
						Stream<void> untyped_allocator = { memory_manager->m_allocators, memory_manager->m_allocator_count };
						untyped_allocator.RemoveSwapBack(index, memory_manager->m_base_allocator_byte_size);
						memory_manager->m_allocator_count--;
					}
				}

				if (was_deallocated) {
					if (memory_manager->m_debug_mode) {
						TrackedAllocation tracked;
						tracked.allocated_pointer = block;
						tracked.debug_info = debug_info;
						tracked.function_type = ECS_DEBUG_ALLOCATOR_DEALLOCATE;
						DebugAllocatorManagerAddEntry(memory_manager, ECS_ALLOCATOR_MANAGER, &tracked);
					}
				}
				return was_deallocated;
			}
		}

		return false;
	}

	void Init(MemoryManager* manager, CreateBaseAllocatorInfo initial_info, CreateBaseAllocatorInfo backup_info, AllocatorPolymorphic backup) {
		ECS_ASSERT(initial_info.allocator_type == backup_info.allocator_type);

		manager->m_allocator_count = 0;
		manager->m_backup = backup;
		size_t base_allocator_size = BaseAllocatorSize(initial_info.allocator_type);
		void* allocation = AllocateTsEx(backup, (base_allocator_size)*ECS_MEMORY_MANAGER_SIZE);
		manager->m_allocators = allocation;
		manager->m_backup_info = backup_info;
		manager->m_base_allocator_byte_size = base_allocator_size;
		manager->m_debug_mode = false;
		manager->CreateAllocator(initial_info);
	}

	MemoryManager::MemoryManager(size_t size, size_t maximum_pool_count, size_t new_allocation_size, AllocatorPolymorphic backup)
	{
		CreateBaseAllocatorInfo initial_info;
		initial_info.allocator_type = ECS_ALLOCATOR_MULTIPOOL;
		initial_info.multipool_block_count = maximum_pool_count;
		initial_info.multipool_capacity = size;
		CreateBaseAllocatorInfo backup_info = initial_info;
		backup_info.multipool_capacity = new_allocation_size;
		Init(this, initial_info, backup_info, backup);
	}

	MemoryManager::MemoryManager(CreateBaseAllocatorInfo initial_info, CreateBaseAllocatorInfo backup_info, AllocatorPolymorphic backup) {
		Init(this, initial_info, backup_info, backup);
	}

	void MemoryManager::Clear(DebugInfo debug_info)
	{
		for (size_t index = 1; index < m_allocator_count; index++) {
			FreeAllocatorFrom(GetAllocator(index), m_backup);
		}
		ClearAllocator(GetAllocator(0));
		m_allocator_count = 1;

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = nullptr;
			tracked.debug_info = debug_info;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_CLEAR;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_MANAGER, &tracked);
		}
	}

	void MemoryManager::Free(DebugInfo debug_info) {
		for (size_t index = 0; index < m_allocator_count; index++) {
			FreeAllocatorFrom(GetAllocator(index), m_backup);
		}
		ECSEngine::DeallocateEx(m_backup, m_allocators);

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = nullptr;
			tracked.debug_info = debug_info;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_FREE;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_MANAGER, &tracked);
		}
	}

	void MemoryManager::CreateAllocator(CreateBaseAllocatorInfo info) {
		ECS_ASSERT(m_allocator_count < ECS_MEMORY_MANAGER_SIZE);
		AllocatorPolymorphic allocator_to_be_constructed = GetAllocator(m_allocator_count);
		CreateBaseAllocator(m_backup, info, allocator_to_be_constructed.allocator);
		m_allocator_count++;
	}

	void* MemoryManager::Allocate(size_t size, size_t alignment, DebugInfo debug_info) {
		return AllocateImpl<false, false>(this, size, alignment, debug_info);
	}

	template<bool trigger_error_if_not_found>
	bool MemoryManager::Deallocate(const void* block, DebugInfo debug_info) {
		return DeallocateImpl<false, trigger_error_if_not_found>(this, block, debug_info);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, MemoryManager::Deallocate, const void*, DebugInfo);

	void* MemoryManager::Reallocate(const void* block, size_t new_size, size_t alignment, DebugInfo debug_info)
	{
		return ReallocateImpl<false>(this, block, new_size, alignment, debug_info);
	}

	bool MemoryManager::DeallocateIfBelongs(const void* block, DebugInfo debug_info)
	{
		return DeallocateIfBelongsImpl<false>(this, block, debug_info);
	}

	bool MemoryManager::IsEmpty() const
	{
		size_t allocator_count = m_allocator_count;
		for (size_t index = 0; index < allocator_count; index++) {
			if (!IsAllocatorEmpty(GetAllocator(index))) {
				return false;
			}
		}
		return true;
	}

	void MemoryManager::Trim()
	{
		size_t allocator_byte_size = BaseAllocatorSize(m_backup.allocator_type);
		for (size_t index = 1; index < m_allocator_count; index++) {
			AllocatorPolymorphic current_allocator = GetAllocator(index);
			if (IsAllocatorEmpty(current_allocator)) {
				FreeAllocatorFrom(current_allocator, m_backup);
				Stream<void> untyped_allocator = { m_allocators, m_allocator_count };
				untyped_allocator.RemoveSwapBack(index, allocator_byte_size);
				m_allocator_count--;
				index--;
			}
		}
	}

	bool MemoryManager::Belongs(const void* buffer) const
	{
		for (size_t index = 0; index < m_allocator_count; index++) {
			AllocatorPolymorphic current_allocator = GetAllocator(index);
			if (ECSEngine::BelongsToAllocator(current_allocator, buffer)) {
				return true;
			}
		}

		return false;
	}

	void MemoryManager::ExitDebugMode()
	{
		m_debug_mode = false;
	}

	void MemoryManager::SetDebugMode(const char* name, bool resizable)
	{
		m_debug_mode = true;
		DebugAllocatorManagerChangeOrAddEntry(this, name, resizable, ECS_ALLOCATOR_MANAGER);
	}

	AllocatorPolymorphic MemoryManager::GetAllocator(size_t index) const
	{
		return { function::OffsetPointer(m_allocators, m_base_allocator_byte_size * index), m_backup_info.allocator_type, ECS_ALLOCATION_SINGLE };
	}

	// ---------------------- Thread safe variants -----------------------------

	void* MemoryManager::Allocate_ts(size_t size, size_t alignment, DebugInfo debug_info) {
		return AllocateImpl<true, false>(this, size, alignment, debug_info);
	}

	template<bool trigger_error_if_not_found>
	bool MemoryManager::Deallocate_ts(const void* block, DebugInfo debug_info) {
		return DeallocateImpl<true, trigger_error_if_not_found>(this, block, debug_info);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, MemoryManager::Deallocate_ts, const void*, DebugInfo);

	void* MemoryManager::Reallocate_ts(const void* block, size_t new_size, size_t alignment, DebugInfo debug_info)
	{
		return ReallocateImpl<true>(this, block, new_size, alignment, debug_info);
	}

	bool MemoryManager::DeallocateIfBelongs_ts(const void* block, DebugInfo debug_info)
	{
		return DeallocateIfBelongsImpl<true>(this, block, debug_info);
	}

	GlobalMemoryManager CreateGlobalMemoryManager(
		size_t size, 
		size_t maximum_pool_count, 
		size_t new_allocation_size
	)
	{
		CreateBaseAllocatorInfo initial_info;
		initial_info.allocator_type = ECS_ALLOCATOR_MULTIPOOL;
		initial_info.multipool_block_count = maximum_pool_count;
		initial_info.multipool_capacity = size;
		CreateBaseAllocatorInfo backup_info = initial_info;
		backup_info.multipool_capacity = new_allocation_size;
		return MemoryManager(initial_info, backup_info, { nullptr });
	}

	ResizableMemoryArena CreateResizableMemoryArena(
		size_t initial_arena_capacity, 
		size_t initial_allocator_count, 
		size_t initial_blocks_per_allocator,
		AllocatorPolymorphic backup,
		size_t arena_capacity, 
		size_t allocator_count, 
		size_t blocks_per_allocator
	)
	{
		arena_capacity = arena_capacity == 0 ? initial_arena_capacity : arena_capacity;
		allocator_count = allocator_count == 0 ? initial_allocator_count : allocator_count;
		blocks_per_allocator = blocks_per_allocator == 0 ? initial_blocks_per_allocator : blocks_per_allocator;

		CreateBaseAllocatorInfo initial_info;
		initial_info.allocator_type = ECS_ALLOCATOR_ARENA;
		initial_info.arena_nested_type = ECS_ALLOCATOR_MULTIPOOL;
		initial_info.arena_allocator_count = initial_allocator_count;
		initial_info.arena_multipool_block_count = initial_blocks_per_allocator;
		initial_info.arena_capacity = initial_arena_capacity;
		CreateBaseAllocatorInfo backup_info = initial_info;
		backup_info.arena_capacity = arena_capacity;
		backup_info.arena_multipool_block_count = blocks_per_allocator;
		backup_info.arena_allocator_count = allocator_count;
		return MemoryManager(initial_info, backup_info, backup);
	}

}
