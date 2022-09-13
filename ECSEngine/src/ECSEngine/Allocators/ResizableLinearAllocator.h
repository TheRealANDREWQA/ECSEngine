#pragma once
#include "AllocatorTypes.h"
#include "../Core.h"
#include "../Internal/Multithreading/ConcurrentPrimitives.h"
#include "../Utilities/StackScope.h"

namespace ECSEngine {

	// Allocates from the stack a buffer and then uses malloc to allocate bigger buffers
	// It also creates a stack scope to release any heap allocations made 
#define ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(name, stack_capacity, heap_capacity)	void* allocation##name = ECS_STACK_ALLOC(stack_capacity); \
																					ResizableLinearAllocator name(allocation##name, stack_capacity, heap_capacity, {nullptr}); \
																					StackScope<ResizableLinearAllocatorScopeDeallocator> scope##name({ &name });
																					

	struct ECSENGINE_API ResizableLinearAllocator
	{
		ResizableLinearAllocator();
		ResizableLinearAllocator(void* buffer, size_t capacity, size_t backup_size, AllocatorPolymorphic allocator);

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(ResizableLinearAllocator);

		void* Allocate(size_t size, size_t alignment = 8);

		void Deallocate(const void* block);

		void SetMarker();

		// Clears all the allocated memory from the backup - the initial buffer is not cleared
		// since it can be taken from the stack
		void ClearBackup();

		// Clears all the allocated memory from the backup - the initial buffer is not cleared
		// since it can be taken from the stack and it returns the top to 0
		void Clear();

		// Clears all the allocated memory from the backup. If the initial buffer is also from the
		// given allocator, it will deallocate it as well
		void Free();

		size_t GetMarker() const;

		void ReturnToMarker(size_t marker);

		// Returns true if the pointer was allocated from this allocator
		bool Belongs(const void* buffer) const;

		// ---------------------- Thread safe variants -----------------------------

		void* Allocate_ts(size_t size, size_t alignment = 8);

		void Deallocate_ts(const void* block);

		void SetMarker_ts();

		SpinLock m_spin_lock;
		void* m_initial_buffer;
		size_t m_initial_capacity;

		// Similar to a resizable stream
		void** m_allocated_buffers;
		unsigned int m_allocated_buffer_capacity;
		unsigned int m_allocated_buffer_size;

		size_t m_top;
		size_t m_marker;
		size_t m_backup_size;
		AllocatorPolymorphic m_backup;
	};

	struct ResizableLinearAllocatorScopeDeallocator {
		void operator() () {
			allocator->ClearBackup();
		}
		ResizableLinearAllocator* allocator;
	};

}