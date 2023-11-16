#pragma once
#include "../Core.h"
#include "../Utilities/DebugInfo.h"
#include "../Multithreading/ConcurrentPrimitives.h"

namespace ECSEngine {

	struct ECSENGINE_API MemoryProtectedAllocator {
		ECS_INLINE MemoryProtectedAllocator() {
			memset(this, 0, sizeof(*this));
		}
		
		// If the chunk size is 0, then each allocation will be on a separate page.
		// If the chunk size is given, then it will make allocations of that chunk size
		// When the chunk size is given, you can specify if the allocations should be made
		// like a linear allocator or as a multipool allocator
		MemoryProtectedAllocator(size_t chunk_size, bool linear_chunks = false);

		void* Allocate(size_t size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO);

		template<bool trigger_error_if_not_found = true>
		bool Deallocate(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		void* Reallocate(const void* block, size_t new_size, size_t alignment, DebugInfo debug_info = ECS_DEBUG_INFO);

		bool Belongs(const void* block) const;

		void Clear(DebugInfo debug_info = ECS_DEBUG_INFO);

		void Free(DebugInfo debug_info = ECS_DEBUG_INFO);

		bool IsEmpty() const;

		ECS_INLINE void Lock() {
			lock.Lock();
		}

		ECS_INLINE void Unlock() {
			lock.Unlock();
		}

		// Disables write protection for all allocations which were made by this allocator
		// Returns true if it succeeded, else false
		bool DisableWriteProtection() const;

		// Disables write protection for a specific allocation - this works only in non chunk mode
		// The block must have been allocated by this allocator, otherwise it will fail
		// Returns true if it succeeded, else false
		bool DisableWriteProtection(void* block) const;

		// Enables write protection for all allocations which were made by this allocator
		// Returns true if it suceeded, else false
		bool EnableWriteProtection() const;

		// Enables write protection for a specific allocation - this works only in non chunk mode
		// The block must have been allocated by this allocator, otherwise it will fail
		// Returns true if it suceeded, else false. 
		bool EnableWriteProtection(void* block) const;

		void ExitDebugMode();

		// This function is here to make the implementation of AllocatorPolymorphic easier
		ECS_INLINE void ExitProfilingMode() {}

		void SetDebugMode(const char* name = nullptr, bool resizable = false);

		// This function is here to make the implementation of AllocatorPolymorphic easier
		ECS_INLINE void SetProfilingMode(const char* name) {}

		// This function is here to make the implementation of AllocatorPolymorphic easier
		ECS_INLINE size_t GetCurrentUsage() const {
			// This function doesn't really have sense, since this allocator is used mostly
			// To detect bugs and measuring its usage is not really something that should be done
			return 0;
		}

		// ------------------------- Thread safe functions -----------------------------------------

		void* Allocate_ts(size_t size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO);

		template<bool trigger_error_if_not_found = true>
		bool Deallocate_ts(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		void* Reallocate_ts(const void* block, size_t new_size, size_t alignment, DebugInfo debug_info = ECS_DEBUG_INFO);

		union {
			struct {
				void** allocators;
				bool linear_allocators;
			};
			struct {
				void** allocation_pointers;
			};
		};
		// This is the current count of allocators/allocations
		unsigned int count;
		// This is the capacity of allocators/allocations
		unsigned int capacity;
		size_t chunk_size;
		bool debug_mode;
		SpinLock lock;
	};

}