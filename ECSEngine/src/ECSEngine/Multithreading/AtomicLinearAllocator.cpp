#include "ecspch.h"
#include "AtomicLinearAllocator.h"
#include "../Utilities/PointerUtilities.h"

namespace ECSEngine {

	AtomicLinearAllocator& AtomicLinearAllocator::operator=(const AtomicLinearAllocator& other)
	{
		top.store(other.top.load(std::memory_order_relaxed), std::memory_order_relaxed);
		marker = other.marker;
		buffer = other.buffer;
		capacity = other.capacity;
		return *this;
	}

	void AtomicLinearAllocator::Clear()
	{
		top = 0;
	}

	void AtomicLinearAllocator::SetMarker()
	{
		marker = top;
	}

	void AtomicLinearAllocator::ReturnToMarker()
	{
		top = marker;
	}

	void* AtomicLinearAllocator::Allocate(size_t size, size_t alignment)
	{
		size_t previous_top = top.fetch_add(size + alignment, std::memory_order_relaxed);
		uintptr_t ptr = (uintptr_t)buffer;
		ptr += previous_top;
		ptr = AlignPointer(ptr, alignment);
		return (void*)ptr;
	}

	void AtomicLinearAllocator::Deallocate(const void* buffer) {}

}