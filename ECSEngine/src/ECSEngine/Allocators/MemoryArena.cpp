#include "ecspch.h"
#include "MemoryArena.h"
#include "../Utilities/Assert.h"

namespace ECSEngine {

	MemoryArena::MemoryArena() : m_allocators(nullptr, 0), m_current_index(0), m_size_per_allocator(0), m_initial_buffer(nullptr) {}

	MemoryArena::MemoryArena(
		void* arena_buffer, 
		void* buffer, 
		size_t capacity, 
		size_t allocator_count,
		size_t blocks_per_allocator
	) : m_allocators(arena_buffer, allocator_count), m_current_index(0), m_initial_buffer(buffer), m_size_per_allocator(0)
	{
		m_size_per_allocator = capacity / allocator_count;
		size_t arena_buffer_offset = sizeof(MultipoolAllocator) * allocator_count;
		for (size_t index = 0; index < allocator_count; index++) {
			/*unsigned char* pogu = (unsigned char*)((uintptr_t)buffer + index * m_size_per_allocator);
			uintptr_t difference = (uintptr_t)pogu - (uintptr_t)buffer;
			void* pogu2 = (void*)((uintptr_t)arena_buffer + arena_buffer_offset + index * MultipoolAllocator::MemoryOf(blocks_per_allocator));
			uintptr_t difference2 = (uintptr_t)pogu2 - (uintptr_t)arena_buffer;*/
			m_allocators[index] = MultipoolAllocator(
				(unsigned char*)((uintptr_t)buffer + index * m_size_per_allocator),
				(void*)((uintptr_t)arena_buffer + arena_buffer_offset + index * (MultipoolAllocator::MemoryOf(blocks_per_allocator) + 16)),
				m_size_per_allocator,
				blocks_per_allocator
			);
		}
	}

	void* MemoryArena::Allocate(size_t size, size_t alignment) {
		size_t index = m_current_index;
		while (index < m_allocators.size) {
			void* allocation = m_allocators[index].Allocate(size, alignment);
			if (allocation != nullptr) {
				m_current_index = (m_current_index + 1) % m_allocators.size;
				return allocation;
			}
			index++;
		}
		index = 0;
		while (index < m_current_index) {
			void* allocation = m_allocators[index].Allocate(size, alignment);
			if (allocation != nullptr) {
				m_current_index = (m_current_index + 1) % m_allocators.size;
				return allocation;
			}
			index++;
		}
		return nullptr;
	}

	template<bool trigger_error_if_not_found>
	void MemoryArena::Deallocate(const void* block)
	{
		uintptr_t block_reinterpretation = (uintptr_t)block;
		size_t offset = block_reinterpretation - (uintptr_t)m_initial_buffer;
		size_t index = offset / m_size_per_allocator;
		m_allocators[index].Deallocate<trigger_error_if_not_found>(block);
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, MemoryArena::Deallocate, const void*);

	// ---------------------------------------------- Thread safe variants ---------------------------------------------------

	void* MemoryArena::Allocate_ts(size_t size, size_t alignment) {
		m_lock.lock();
		void* allocation = Allocate(size, alignment);
		m_lock.unlock();
		return allocation;
	}

	template<bool trigger_error_if_not_found>
	void MemoryArena::Deallocate_ts(const void* block) {
		m_lock.lock();
		Deallocate<trigger_error_if_not_found>(block);
		m_lock.unlock();
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, MemoryArena::Deallocate_ts, const void*);

	void MemoryArena::SetDebugBuffer(unsigned int* buffer, unsigned int blocks_per_allocator)
	{
		for (size_t index = 0; index < m_allocators.size; index++) {
			m_allocators[index].SetDebugBuffer(buffer + index * blocks_per_allocator);
		}
	}

	size_t MemoryArena::MemoryOf(size_t allocator_count, size_t blocks_per_allocator) {
		return (sizeof(MultipoolAllocator) + MultipoolAllocator::MemoryOf(blocks_per_allocator) + 16) * allocator_count + 16;
	}

	ResizableMemoryArena::ResizableMemoryArena() : m_backup(nullptr), m_new_allocator_count(0), m_new_arena_capacity(0),
		m_new_blocks_per_allocator(0) {}

	ResizableMemoryArena::ResizableMemoryArena(
		GlobalMemoryManager* backup,
		unsigned int initial_arena_capacity,
		unsigned int initial_allocator_count,
		unsigned int initial_blocks_per_allocator,
		unsigned int arena_capacity = 0,
		unsigned int allocator_count = 0,
		unsigned int blocks_per_allocator = 0
	) : m_backup(backup), m_new_allocator_count(allocator_count), m_new_arena_capacity(arena_capacity),
		m_new_blocks_per_allocator(blocks_per_allocator) 
	{
		void* arena_allocation = backup->Allocate(MemoryArena::MemoryOf(initial_allocator_count, initial_blocks_per_allocator), 8);
		void* buffer = backup->Allocate(initial_arena_capacity, 8);
		memset(arena_allocation, 0, MemoryArena::MemoryOf(initial_allocator_count, initial_blocks_per_allocator));
		memset(buffer, 0, initial_arena_capacity);
#ifdef ECSENGINE_DEBUG
		void* debug_buffer = backup->Allocate(containers::BlockRange::MemoryOfDebug(initial_blocks_per_allocator) * initial_allocator_count);
#endif
		m_arenas = containers::ResizableStream<MemoryArena, GlobalMemoryManager>(backup, ECS_MEMORY_ARENA_DEFAULT_STREAM_SIZE);
		m_arenas.Add(MemoryArena(arena_allocation, buffer, initial_arena_capacity, initial_allocator_count, initial_blocks_per_allocator));
#ifdef ECSENGINE_DEBUG
		m_arenas[0].SetDebugBuffer((unsigned int*)debug_buffer, initial_blocks_per_allocator);
#endif

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
		for (int64_t index = m_arenas.size - 1; index >= 0; index--) {
			void* allocation = m_arenas[index].Allocate(size, alignment);
			if (allocation != nullptr) {
				return allocation;
			}
		}
		CreateArena();
		void* allocation = m_arenas[m_arenas.size - 1].Allocate(size, alignment);
		ECS_ASSERT(allocation != nullptr);
		return allocation;
	}

	template<bool trigger_error_if_not_found>
	void ResizableMemoryArena::Deallocate(const void* block) {
		uintptr_t block_reinterpretation = (uintptr_t)block;
		for (int64_t index = m_arenas.size - 1; index >= 0; index--) {
			uintptr_t arena_buffer = (uintptr_t)m_arenas[index].m_initial_buffer;
			if (arena_buffer <= block_reinterpretation && arena_buffer + m_arenas[index].m_allocators.size * m_arenas[index].m_size_per_allocator >= block_reinterpretation) {
				m_arenas[index].Deallocate<trigger_error_if_not_found>(block);
				return;
			}
		}
		if constexpr (trigger_error_if_not_found) {
			ECS_ASSERT(false);
		}
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, ResizableMemoryArena::Deallocate, const void*);

	void ResizableMemoryArena::CreateArena() {
		void* arena_allocation = m_backup->Allocate(MemoryArena::MemoryOf(m_new_allocator_count, m_new_blocks_per_allocator), 8);
		void* buffer = m_backup->Allocate(m_new_arena_capacity, 8);
#ifdef ECSENGINE_DEBUG
		void* debug_buffer = m_backup->Allocate(containers::BlockRange::MemoryOfDebug(m_new_blocks_per_allocator) * m_new_allocator_count);
#endif
		ECS_ASSERT(arena_allocation != nullptr && buffer != nullptr);
		m_arenas.Add(MemoryArena(
			arena_allocation,
			buffer,
			m_new_arena_capacity,
			m_new_allocator_count,
			m_new_blocks_per_allocator
		));
#ifdef ECSENGINE_DEBUG
		m_arenas[m_arenas.size - 1].SetDebugBuffer((unsigned int*)debug_buffer, m_new_blocks_per_allocator);
#endif
	}

	void ResizableMemoryArena::CreateArena(unsigned int arena_capacity, unsigned int allocator_count, unsigned int blocks_per_allocator) {
		void* arena_allocation = m_backup->Allocate(MemoryArena::MemoryOf(allocator_count, blocks_per_allocator), 8);
		void* buffer = m_backup->Allocate(m_new_arena_capacity, 8);
		m_arenas.Add(MemoryArena(
			arena_allocation,
			buffer,
			arena_capacity,
			allocator_count,
			blocks_per_allocator
		));
	}

	// ---------------------------------------------------- Thread safe variants -----------------------------------------

	void* ResizableMemoryArena::Allocate_ts(size_t size, size_t alignment) {
		for (int64_t index = m_arenas.size - 1; index >= 0; index--) {
			void* allocation = m_arenas[index].Allocate_ts(size, alignment);
			if (allocation != nullptr) {
				return allocation;
			}
		}
		unsigned int current_arena_size = m_arenas.size;
		bool try_lock = m_lock.try_lock();
		if (try_lock) {
			CreateArena();
			m_lock.unlock();
		}
		else {
			while (current_arena_size == m_arenas.size) {
				_mm_pause();
			}
		}
		return m_arenas[m_arenas.size - 1].Allocate(size, alignment);
	}

	template<bool trigger_error_if_not_found>
	void ResizableMemoryArena::Deallocate_ts(const void* block)
	{
		uintptr_t block_reinterpretation = (uintptr_t)block;
		for (int64_t index = m_arenas.size - 1; index >= 0; index--) {
			uintptr_t arena_buffer = (uintptr_t)m_arenas[index].m_initial_buffer;
			if (arena_buffer <= block_reinterpretation && arena_buffer + m_arenas[index].m_allocators.size * m_arenas[index].m_size_per_allocator >= block_reinterpretation) {
				m_arenas[index].Deallocate_ts<trigger_error_if_not_found>(block);
				break;
			}
		}
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, ResizableMemoryArena::Deallocate_ts, const void*);

}