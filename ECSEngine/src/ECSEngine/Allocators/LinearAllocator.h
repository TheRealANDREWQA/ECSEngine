#pragma once
#include "../Core.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "AllocatorTypes.h"
#include "../Utilities/DebugInfo.h"

namespace ECSEngine {

#define ECS_STACK_LINEAR_ALLOCATOR(name, capacity)	char allocator_bytes##name[capacity]; \
													LinearAllocator name(allocator_bytes##name, capacity);

	struct ECSENGINE_API LinearAllocator
	{
		ECS_INLINE LinearAllocator() : m_buffer(nullptr), m_capacity(0), m_top(0), m_marker(0), m_debug_mode(false) {}
		ECS_INLINE LinearAllocator(void* buffer, size_t capacity) : m_buffer(buffer), m_capacity(capacity), m_top(0), 
			m_marker(0), m_spin_lock(), m_debug_mode(false) {}
		LinearAllocator(AllocatorPolymorphic allocator, size_t capacity);

		void* Allocate(size_t size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO);

		template<bool trigger_error_if_not_found = true>
		bool Deallocate(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		void SetMarker();

		void Clear(DebugInfo debug_info = ECS_DEBUG_INFO);

		size_t GetMarker() const;

		void* GetAllocatedBuffer() const;

		void ReturnToMarker(size_t marker, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Returns true if the pointer was allocated from this allocator
		bool Belongs(const void* buffer) const;

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
		
		void SetDebugMode(const char* name = nullptr, bool resizable = false);

		// ---------------------- Thread safe variants -----------------------------
		
		void* Allocate_ts(size_t size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO);

		template<bool trigger_error_if_not_found = true>
		bool Deallocate_ts(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		void SetMarker_ts();

		SpinLock m_spin_lock;
		bool m_debug_mode;
		void* m_buffer;
		size_t m_capacity;
		size_t m_marker;
		size_t m_top;
	};
}

