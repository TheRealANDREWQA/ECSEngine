#pragma once
#include "../Core.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "../Utilities/DebugInfo.h"
#include "AllocatorBase.h"

namespace ECSEngine {

#define ECS_STACK_LINEAR_ALLOCATOR(name, capacity)	char allocator_bytes##name[capacity]; \
													LinearAllocator name(allocator_bytes##name, capacity);

	struct ECSENGINE_API LinearAllocator : public AllocatorBase
	{
		ECS_INLINE LinearAllocator() : AllocatorBase(ECS_ALLOCATOR_LINEAR), m_buffer(nullptr), m_capacity(0), m_top(0), m_marker(0) {}
		ECS_INLINE LinearAllocator(void* buffer, size_t capacity) : AllocatorBase(ECS_ALLOCATOR_LINEAR), m_buffer(buffer), m_capacity(capacity), m_top(0), 
			m_marker(0) {}

		void* Allocate(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO);

		template<typename T>
		ECS_INLINE T* Allocate(DebugInfo debug_info = ECS_DEBUG_INFO) {
			return (T*)Allocate(sizeof(T), alignof(T), debug_info);
		}

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

		ECS_INLINE size_t GetCurrentUsage() const {
			return m_top;
		}

		// Region start and region size are parallel arrays. Returns the count of regions
		// Pointer capacity must represent the count of valid entries for the given pointers
		size_t GetAllocatedRegions(void** region_start, size_t* region_size, size_t pointer_capacity) const;

		static LinearAllocator InitializeFrom(AllocatorPolymorphic allocator, size_t capacity);

		// ---------------------- Thread safe variants -----------------------------
		
		void* Allocate_ts(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO);

		template<bool trigger_error_if_not_found = true>
		bool Deallocate_ts(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		void SetMarker_ts();

		void* m_buffer;
		size_t m_capacity;
		size_t m_marker;
		size_t m_top;
	};
}

