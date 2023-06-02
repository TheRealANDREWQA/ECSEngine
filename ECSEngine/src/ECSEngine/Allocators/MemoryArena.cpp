#include "ecspch.h"
#include "MemoryArena.h"
#include "../Utilities/Assert.h"

namespace ECSEngine {

	MemoryArena::MemoryArena() : m_allocators(nullptr), m_allocator_count(0), m_current_index(0), m_size_per_allocator(0), m_initial_buffer(nullptr) {}

	MemoryArena::MemoryArena(
		void* buffer,
		size_t capacity, 
		size_t allocator_count,
		size_t blocks_per_allocator
	) : m_allocators((MultipoolAllocator*)buffer), m_allocator_count(allocator_count), m_current_index(0), m_size_per_allocator(0)
	{
		m_size_per_allocator = capacity / allocator_count;
		size_t arena_buffer_offset = sizeof(MultipoolAllocator) * allocator_count;

		uintptr_t ptr = (uintptr_t)buffer;
		m_initial_buffer = (void*)(ptr + MemoryOfArenas(allocator_count, blocks_per_allocator));
		for (size_t index = 0; index < allocator_count; index++) {
			m_allocators[index] = MultipoolAllocator(
				(unsigned char*)((uintptr_t)m_initial_buffer + index * m_size_per_allocator),
				(void*)((uintptr_t)m_allocators + arena_buffer_offset + index * (MultipoolAllocator::MemoryOf(blocks_per_allocator) + 16)),
				m_size_per_allocator,
				blocks_per_allocator
			);
		}
	}

	void* MemoryArena::Allocate(size_t size, size_t alignment) {
		size_t index = m_current_index;
		while (index < m_allocator_count) {
			void* allocation = m_allocators[index].Allocate(size, alignment);
			if (allocation != nullptr) {
				m_current_index = m_current_index + 1 == m_allocator_count ? 0 : m_current_index + 1;
				return allocation;
			}
			index++;
		}
		index = 0;
		while (index < m_current_index) {
			void* allocation = m_allocators[index].Allocate(size, alignment);
			if (allocation != nullptr) {
				m_current_index = m_current_index + 1 == m_allocator_count ? 0 : m_current_index + 1;
				return allocation;
			}
			index++;
		}
		return nullptr;
	}

	bool MemoryArena::Belongs(const void* buffer) const
	{
		uintptr_t ptr = (uintptr_t)buffer;
		return ptr >= (uintptr_t)m_initial_buffer && ptr < (uintptr_t)m_initial_buffer + m_size_per_allocator * m_allocator_count;
	}

	void MemoryArena::Clear()
	{
		for (size_t index = 0; index < m_allocator_count; index++) {
			m_allocators[index].Clear();
		}
	}

	const void* MemoryArena::GetAllocatedBuffer() const
	{
		uintptr_t ptr = (uintptr_t)m_initial_buffer;
		return (const void*)(ptr - MemoryOfArenas(m_allocator_count, m_allocators[0].m_range.GetCapacity()));
	}

	bool MemoryArena::IsEmpty() const
	{
		for (size_t index = 0; index < m_allocator_count; index++) {
			if (!m_allocators[index].IsEmpty()) {
				return false;
			}
		}
		return true;
	}

	// ---------------------------------------------- Thread safe variants ---------------------------------------------------

	size_t GetAllocatorIndex(const MemoryArena* arena, const void* block) {
		uintptr_t block_reinterpretation = (uintptr_t)block;
		size_t offset = block_reinterpretation - (uintptr_t)arena->m_initial_buffer;
		return offset / arena->m_size_per_allocator;
	}

	template<bool trigger_error_if_not_found>
	bool MemoryArena::Deallocate(const void* block)
	{
		size_t arena_index = GetAllocatorIndex(this, block);
		return m_allocators[arena_index].Deallocate<trigger_error_if_not_found>(block);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, MemoryArena::Deallocate, const void*);

	void* MemoryArena::Reallocate(const void* block, size_t new_size, size_t alignment)
	{
		size_t arena_index = GetAllocatorIndex(this, block);
		void* reallocation = m_allocators[arena_index].Reallocate(block, new_size, alignment);
		if (reallocation == nullptr) {
			// Deallocate this block and try to reallocate
			Deallocate(block);
			return Allocate(new_size, alignment);
		}
		return reallocation;
	}

	// ---------------------------------------------- Thread safe variants ---------------------------------------------------

	void* MemoryArena::Allocate_ts(size_t size, size_t alignment) {
		return ThreadSafeFunctorReturn(&m_lock, [&]() {
			return Allocate(size, alignment);
		});
	}

	template<bool trigger_error_if_not_found>
	bool MemoryArena::Deallocate_ts(const void* block) {
		return ThreadSafeFunctorReturn(&m_lock, [&]() {
			return Deallocate<trigger_error_if_not_found>(block);
		});
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, MemoryArena::Deallocate_ts, const void*);

	void* MemoryArena::Reallocate_ts(const void* block, size_t new_size, size_t alignment)
	{
		return ThreadSafeFunctorReturn(&m_lock, [&]() {
			return Reallocate(block, new_size, alignment);
		});
	}

	size_t MemoryArena::MemoryOfArenas(size_t allocator_count, size_t blocks_per_allocator) {
		return (sizeof(MultipoolAllocator) + MultipoolAllocator::MemoryOf(blocks_per_allocator) + 16) * allocator_count + 16;
	}

	size_t MemoryArena::MemoryOf(size_t capacity, size_t allocator_count, size_t blocks_per_allocator) {
		return capacity + MemoryOfArenas(allocator_count, blocks_per_allocator);
	}

	ResizableMemoryArena::ResizableMemoryArena() : m_backup(nullptr), m_new_allocator_count(0), m_new_arena_capacity(0),
		m_new_blocks_per_allocator(0) {}

	ResizableMemoryArena::ResizableMemoryArena(
		GlobalMemoryManager* backup,
		size_t initial_arena_capacity,
		size_t initial_allocator_count,
		size_t initial_blocks_per_allocator,
		size_t arena_capacity = 0,
		size_t allocator_count = 0,
		size_t blocks_per_allocator = 0
	) : m_backup(backup), m_new_allocator_count(allocator_count), m_new_arena_capacity(arena_capacity),
		m_new_blocks_per_allocator(blocks_per_allocator) 
	{
		void* arena_allocation = backup->Allocate_ts(MemoryArena::MemoryOf(initial_arena_capacity, initial_allocator_count, initial_blocks_per_allocator));
		m_arenas = (MemoryArena*)backup->Allocate_ts(sizeof(MemoryArena) * ECS_MEMORY_ARENA_DEFAULT_STREAM_SIZE);
		m_arenas[0] = MemoryArena(arena_allocation, initial_arena_capacity, initial_allocator_count, initial_blocks_per_allocator);
		m_arena_size = 1;
		m_arena_capacity = ECS_MEMORY_ARENA_DEFAULT_STREAM_SIZE;

		if (arena_capacity == 0) {
			m_new_arena_capacity = initial_arena_capacity;
		}
		if (allocator_count == 0) {
			m_new_allocator_count = initial_allocator_count;
		}
		if (blocks_per_allocator == 0) {
			m_new_blocks_per_allocator = initial_blocks_per_allocator;
		}
	}

	void* ResizableMemoryArena::Allocate(size_t size, size_t alignment) {
		for (int64_t index = m_arena_size - 1; index >= 0; index--) {
			void* allocation = m_arenas[index].Allocate(size, alignment);
			if (allocation != nullptr) {
				return allocation;
			}
		}
		CreateArena();
		void* allocation = m_arenas[m_arena_size - 1].Allocate(size, alignment);
		ECS_ASSERT(allocation != nullptr);
		return allocation;
	}

	template<bool trigger_error_if_not_found>
	bool ResizableMemoryArena::Deallocate(const void* block) {
		for (int64_t index = m_arena_size - 1; index >= 0; index--) {
			if (m_arenas[index].Belongs(block)) {
				bool was_deallocated = m_arenas[index].Deallocate<trigger_error_if_not_found>(block);
				if (was_deallocated) {
					if (m_arenas[index].IsEmpty()) {
						DeallocateArena(index);
					}
					return true;
				}
				else {
					return false;
				}
			}
		}
		if constexpr (trigger_error_if_not_found) {
			ECS_ASSERT(false);
		}
		return false;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, ResizableMemoryArena::Deallocate, const void*);

	void* ResizableMemoryArena::Reallocate(const void* block, size_t new_size, size_t alignment)
	{
		for (int64_t index = m_arena_size - 1; index >= 0; index--) {
			if (m_arenas[index].Belongs(block)) {
				return m_arenas[index].Reallocate(block, new_size, alignment);
			}
		}
		ECS_ASSERT(false);
		return nullptr;
	}

	bool ResizableMemoryArena::Belongs(const void* buffer) const
	{
		for (size_t index = 0; index < m_arena_size; index++) {
			if (m_arenas[index].Belongs(buffer)) {
				return true;
			}
		}

		return false;
	}

	void ResizableMemoryArena::Clear()
	{
		for (size_t index = 1; index < m_arena_size; index++) {
			DeallocateArena(index);
		}
		m_arena_size = 1;
		m_arenas[0].Clear();
	}

	void ResizableMemoryArena::CreateArena() {
		CreateArena(m_new_arena_capacity, m_new_allocator_count, m_new_blocks_per_allocator);
	}

	void ResizableMemoryArena::CreateArena(size_t arena_capacity, size_t allocator_count, size_t blocks_per_allocator) {
		void* arena_allocation = m_backup->Allocate_ts(MemoryArena::MemoryOf(arena_capacity, allocator_count, blocks_per_allocator));

		ECS_ASSERT(arena_allocation != nullptr);
		if (m_arena_size == m_arena_capacity) {
			size_t new_capacity = (size_t)((float)m_arena_capacity * 1.5f + 1);
			MemoryArena* new_arenas = (MemoryArena*)m_backup->Allocate_ts(sizeof(MemoryArena) * new_capacity);
			memcpy(new_arenas, m_arenas, sizeof(MemoryArena) * m_arena_capacity);
			m_arena_capacity = new_capacity;
			m_arenas = new_arenas;
		}

		m_arenas[m_arena_size++] = MemoryArena(
			arena_allocation,
			arena_capacity,
			allocator_count,
			blocks_per_allocator
		);
	}

	void ResizableMemoryArena::DeallocateArena(size_t index)
	{
		m_backup->Deallocate_ts(m_arenas[index].GetAllocatedBuffer());
	}

	void ResizableMemoryArena::Free()
	{
		for (size_t index = 0; index < m_arena_size; index++) {
			DeallocateArena(index);
		}
		m_backup->Deallocate(m_arenas);
	}

	// ---------------------------------------------------- Thread safe variants -----------------------------------------

	void* ResizableMemoryArena::Allocate_ts(size_t size, size_t alignment) {
		for (int64_t index = m_arena_size - 1; index >= 0; index--) {
			void* allocation = m_arenas[index].Allocate_ts(size, alignment);
			if (allocation != nullptr) {
				return allocation;
			}
		}
		unsigned int current_arena_size = m_arena_size;
		bool try_lock = m_lock.try_lock();
		if (try_lock) {
			CreateArena();
			m_lock.unlock();
		}
		else {
			m_lock.wait_locked();
		}
		return m_arenas[m_arena_size - 1].Allocate_ts(size, alignment);
	}

	template<bool trigger_error_if_not_found>
	bool ResizableMemoryArena::Deallocate_ts(const void* block)
	{
		for (int64_t index = m_arena_size - 1; index >= 0; index--) {
			if (m_arenas[index].Belongs(block)) {
				return m_arenas[index].Deallocate_ts<trigger_error_if_not_found>(block);
			}
		}
		return false;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, ResizableMemoryArena::Deallocate_ts, const void*);

	void* ResizableMemoryArena::Reallocate_ts(const void* block, size_t new_size, size_t alignment)
	{
		for (int64_t index = m_arena_size - 1; index >= 0; index--) {
			if (m_arenas[index].Belongs(block)) {
				return m_arenas[index].Reallocate_ts(block, new_size, alignment);
			}
		}
		ECS_ASSERT(false);
		return nullptr;
	}

}