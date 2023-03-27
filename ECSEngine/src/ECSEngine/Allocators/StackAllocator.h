#pragma once
#include "../Core.h"
#include "../Multithreading/ConcurrentPrimitives.h"

namespace ECSEngine {

	struct ECSENGINE_API StackAllocator
	{
		StackAllocator(unsigned char* buffer, size_t capacity) : m_buffer(buffer), m_capacity(capacity), m_top(0), m_marker(0),
			m_last_top(0), m_spin_lock() {}

		void* Allocate(size_t size, size_t alignment = 8);
		
		void Deallocate(const void* block);

		void Deallocate();
		
		void Clear();

		bool Belongs(const void* pointer) const;

		void* GetAllocatedBuffer();

		void SetMarker();

		void ReturnToMarker();

		size_t GetTop() const;

		size_t GetCapacity() const;

		// ---------------------------------------------- Thread safe variants ---------------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = 8);
		
		void Deallocate_ts(const void* block);
		void Deallocate_ts();
		
		void SetMarker_ts();
		void ReturnToMarker_ts();
	
		unsigned char* m_buffer;
		size_t m_capacity;
		size_t m_top;
		size_t m_marker;
		size_t m_last_top;
		SpinLock m_spin_lock;
	};
}

