#include "ecspch.h"
#include "MemoryProtectedAllocator.h"
#include "LinearAllocator.h"
#include "MultipoolAllocator.h"
#include "AllocatorCallsDebug.h"
#include "../Utilities/PointerUtilities.h"
#include "../OS/Memory.h"
#include "../Utilities/Utilities.h"

#define DEFAULT_PAGE_SIZE ECS_KB * 4
#define MULTIPOOL_BLOCK_COUNT ECS_KB * 8

namespace ECSEngine {

	static void* AllocateBaseAllocator(const MemoryProtectedAllocator* allocator) {
		void* virtual_allocation = OS::VirtualAllocation(allocator->chunk_size);
		if (allocator->linear_allocators) {
			LinearAllocator* initial_allocator = (LinearAllocator*)Malloc(sizeof(LinearAllocator));
			*initial_allocator = LinearAllocator(virtual_allocation, allocator->chunk_size);
			// Set this allocator to not crash on failure
			initial_allocator->ExitCrashOnAllocationFailure();
			return initial_allocator;
		}
		else {
			MultipoolAllocator* initial_allocator = (MultipoolAllocator*)Malloc(sizeof(MultipoolAllocator));
			void* block_range_buffer = Malloc(MultipoolAllocator::MemoryOf(MULTIPOOL_BLOCK_COUNT));
			*initial_allocator = MultipoolAllocator(virtual_allocation, block_range_buffer, allocator->chunk_size, MULTIPOOL_BLOCK_COUNT);
			// Set this allocator to not crash on failure
			initial_allocator->ExitCrashOnAllocationFailure();
			return initial_allocator;
		}
	}

	static void* AllocateFromBase(MemoryProtectedAllocator* allocator, size_t size, size_t alignment) {
		if (allocator->linear_allocators) {
			LinearAllocator* current_allocator = (LinearAllocator*)allocator->allocators[allocator->count - 1];
			return current_allocator->Allocate(size, alignment);
		}
		else {
			MultipoolAllocator* current_allocator = (MultipoolAllocator*)allocator->allocators[allocator->count - 1];
			return current_allocator->Allocate(size, alignment);
		}
	}

	static void* AllocateFromBaseTs(MemoryProtectedAllocator* allocator, size_t size, size_t alignment) {
		if (allocator->linear_allocators) {
			LinearAllocator* current_allocator = (LinearAllocator*)allocator->allocators[allocator->count - 1];
			return current_allocator->Allocate_ts(size, alignment);
		}
		else {
			MultipoolAllocator* current_allocator = (MultipoolAllocator*)allocator->allocators[allocator->count - 1];
			return current_allocator->Allocate_ts(size, alignment);
		}
	}

	static void ResizeAllocators(MemoryProtectedAllocator* allocator) {
		unsigned int new_capacity = (unsigned int)((float)allocator->capacity * 1.5f + 1);
		void** new_pointers = (void**)Malloc(sizeof(void*) * new_capacity);
		memcpy(new_pointers, allocator->allocators, sizeof(void*) * allocator->capacity);
		
		if (allocator->capacity > 0) {
			Free(allocator->allocators);
		}

		allocator->capacity = new_capacity;
		allocator->allocators = new_pointers;
	}

	static void ResizeIndividualBlocks(MemoryProtectedAllocator* allocator) {
		unsigned int new_capacity = (unsigned int)((float)allocator->capacity * 1.5f + 1);
		void** new_pointers = (void**)Malloc(sizeof(void*) * new_capacity);
		memcpy(new_pointers, allocator->allocation_pointers, sizeof(void*) * allocator->capacity);
		
		if (allocator->capacity > 0) {
			Free(allocator->allocation_pointers);
		}

		allocator->allocation_pointers = new_pointers;
		allocator->capacity = new_capacity;
	}

	static void* CreateVirtualAllocation(size_t size, size_t alignment) {
		void* allocation = OS::VirtualAllocation(size + alignment);
		void* original_allocation = allocation;
		allocation = (void*)AlignPointerStack((uintptr_t)allocation, alignment);
		unsigned char* char_value = (unsigned char*)allocation;
		char_value[-1] = (unsigned char)PointerDifference(allocation, original_allocation);
	
		return allocation;
	}

	static void* GetOriginalPointerFromVirtualAllocation(void* allocation) {
		unsigned char* char_allocation = (unsigned char*)allocation;
		ECS_ASSERT(char_allocation[-1] <= ECS_CACHE_LINE_SIZE, "Invalid virtual allocation offset");
		return OffsetPointer(allocation, -(int64_t)char_allocation[-1]);
	}

	static void FreeAllocatorBase(MemoryProtectedAllocator* allocator, unsigned int index) {
		if (allocator->linear_allocators) {
			LinearAllocator* current_allocator = (LinearAllocator*)allocator->allocators[index];
			OS::VirtualDeallocation(current_allocator->GetAllocatedBuffer());
		}
		else {
			MultipoolAllocator* current_allocator = (MultipoolAllocator*)allocator->allocators[index];
			OS::VirtualDeallocation(current_allocator->GetAllocatedBuffer());
			// We also need to normal Free the block range buffer here
			Free((void*)current_allocator->m_range.GetAllocatedBuffer());
		}
	}

	MemoryProtectedAllocator::MemoryProtectedAllocator(size_t _chunk_size, bool linear_chunks) : AllocatorBase(ECS_ALLOCATOR_MEMORY_PROTECTED)
	{
		count = 0;
		capacity = 0;
		chunk_size = _chunk_size;
		if (chunk_size > 0) {
			linear_allocators = linear_chunks;

			allocators = (void**)Malloc(sizeof(void*) * 1);
			allocators[0] = AllocateBaseAllocator(this);
			count = 1;
			capacity = 1;
		}
	}

	void* MemoryProtectedAllocator::Allocate(size_t size, size_t alignment, DebugInfo debug_info)
	{
		void* allocation = nullptr;

		if (chunk_size > 0) {
			if (size > chunk_size) {
				ECS_ASSERT(m_crash_on_allocation_failure, "MemoryProtectedAllocator allocation size exceeds the chunk size");
				return nullptr;
			}

			allocation = AllocateFromBase(this, size, alignment);
			if (allocation == nullptr) {
				// We need to create a new entry
				if (count == capacity) {
					ResizeAllocators(this);
				}
				allocators[count++] = AllocateBaseAllocator(this);
				allocation = AllocateFromBase(this, size, alignment);
			}
		}
		else {
			// For each individual chunk, do the same trick as for the stack allocator - allocate the size + alignment
			// And offset the pointer to the next natural alignment and write in the previous byte the offset to reach
			// the original allocated pointer
			void* virtual_allocation = CreateVirtualAllocation(size, alignment);

			unsigned int current_count = count;
			if (current_count == capacity) {
				ResizeIndividualBlocks(this);
			}
			allocation_pointers[current_count] = virtual_allocation;
			count++;

			allocation = virtual_allocation;
		}

		if (allocation == nullptr) {
			ECS_ASSERT(m_crash_on_allocation_failure, "MemoryProtectedAllocation allocation could not be fulfilled");
			return nullptr;
		}

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = allocation;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_ALLOCATE;
			tracked.debug_info = debug_info;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_LINEAR, &tracked);
		}
		return allocation;
	}

	template<bool trigger_error_if_not_found>
	bool MemoryProtectedAllocator::Deallocate(const void* block, DebugInfo debug_info)
	{
		bool was_deallocated = false;
		if (chunk_size > 0) {
			// For linear allocators the deallocate does nothing, so don't do anything there
			if (!linear_allocators) {
				unsigned int index = 0;
				for (; index < count; index++) {
					MultipoolAllocator* allocator_pointer = (MultipoolAllocator*)allocators[index];
					if (allocator_pointer->Belongs(block)) {
						was_deallocated = allocator_pointer->Deallocate<trigger_error_if_not_found>(block);
						if (was_deallocated && allocator_pointer->IsEmpty()) {
							// Swap back the current buffer and deallocate the virtual allocation
							count--;
							OS::VirtualDeallocation(allocator_pointer->GetAllocatedBuffer());
							allocators[index] = allocators[count];
							// Decrement the index such that when deallocating the last allocator it won't cause
							// A trigger error since the index will be equal to the allocator count
							index--;
						}
						break;
					}
				}

				if constexpr (trigger_error_if_not_found) {
					if (index == count) {
						ECS_ASSERT(false, "Invalid block for deallocation to MemoryProtectedAllocator");
					}
				}
			}
		}
		else {
			size_t search_index = SearchBytes(allocation_pointers, count, (size_t)block, sizeof(block));
			if (search_index != -1) {
				void* original_pointer = GetOriginalPointerFromVirtualAllocation(allocation_pointers[search_index]);
				OS::VirtualDeallocation(original_pointer);

				was_deallocated = true;
				count--;
				allocation_pointers[search_index] = allocation_pointers[count];
			}

			if constexpr (trigger_error_if_not_found) {
				if (!was_deallocated) {
					ECS_ASSERT(false, "Invalid block for deallocation to MemoryProtectedAllocator");
				}
			}
		}

		if (was_deallocated) {
			if (m_debug_mode) {
				TrackedAllocation tracked;
				tracked.allocated_pointer = block;
				tracked.debug_info = debug_info;
				tracked.function_type = ECS_DEBUG_ALLOCATOR_DEALLOCATE;
				DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_MEMORY_PROTECTED, &tracked);
			}
		}

		return was_deallocated;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, MemoryProtectedAllocator::Deallocate, const void*, DebugInfo);

	void* MemoryProtectedAllocator::Reallocate(const void* block, size_t new_size, size_t alignment, DebugInfo debug_info)
	{
		void* new_allocation = nullptr;

		if (chunk_size > 0) {
			if (linear_allocators) {
				// For linear allocators we just have to allocate a new block
				// Make sure to disable the debug mode to not generate an allocate entry
				bool previous_debug_mode = m_debug_mode;
				m_debug_mode = false;
				new_allocation = Allocate(new_size, alignment);
				m_debug_mode = previous_debug_mode;
			}
			else {
				unsigned int index = 0;
				for (; index < count; index++) {
					MultipoolAllocator* allocator_pointer = (decltype(allocator_pointer))allocators[index];
					if (allocator_pointer->Belongs(block)) {
						new_allocation = allocator_pointer->Reallocate(block, new_size, alignment);
						if (new_allocation == nullptr) {
							// Allocate a new block and deallocate this one
							// Make sure to disable the debug mode to not generate an allocate entry
							bool previous_debug_mode = m_debug_mode;
							m_debug_mode = false;
							new_allocation = Allocate(new_size, alignment);
							m_debug_mode = previous_debug_mode;
							allocator_pointer->Deallocate(block);
						}
						break;
					}
				}

				ECS_ASSERT(index < count, "Invalid reallocate block for MemoryProtectedAllocator");
			}
		}
		else {
			// Allocate a new block and deallocate the old one
			// Make sure to disable the debug mode to not generate an allocate entry or a deallocate entry

			bool previous_debug_mode = m_debug_mode;
			m_debug_mode = false;

			new_allocation = Allocate(new_size, alignment);
			Deallocate(block);

			m_debug_mode = previous_debug_mode;
		}

		// Crashing on allocation failure is handled through the allocate function, although the failure string
		// Might be a bit incorrect (it will says allocation instead of reallocation, but the gist is the same)

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.allocated_pointer = block;
			tracked.secondary_pointer = new_allocation;
			tracked.debug_info = debug_info;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_REALLOCATE;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_MEMORY_PROTECTED, &tracked);
		}

		return new_allocation;
	}

	bool MemoryProtectedAllocator::Belongs(const void* block) const
	{
		if (chunk_size > 0) {
			if (linear_allocators) {
				for (unsigned int index = 0; index < count; index++) {
					const LinearAllocator* current_allocator = (const LinearAllocator*)allocators[index];
					if (current_allocator->Belongs(block)) {
						return true;
					}
				}
			}
			else {
				for (unsigned int index = 0; index < count; index++) {
					const MultipoolAllocator* current_allocator = (const MultipoolAllocator*)allocators[index];
					if (current_allocator->Belongs(block)) {
						return true;
					}
				}
			}
		}
		else {
			return SearchBytes(allocation_pointers, count, (size_t)block, sizeof(block)) != -1;
		}

		return false;
	}

	void MemoryProtectedAllocator::Clear(DebugInfo debug_info)
	{
		if (chunk_size > 0) {
			for (unsigned int index = 1; index < count; index++) {
				FreeAllocatorBase(this, index);
			}

			if (linear_allocators) {
				LinearAllocator* allocator = (LinearAllocator*)allocators[0];
				allocator->Clear();
			}
			else {
				MultipoolAllocator* allocator = (MultipoolAllocator*)allocators[0];
				allocator->Clear();
			}

			count = 1;
		}
		else {
			for (unsigned int index = 0; index < count; index++) {
				void* original_pointer = GetOriginalPointerFromVirtualAllocation(allocation_pointers[index]);
				OS::VirtualDeallocation(original_pointer);
			}
			count = 0;
		}

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.debug_info = debug_info;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_CLEAR;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_MEMORY_PROTECTED, &tracked);
		}
	}

	void MemoryProtectedAllocator::Free(DebugInfo debug_info)
	{
		bool previous_debug_mode = m_debug_mode;
		m_debug_mode = false;
		Clear();
		m_debug_mode = previous_debug_mode;

		if (chunk_size > 0) {
			FreeAllocatorBase(this, 0);
			ECSEngine::Free(allocators);
		}
		else {
			ECSEngine::Free(allocation_pointers);
		}

		if (m_debug_mode) {
			TrackedAllocation tracked;
			tracked.debug_info = debug_info;
			tracked.function_type = ECS_DEBUG_ALLOCATOR_FREE;
			DebugAllocatorManagerAddEntry(this, ECS_ALLOCATOR_MEMORY_PROTECTED, &tracked);
		}

		memset(this, 0, sizeof(*this));
	}

	bool MemoryProtectedAllocator::IsEmpty() const
	{
		if (chunk_size > 0) {
			if (linear_allocators) {
				LinearAllocator* allocator = (LinearAllocator*)allocators[0];
				return count == 1 && allocator->IsEmpty();
			}
			else {
				MultipoolAllocator* allocator = (MultipoolAllocator*)allocators[0];
				return count == 1 && allocator->IsEmpty();
			}
		}
		else {
			return count == 0;
		}
	}

	bool MemoryProtectedAllocator::DisableWriteProtection() const
	{
		bool success = true;
		if (chunk_size > 0) {
			for (unsigned int index = 0; index < count; index++) {
				void* allocated_buffer = nullptr;
				if (linear_allocators) {
					const LinearAllocator* allocator = (const LinearAllocator*)allocators[index];
					allocated_buffer = allocator->GetAllocatedBuffer();
				}
				else {
					const MultipoolAllocator* allocator = (const MultipoolAllocator*)allocators[index];
					allocated_buffer = allocator->GetAllocatedBuffer();
				}

				success &= OS::DisableVirtualWriteProtection(allocated_buffer);
			}
		}
		else {
			for (unsigned int index = 0; index < count; index++) {
				success &= OS::DisableVirtualWriteProtection(GetOriginalPointerFromVirtualAllocation(allocation_pointers[index]));
			}
		}
		return success;
	}

	bool MemoryProtectedAllocator::DisableWriteProtection(void* block) const
	{
		ECS_ASSERT(chunk_size == 0, "MemoryProtectedAllocator: Cannot disable write protection for a particular block in chunked mode");

		// We cannot block directly to the function since we need to get the original pointer
		size_t search_index = SearchBytes(allocation_pointers, count, (size_t)block, sizeof(block));
		if (search_index != -1) {
			return OS::DisableVirtualWriteProtection(GetOriginalPointerFromVirtualAllocation(allocation_pointers[search_index]));
		}
		return false;
	}

	bool MemoryProtectedAllocator::EnableWriteProtection() const
	{
		bool success = true;
		if (chunk_size > 0) {
			for (unsigned int index = 0; index < count; index++) {
				void* allocated_buffer = nullptr;
				if (linear_allocators) {
					const LinearAllocator* allocator = (const LinearAllocator*)allocators[index];
					allocated_buffer = allocator->GetAllocatedBuffer();
				}
				else {
					const MultipoolAllocator* allocator = (const MultipoolAllocator*)allocators[index];
					allocated_buffer = allocator->GetAllocatedBuffer();
				}

				success &= OS::EnableVirtualWriteProtection(allocated_buffer);
			}
		}
		else {
			for (unsigned int index = 0; index < count; index++) {
				success &= OS::EnableVirtualWriteProtection(GetOriginalPointerFromVirtualAllocation(allocation_pointers[index]));
			}
		}
		return success;
	}

	bool MemoryProtectedAllocator::EnableWriteProtection(void* block) const
	{
		ECS_ASSERT(chunk_size == 0, "MemoryProtectedAllocator: Cannot enable write protection for a particular block in chunked mode");

		// We cannot block directly to the function since we need to get the original pointer
		size_t search_index = SearchBytes(allocation_pointers, count, (size_t)block, sizeof(block));
		if (search_index != -1) {
			return OS::EnableVirtualWriteProtection(GetOriginalPointerFromVirtualAllocation(allocation_pointers[search_index]));
		}
		return false;
	}

	// ----------------------------------------- Thread Safe functions -------------------------------------------------------

	void* MemoryProtectedAllocator::Allocate_ts(size_t size, size_t alignment, DebugInfo debug_info) {
		return ThreadSafeFunctorReturn(&m_lock, [&]() {
			return Allocate(size, alignment, debug_info);
		});
	}

	template<bool trigger_error_if_not_found>
	bool MemoryProtectedAllocator::Deallocate_ts(const void* block, DebugInfo debug_info) {
		return ThreadSafeFunctorReturn(&m_lock, [&]() {
			return Deallocate<trigger_error_if_not_found>(block, debug_info);
		});
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, MemoryProtectedAllocator::Deallocate_ts, const void*, DebugInfo);

	void* MemoryProtectedAllocator::Reallocate_ts(const void* block, size_t new_size, size_t alignment, DebugInfo debug_info)
	{
		return ThreadSafeFunctorReturn(&m_lock , [&]() {
			return Reallocate(block, new_size, alignment, debug_info);
		});
	}

}