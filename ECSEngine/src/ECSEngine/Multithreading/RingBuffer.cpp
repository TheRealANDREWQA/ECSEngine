#include "ecspch.h"
#include "RingBuffer.h"
#include "ConcurrentPrimitives.h"
#include "../Utilities/PointerUtilities.h"
#include "../Profiling/AllocatorProfilingGlobal.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------

	RingBuffer::RingBuffer(const RingBuffer& other)
	{
		memcpy(this, &other, sizeof(other));
	}

	// ------------------------------------------------------------------------------------

	RingBuffer& RingBuffer::operator=(const RingBuffer& other)
	{
		memcpy(this, &other, sizeof(other));
		return *this;
	}

	// ------------------------------------------------------------------------------------

	void* RingBuffer::Allocate(size_t allocation_size)
	{
		// TODO: Investigate if there is a way of removing these locks here since
		// They have a bit of overhead
		lock.Lock();

		if (size + allocation_size > capacity) {
			size = 0;
		}

		lock.Unlock();

		// The only wait condition is that the size is less than last_in_use
		// and the size + allocation_size is greater than last_in_use
		size_t current_last_in_use = last_in_use.load(ECS_RELAXED);
		while (size < current_last_in_use && size + allocation_size > current_last_in_use) {
			// At the moment assert that this never happens
			ECS_ASSERT(false, "Task Manager Dynamic Task Allocator size too small");

			// We need to wait
			//BOOL success = WaitOnAddress(&last_in_use, &current_last_in_use, sizeof(last_in_use), INFINITE);
			//current_last_in_use = last_in_use.load(ECS_RELAXED);
		}

		lock.Lock();
		// Recheck again if the size + the allocation size exceed the capacity
		if (size + allocation_size > capacity) {
			size = 0;
		}

		void* allocation = OffsetPointer(buffer, size);
		size += allocation_size;
		lock.Unlock();

		if (profiling_mode) {
			// We need to calculate the current usage
			size_t usage = 0;
			if (size < last_in_use) {
				usage = capacity - current_last_in_use + size;
			}
			else {
				usage = size - current_last_in_use;
			}
			AllocatorProfilingAddAllocation(this, usage);
		}

		return allocation;
	}

	// ------------------------------------------------------------------------------------

	void* RingBuffer::Allocate(size_t allocation_size, size_t alignment)
	{
		void* allocation = Allocate(allocation_size + alignment - 1);
		return (void*)AlignPointer((uintptr_t)allocation, alignment);
	}

	// ------------------------------------------------------------------------------------

	void RingBuffer::Finish(const void* allocation_buffer, size_t allocation_size)
	{
		size_t offset = PointerDifference(allocation_buffer, buffer);
		ECS_ASSERT(offset < capacity);

		last_in_use.store(offset + allocation_size, ECS_RELAXED);
		// At the moment we don't use waiting
		//WakeByAddressAll(&last_in_use);
		
		// Don't measure this for the profiling
	}

	// ------------------------------------------------------------------------------------

	void RingBuffer::InitializeFromBuffer(uintptr_t& ptr, size_t _capacity)
	{
		buffer = (void*)ptr;
		size = 0;
		last_in_use.store(0, ECS_RELAXED);
		capacity = _capacity;
		profiling_mode = false;

		ptr += _capacity;
	}

	// ------------------------------------------------------------------------------------

	void RingBuffer::ExitProfilingMode()
	{
		profiling_mode = false;
	}

	size_t AllocatorCustomUsageFunction(const void* allocator) {
		// At the end of the frame, this will always 0
		return 0;
	}

	void AllocatorCustomExitFunction(void* allocator) {
		RingBuffer* ring_buffer = (RingBuffer*)allocator;
		ring_buffer->ExitProfilingMode();
	}

	void RingBuffer::SetProfilingMode(const char* name)
	{
		profiling_mode = true;
		AllocatorProfilingAddEntry(this, ECS_ALLOCATOR_TYPE_COUNT, name, AllocatorCustomUsageFunction, AllocatorCustomExitFunction);
	}

	// ------------------------------------------------------------------------------------

}