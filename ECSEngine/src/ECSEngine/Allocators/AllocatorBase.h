#pragma once
#include "AllocatorTypes.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "../Utilities/Assert.h"
#include "../Utilities/DebugInfo.h"

namespace ECSEngine {

	// Some virtual functions are implemented directly here to ease the ability to write custom allocators, that do not need the entire
	// Function set, and as such, no need to implemented every function.
	// The default Ts (threadsafe functions) simply guard the single threaded function with the default lock
	// Don't implement the SetDebugMode and SetProfilingMode directly in the header
	// Since this would require the inclusion of the headers for AllocatorCallsDebug and AllocatorProfiling
	// Those would probably would not be useful for the user, and it can pollute compile time and with names
	// So, we can just implement them in the .cpp and have the template be exported
	struct ECSENGINE_API AllocatorBase {
		// Set the allocator to default to crashing on allocation failure, since that's the most appropriate
		// Action for most allocators
		ECS_INLINE AllocatorBase(ECS_ALLOCATOR_TYPE allocator_type) : m_allocator_type(allocator_type), m_crash_on_allocation_failure(true),
			m_debug_mode(false), m_profiling_mode(false) {}

		virtual void* Allocate(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) = 0;

		template<typename T>
		ECS_INLINE T* Allocate(DebugInfo debug_info = ECS_DEBUG_INFO) {
			return (T*)Allocate(sizeof(T), alignof(T), debug_info);
		}

		virtual void* AllocateTs(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) {
			m_lock.Lock();
			void* pointer = Allocate(size, alignment, debug_info);
			m_lock.Unlock();
			return pointer;
		}

		// Returns true if the given buffer belongs to this allocator's instance (not a guarantee that it was allocated from,
		// Only that it is in range), else false
		virtual bool Belongs(const void* buffer) const = 0;

		// Implementation function that should return true if the buffer was deallocated, else false
		virtual bool DeallocateNoAssert(const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) = 0;

		virtual bool DeallocateNoAssertTs(const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
			m_lock.Lock();
			bool was_deallocated = DeallocateNoAssert(buffer, debug_info);
			m_lock.Unlock();
			return was_deallocated;
		}

		// It asserts that the deallocation was successful
		ECS_INLINE void Deallocate(const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
			ECS_ASSERT(DeallocateNoAssert(buffer, debug_info));
		}
		
		// It asserts that the deallocation was successful
		ECS_INLINE void DeallocateTs(const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
			ECS_ASSERT(DeallocateNoAssertTs(buffer, debug_info));
		}

		ECS_INLINE bool DeallocateIfBelongs(const void* buffer, DebugInfo debug_info = ECS_DEBUG_INFO) {
			if (buffer != nullptr && Belongs(buffer)) {
				Deallocate(buffer);
				return true;
			}
			return false;
		}

		virtual void* Reallocate(const void* buffer, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) = 0;

		virtual void* ReallocateTs(const void* buffer, size_t new_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) {
			m_lock.Lock();
			void* new_pointer = Reallocate(buffer, new_size, alignment, debug_info);
			m_lock.Unlock();
			return new_pointer;
		}

		virtual void Clear(DebugInfo debug_info = ECS_DEBUG_INFO) = 0;

		// The parameter controls whether or not it should assert that this allocator is standalone (i.e. capable
		// Of deallocating its memory without needing an allocator parameter)
		virtual void Free(bool assert_that_is_standalone = false, DebugInfo debug_info = ECS_DEBUG_INFO) {
			ECS_ASSERT(false, "Unimplemented allocator Free() function!");
		}

		virtual void FreeFrom(AllocatorBase* backup_allocator, DebugInfo debug_info = ECS_DEBUG_INFO) {
			ECS_ASSERT(false, "Unimplemented allocator FreeFrom() function!");
		}

		// Returns the buffer from which the allocator was initialized. It doesn't make sense for the resizable allocators
		// (MemoryManager, ResizableLinearAllocator). It returns nullptr for these
		virtual void* GetAllocatedBuffer() const {
			ECS_ASSERT(false, "Unimplemented allocator GetAllocatedBuffer() function!");
			return nullptr;
		}

		// Returns true if it is empty, else false
		virtual bool IsEmpty() const {
			ECS_ASSERT(false, "Unimplemented allocator IsEmpty() function!");
			return false;
		}

		// The current amount of bytes in use from the allocator
		virtual size_t GetCurrentUsage() const {
			ECS_ASSERT(false, "Unimplemented allocator GetCurrentUsage() function!");
			return 0;
		}

		// Fills in the regions of memory that are allocated from this instance. Returns the number of regions filled in.
		// The region_pointers and region_size pointers must be parallel
		virtual size_t GetRegions(void** region_pointers, size_t* region_size, size_t pointer_capacity) const {
			ECS_ASSERT(false, "Unimplemented allocator GetRegions() function!");
			return 0;
		}

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

	// Declare the global malloc allocator here such that is accessible everywhere.
	// Implemented in MallocAllocator.cpp
	ECSENGINE_API extern AllocatorBase* ECS_MALLOC_ALLOCATOR;

}