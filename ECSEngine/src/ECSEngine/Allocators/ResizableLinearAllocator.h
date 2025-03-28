#pragma once
#include "../Core.h"
#include "../Multithreading/ConcurrentPrimitives.h"
#include "../Utilities/StackScope.h"
#include "../Utilities/DebugInfo.h"
#include "AllocatorBase.h"

namespace ECSEngine {

	// Allocates from the stack a buffer and then uses Malloc to allocate bigger buffers
	// It also creates a stack scope to release any heap allocations made 
#define ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(name, stack_capacity, heap_capacity)	void* allocation##name = ECS_STACK_ALLOC(stack_capacity); \
																					ResizableLinearAllocator name(allocation##name, stack_capacity, heap_capacity, {nullptr}); \
																					StackScope<ResizableLinearAllocatorScopeDeallocator> scope##name({ &name });
	
	// The same as the other variant, but instead it uses a different backing allocator																		
#define ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR_BACKUP(name, stack_capacity, heap_capacity, backup) \
	/* We can simply use the other variant and change the backup allocator */ \
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(name, stack_capacity, heap_capacity); \
	name.m_backup = backup;

	struct ECSENGINE_API ResizableLinearAllocator : public AllocatorBase
	{
		ECS_INLINE ResizableLinearAllocator() : AllocatorBase(ECS_ALLOCATOR_RESIZABLE_LINEAR), m_initial_buffer(nullptr), m_initial_capacity(0), m_allocated_buffers(nullptr),
			m_allocated_buffer_capacity(0), m_allocated_buffer_size(0), m_top(0), m_marker(0), m_backup_size(0), m_backup({ nullptr }) {}
		ResizableLinearAllocator(size_t capacity, size_t backup_size, AllocatorPolymorphic allocator);
		ResizableLinearAllocator(void* buffer, size_t capacity, size_t backup_size, AllocatorPolymorphic allocator);

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(ResizableLinearAllocator);

		void* Allocate(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO);

		template<typename T>
		ECS_INLINE T* Allocate(DebugInfo debug_info = ECS_DEBUG_INFO) {
			return (T*)Allocate(sizeof(T), alignof(T), debug_info);
		}

		template<bool trigger_error_if_not_found = true>
		bool Deallocate(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		void SetMarker();

		// Clears all the allocated memory from the backup - the initial buffer is not cleared
		// since it can be taken from the stack
		void ClearBackup(DebugInfo debug_info = ECS_DEBUG_INFO);

		// Clears all the allocated memory from the backup - the initial buffer is not cleared
		// since it can be taken from the stack and it returns the top to 0
		void Clear(DebugInfo debug_info = ECS_DEBUG_INFO);

		// Clears all the allocated memory from the backup. If the initial buffer is also from the
		// given allocator, it will deallocate it as well
		void Free(DebugInfo debug_info = ECS_DEBUG_INFO);

		void GetMarker(size_t* marker, size_t* current_usage) const;

		// It will use the value stored in this structure's marker
		void ReturnToMarker(DebugInfo debug_info = ECS_DEBUG_INFO);

		// You need to restore both values from the GetMarker function
		void ReturnToMarker(size_t marker, size_t usage, DebugInfo debug_info = ECS_DEBUG_INFO);

		// Returns true if the pointer was allocated from this allocator
		bool Belongs(const void* buffer) const;

		ECS_INLINE bool IsEmpty() const {
			return m_top == 0;
		}

		ECS_INLINE size_t GetCurrentUsage() const {
			return m_current_usage;
		}

		// Region start and region size are parallel arrays. Returns the count of regions
		// Pointer capacity must represent the count of valid entries for the given pointers
		size_t GetAllocatedRegions(void** region_start, size_t* region_size, size_t pointer_capacity) const;

		// ---------------------- Thread safe variants -----------------------------

		void* Allocate_ts(size_t size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO);

		template<bool trigger_error_if_not_found = true>
		bool Deallocate_ts(const void* block, DebugInfo debug_info = ECS_DEBUG_INFO);

		void SetMarker_ts();

		// This is not the real buffer received in the constructor
		// It is offsetted by a count of void* in order to keep the m_allocated_buffers
		// inside the initial_allocation. So the buffer in the constructor is actually the
		// m_allocated_buffers
		void* m_initial_buffer;
		size_t m_initial_capacity;

		// Similar to a resizable stream
		void** m_allocated_buffers;
		unsigned short m_allocated_buffer_capacity;
		unsigned short m_allocated_buffer_size;
		bool m_initial_allocation_from_backup;

		size_t m_top;
		size_t m_marker;
		size_t m_backup_size;
		size_t m_current_usage;
		// We need to keep a separate count for the current usage
		// In order to properly restore it
		size_t m_marker_current_usage;
		AllocatorPolymorphic m_backup;

	};

	struct ResizableLinearAllocatorScopeDeallocator {
		void operator() () {
			allocator->ClearBackup();
		}
		ResizableLinearAllocator* allocator;
	};

}