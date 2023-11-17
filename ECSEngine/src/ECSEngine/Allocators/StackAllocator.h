#pragma once
#include "../Core.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "../Utilities/DebugInfo.h"

namespace ECSEngine {

	struct ECSENGINE_API StackAllocator
	{
		ECS_INLINE StackAllocator(void* buffer, size_t capacity) : m_buffer((unsigned char*)buffer), m_capacity(capacity), m_top(0), m_marker(0),
			m_last_top(0), m_spin_lock(), m_debug_mode(false), m_profiling_mode(false) {}

		void* Allocate(size_t size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO);
		
		template<bool trigger_error_if_not_found = true>
		bool Deallocate(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		void Deallocate(DebugInfo debug_info = ECS_DEBUG_INFO);
		
		void Clear(DebugInfo debug_info = ECS_DEBUG_INFO);

		bool Belongs(const void* pointer) const;

		void* GetAllocatedBuffer() const;

		void SetMarker();

		void ReturnToMarker(DebugInfo debug_info = ECS_DEBUG_INFO);

		size_t GetTop() const;

		size_t GetCapacity() const;

		ECS_INLINE bool IsEmpty() const {
			return m_top == 0;
		}

		ECS_INLINE void Lock() {
			m_spin_lock.Lock();
		}

		ECS_INLINE void Unlock() {
			m_spin_lock.Unlock();
		}

		ECS_INLINE size_t GetCurrentUsage() const {
			return m_top;
		}

		void ExitDebugMode();

		void ExitProfilingMode();

		void SetDebugMode(const char* name = nullptr, bool resizable = false);

		void SetProfilingMode(const char* name);

		// Region start and region size are parallel arrays. Returns the count of regions
		// Pointer capacity must represent the count of valid entries for the given pointers
		size_t GetAllocatedRegions(void** region_start, size_t* region_size, size_t pointer_capacity) const;

		// ---------------------------------------------- Thread safe variants ---------------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO);
		
		template<bool trigger_error_if_not_found = true>
		bool Deallocate_ts(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		void SetMarker_ts();
		void ReturnToMarker_ts(DebugInfo debug_info = ECS_DEBUG_INFO);
	
		unsigned char* m_buffer;
		size_t m_capacity;
		size_t m_top;
		size_t m_marker;
		size_t m_last_top;
		SpinLock m_spin_lock;
		bool m_debug_mode;
		bool m_profiling_mode;
	};
}

