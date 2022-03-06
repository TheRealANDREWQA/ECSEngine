#pragma once
#include "../Core.h"
#include "../Containers/BooleanBitField.h"
#include "../Internal/Multithreading/ConcurrentPrimitives.h"

namespace ECSEngine {

	/* the free list is going to receive its own pool, so pool_count needs to be 1 more than the desired number */
	struct ECSENGINE_API PoolAllocator
	{
	public:
		PoolAllocator(unsigned char* buffer, size_t pool_count, size_t pool_size) 
			: m_buffer(buffer + pool_size * sizeof(unsigned short)), m_pool_count(pool_count - 1), m_pool_size(pool_size), 
			m_free_list((byte64*)buffer, (size_t)(pool_count - 1), true) {}

		void* Allocate(size_t size, size_t alignment = 8);

		template<bool trigger_error_if_not_found = true>
		void Deallocate(const void* block);

		void Clear(); 

		// ---------------------------------------------- Thread safe variants -------------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = 8);

		template<bool trigger_error_if_not_found = true>
		void Deallocate_ts(const void* block);

	//private:
		unsigned char* m_buffer;
		size_t m_pool_count;
		size_t m_pool_size;
		SpinLock m_spin_lock;
		BooleanBitField m_free_list;
	};

}

