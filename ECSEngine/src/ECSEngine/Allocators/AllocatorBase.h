#pragma once
#include "AllocatorTypes.h"
#include "../Multithreading/ConcurrentPrimitives.h"

namespace ECSEngine {

	// Don't implement the SetDebugMode and SetProfilingMode directly in the header
	// Since this would require the inclusion of the headers for AllocatorCallsDebug and AllocatorProfiling
	// Those would probably would not be useful for the user, and it can pollute compile time and with names
	// So, we can just implement them in the .cpp and have the template be exported
	struct ECSENGINE_API AllocatorBase {
		// Set the allocator to default to crashing on allocation failure, since that's the most appropriate
		// Action for most allocators
		ECS_INLINE AllocatorBase(ECS_ALLOCATOR_TYPE allocator_type) : m_allocator_type(allocator_type), m_crash_on_allocation_failure(true),
			m_debug_mode(false), m_profiling_mode(false) {}

		ECS_INLINE void Lock() {
			m_lock.Lock();
		}

		ECS_INLINE void Unlock() {
			m_lock.Unlock();
		}

		ECS_INLINE void ExitDebugMode() {
			m_debug_mode = false;
		}

		ECS_INLINE void ExitProfilingMode() {
			m_profiling_mode = false;
		}

		ECS_INLINE void ExitCrashOnAllocationFailure() {
			m_crash_on_allocation_failure = false;
		}

		void SetDebugMode(const char* name = nullptr, bool resizable = false);

		void SetProfilingMode(const char* name);

		ECS_INLINE void SetCrashOnAllocationFailure() {
			m_crash_on_allocation_failure = true;
		}

		SpinLock m_lock;
		bool m_debug_mode;
		bool m_profiling_mode;
		bool m_crash_on_allocation_failure;
		ECS_ALLOCATOR_TYPE m_allocator_type;
	};

}