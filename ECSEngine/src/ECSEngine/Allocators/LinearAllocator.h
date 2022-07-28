#pragma once
#include "../Core.h"
#include "../Internal/Multithreading/ConcurrentPrimitives.h"

namespace ECSEngine {

	struct ECSENGINE_API LinearAllocator
	{
		LinearAllocator() : m_buffer(nullptr), m_capacity(0), m_top(0), m_marker(0) {}
		LinearAllocator(void* buffer, size_t capacity) : m_buffer(buffer), m_capacity(capacity), m_top(0), 
			m_marker(0), m_spin_lock() {}
		
		void* Allocate(size_t size, size_t alignment = 8);

		void Deallocate(const void* block);

		void SetMarker();

		void Clear();

		size_t GetMarker() const;

		void ReturnToMarker(size_t marker);

		// Returns true if the pointer was allocated from this allocator
		bool Belongs(const void* buffer) const;

		// ---------------------- Thread safe variants -----------------------------
		
		void* Allocate_ts(size_t size, size_t alignment = 8);

		void Deallocate_ts(const void* block);

		void SetMarker_ts();

		SpinLock m_spin_lock;
		void* m_buffer;
		size_t m_capacity;
		size_t m_marker;
		size_t m_top;
	};
}

