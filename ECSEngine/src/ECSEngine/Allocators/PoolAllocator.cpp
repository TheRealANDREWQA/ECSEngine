#include "ecspch.h"
#include "PoolAllocator.h"

namespace ECSEngine {

	//PoolAllocator::PoolAllocator(void* buffer, size_t pool_count, size_t pool_size) : m_buffer((unsigned char*)buffer), m_pool_count()
	//{
	//}

	//void* PoolAllocator::Allocate(size_t size, size_t alignment) {
	//	size_t chunks = size / m_pool_size;
	//	size_t pool_index
	//	ECS_ASSERT(pool_index < m_pool_count);

	//	return m_buffer + pool_index * m_pool_size;

	//}

	//template<bool trigger_error_if_not_found>
	//void PoolAllocator::Deallocate(const void* block) {
	//	ECS_ASSERT(block != nullptr);

	//	size_t pool_index = (size_t)((uintptr_t)block - (uintptr_t)m_buffer) / m_pool_size;

	//	if constexpr (trigger_error_if_not_found) {
	//		ECS_ASSERT(pool_index >= 0 && pool_index < m_pool_count);
	//		m_free_list.Free(pool_index);
	//	}
	//	else {
	//		if (pool_index >= 0 && pool_index < m_pool_count) {
	//			m_free_list.Free(pool_index);
	//		}
	//	}
	//}
	//
	//ECS_TEMPLATE_FUNCTION_BOOL(void, PoolAllocator::Deallocate, const void*);

	//void PoolAllocator::Clear() {
	//	memset
	//}

	//// ---------------------- Thread safe variants -----------------------------

	//void* PoolAllocator::Allocate_ts(size_t size, size_t alignment) {
	//	
	//	m_spin_lock.lock();
	//	void* buffer = Allocate(size, alignment);
	//	m_spin_lock.unlock();

	//	return buffer;
	//}

	//template<bool trigger_error_if_not_found>
	//void PoolAllocator::Deallocate_ts(const void* block) {
	//	m_spin_lock.lock();
	//	Deallocate<trigger_error_if_not_found>(block);
	//	m_spin_lock.unlock();
	//}

	//ECS_TEMPLATE_FUNCTION_BOOL(void, PoolAllocator::Deallocate_ts, const void*);

}
