#include "ecspch.h"
#include "RingBuffer.h"
#include "ConcurrentPrimitives.h"
#include "../../Utilities/Function.h"

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
		if (size + allocation_size > capacity) {
			size = 0;
		}

		// The only wait condition is that the size is less than last_in_use
		// and the size + allocation_size is greater than last_in_use
		size_t current_last_in_use = last_in_use.load(ECS_RELAXED);
		while (size < current_last_in_use && size + allocation_size > current_last_in_use) {
			// We need to wait
			BOOL success = WaitOnAddress(&last_in_use, &current_last_in_use, sizeof(last_in_use), INFINITE);
			current_last_in_use = last_in_use.load(ECS_RELAXED);
		}

		void* allocation = function::OffsetPointer(buffer, size);
		size += allocation_size;

		return allocation;
	}

	// ------------------------------------------------------------------------------------

	void RingBuffer::Finish(const void* allocation_buffer, size_t allocation_size)
	{
		size_t offset = function::PointerDifference(allocation_buffer, buffer);
		ECS_ASSERT(offset < capacity);

		last_in_use.store(offset + allocation_size, ECS_RELAXED);
		WakeByAddressSingle(&last_in_use);
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