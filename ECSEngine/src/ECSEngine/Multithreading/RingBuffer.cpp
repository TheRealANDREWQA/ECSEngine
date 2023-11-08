#include "ecspch.h"
#include "RingBuffer.h"
#include "ConcurrentPrimitives.h"
#include "../Utilities/PointerUtilities.h"

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
	}

	// ------------------------------------------------------------------------------------

	void RingBuffer::InitializeFromBuffer(uintptr_t& ptr, size_t _capacity)
	{
		buffer = (void*)ptr;
		size = 0;
		last_in_use.store(0, ECS_RELAXED);
		capacity = _capacity;

		ptr += _capacity;
	}

	// ------------------------------------------------------------------------------------

}