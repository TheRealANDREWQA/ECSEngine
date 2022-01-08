#include "ecspch.h"
#include "MemoryManager.h"
#include "../Utilities/Assert.h"
#include "../Utilities/Function.h"

#ifndef ECS_GLOBAL_MANAGER_SIZE
#define ECS_GLOBAL_MANAGER_SIZE 8
#endif

#ifndef ECS_MEMORY_MANAGER_SIZE
#define ECS_MEMORY_MANAGER_SIZE 8
#endif

namespace ECSEngine {

	// ----------------------------------------------------- Global Memory Manager ---------------------------------------------------

	GlobalMemoryManager::GlobalMemoryManager(size_t size, size_t maximum_pool_count, size_t new_allocation_size) : m_allocator_count(0) {
		m_allocators = new MultipoolAllocator[ECS_GLOBAL_MANAGER_SIZE];
		m_new_allocation_size = new_allocation_size;
		m_maximum_pool_count = maximum_pool_count;
		CreateAllocator(size, maximum_pool_count);
	}

	void GlobalMemoryManager::CreateAllocator(size_t size, size_t maximum_pool_count) {
		ECS_ASSERT(m_allocator_count < ECS_GLOBAL_MANAGER_SIZE);
		void* allocation = (void*) (new unsigned char[MultipoolAllocator::MemoryOf(maximum_pool_count, size)]);
		m_allocators[m_allocator_count] = MultipoolAllocator((unsigned char*)allocation, size, maximum_pool_count);
		m_allocator_count++;
	}

	void GlobalMemoryManager::Trim()
	{
		for (size_t index = 1; index < m_allocator_count; index++) {
			if (m_allocators[index].IsEmpty()) {
				delete[] m_allocators[index].GetAllocatedBuffer();
				m_allocator_count--;
				m_allocators[index] = m_allocators[m_allocator_count];
				index--;
			}
		}
	}

	void GlobalMemoryManager::ReleaseResources()
	{
		for (size_t index = 0; index < m_allocator_count; index++) {
			delete[] m_allocators[index].GetAllocatedBuffer();
		}
		delete[] m_allocators;
	}

	void* GlobalMemoryManager::Allocate(size_t size, size_t alignment) {
		for (size_t index = 0; index < m_allocator_count; index++) {
			void* allocation = m_allocators[index].Allocate(size, alignment);
			if ( allocation != nullptr )
				return allocation;
		}

		CreateAllocator(m_new_allocation_size, m_maximum_pool_count);
		return m_allocators[m_allocator_count - 1].Allocate(size, alignment);
	}

	template<bool trigger_error_if_not_found>
	void GlobalMemoryManager::Deallocate(const void* block) {
		for (size_t index = 0; index < m_allocator_count; index++) {
			const void* buffer = m_allocators[index].GetAllocatedBuffer();
			size_t capacity = m_allocators[index].GetSize();
			if (buffer <= block && function::OffsetPointer(buffer, capacity) > block) {
				m_allocators[index].Deallocate<trigger_error_if_not_found>(block);
				if (index > 0 && m_allocators[index].IsEmpty()) {
					delete[] m_allocators[index].GetAllocatedBuffer();
					m_allocator_count--;
					m_allocators[index] = m_allocators[m_allocator_count];
				}
				return;
			}
		}
		if constexpr (trigger_error_if_not_found) {
			ECS_ASSERT(false);
		}
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, GlobalMemoryManager::Deallocate, const void*);

	// ---------------------- Thread safe variants -----------------------------

	void* GlobalMemoryManager::Allocate_ts(size_t size, size_t alignment) {
		for (size_t index = 0; index < m_allocator_count; index++) {
			void* allocation = m_allocators[index].Allocate_ts(size, alignment);
			if ( allocation != nullptr )
				return allocation;
		}
		m_spin_lock.lock();
		CreateAllocator(m_new_allocation_size, m_maximum_pool_count);
		void* allocation = m_allocators[m_allocator_count - 1].Allocate(size, alignment);
		m_spin_lock.unlock();
		return allocation;
	}

	template<bool trigger_error_if_not_found>
	void GlobalMemoryManager::Deallocate_ts(const void* block) {
		for (size_t index = 0; index < m_allocator_count; index++) {
			const void* buffer = m_allocators[index].GetAllocatedBuffer();
			size_t capacity = m_allocators[index].GetSize();
			if (buffer <= block && function::OffsetPointer(buffer, capacity) > block) {
				m_allocators[index].Deallocate_ts<trigger_error_if_not_found>(block);
				return;
			}
		}
		if constexpr (trigger_error_if_not_found) {
			ECS_ASSERT(false);
		}
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, GlobalMemoryManager::Deallocate_ts, const void*);

	// ----------------------------------------------------- Memory Manager ---------------------------------------------------

	MemoryManager::MemoryManager() : m_backup(nullptr), m_allocators(nullptr), m_new_allocation_size(0), m_maximum_pool_count(0), m_allocator_count(0) {}

	MemoryManager::MemoryManager(size_t size, size_t maximum_pool_count, size_t new_allocation_size, GlobalMemoryManager* backup) : m_allocator_count(0) {
		m_backup = backup;
		void* allocation = backup->Allocate((sizeof(MultipoolAllocator) + sizeof(void*) + sizeof(size_t)) * ECS_MEMORY_MANAGER_SIZE, alignof(MultipoolAllocator));
		m_allocators = (MultipoolAllocator*)allocation;
		m_new_allocation_size = new_allocation_size;
		m_maximum_pool_count = maximum_pool_count;
		CreateAllocator(size, maximum_pool_count);
	}

	void MemoryManager::Free() {
		for (size_t index = 0; index < m_allocator_count; index++) {
			m_backup->Deallocate(m_allocators[index].GetAllocatedBuffer());
		}
		m_backup->Deallocate((void*)m_allocators);
	}

	void MemoryManager::CreateAllocator(size_t size, size_t maximum_pool_count) {
		ECS_ASSERT(m_allocator_count < ECS_MEMORY_MANAGER_SIZE);
		void* allocation = m_backup->Allocate(MultipoolAllocator::MemoryOf(maximum_pool_count, size));
		m_allocators[m_allocator_count] = MultipoolAllocator((unsigned char*)allocation, size, maximum_pool_count);
		m_allocator_count++;
	}

	void* MemoryManager::Allocate(size_t size, size_t alignment) {
		for (size_t index = 0; index < m_allocator_count; index++) {
			void* allocation = m_allocators[index].Allocate(size, alignment);
			if (allocation != nullptr)
				return allocation;
		}

		CreateAllocator(m_new_allocation_size, m_maximum_pool_count);
		return m_allocators[m_allocator_count - 1].Allocate(size, alignment);
	}

	template<bool trigger_error_if_not_found>
	void MemoryManager::Deallocate(const void* block) {
		for (size_t index = 0; index < m_allocator_count; index++) {
			const void* buffer = m_allocators[index].GetAllocatedBuffer();
			size_t capacity = m_allocators[index].GetSize();
			if (buffer <= block && function::OffsetPointer(buffer, capacity) > block) {
				m_allocators[index].Deallocate<trigger_error_if_not_found>(block);
				if (index > 0 && m_allocators[index].IsEmpty()) {
					m_backup->Deallocate(m_allocators[index].GetAllocatedBuffer());
					m_allocator_count--;
					m_allocators[index] = m_allocators[m_allocator_count];
				}
				return;
			}
		}
		if constexpr (trigger_error_if_not_found) {
			ECS_ASSERT(false);
		}
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, MemoryManager::Deallocate, const void*);

	void MemoryManager::Lock()
	{
		m_spin_lock.lock();
	}

	void MemoryManager::Unlock()
	{
		m_spin_lock.unlock();
	}

	void MemoryManager::Trim()
	{
		for (size_t index = 1; index < m_allocator_count; index++) {
			if (m_allocators[index].IsEmpty()) {
				m_backup->Deallocate(m_allocators[index].GetAllocatedBuffer());
				m_allocator_count--;
				m_allocators[index] = m_allocators[m_allocator_count];
				index--;
			}
		}
	}

	// ---------------------- Thread safe variants -----------------------------

	void* MemoryManager::Allocate_ts(size_t size, size_t alignment) {
		for (size_t index = 0; index < m_allocator_count; index++) {
			void* allocation = m_allocators[index].Allocate_ts(size, alignment);
			if (allocation != nullptr)
				return allocation;
		}
		m_spin_lock.lock();
		CreateAllocator(m_new_allocation_size, m_maximum_pool_count);
		void* allocation = m_allocators[m_allocator_count - 1].Allocate(size, alignment);
		m_spin_lock.unlock();
		return allocation;
	}

	template<bool trigger_error_if_not_found>
	void MemoryManager::Deallocate_ts(const void* block) {
		for (size_t index = 0; index < m_allocator_count; index++) {
			const void* buffer = m_allocators[index].GetAllocatedBuffer();
			size_t capacity = m_allocators[index].GetSize();
			if (buffer <= block && function::OffsetPointer(buffer, capacity) > block) {
				m_allocators[index].Deallocate_ts<trigger_error_if_not_found>(block);
				return;
			}
		}
		if constexpr (trigger_error_if_not_found) {
			ECS_ASSERT(false);
		}
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, MemoryManager::Deallocate_ts, const void*);

}
