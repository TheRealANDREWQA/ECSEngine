#pragma once
#include "../Core.h"
#include "../Internal/Multithreading/ConcurrentPrimitives.h"

namespace ECSEngine {

	struct ECSENGINE_API LinearAllocator
	{
	public:
		LinearAllocator() : m_buffer(nullptr), m_capacity(0), m_top(0), m_marker(0) {}
		LinearAllocator(void* buffer, size_t capacity) : m_buffer((unsigned char*)buffer), m_capacity(capacity), m_top(0), 
			m_marker(0), m_spin_lock() {}
		
		void* Allocate(size_t size, size_t alignment = 8);
		void Deallocate(const void* block);
		void SetMarker();
		void Clear();

		// ---------------------- Thread safe variants -----------------------------
		void* Allocate_ts(size_t size, size_t alignment = 8);
		void Deallocate_ts(const void* block);
		void SetMarker_ts();

	//private:
		SpinLock m_spin_lock;
		void* m_buffer;
		size_t m_capacity;
		size_t m_marker;
		size_t m_top;
	};
}

