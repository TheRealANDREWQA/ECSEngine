#include "ecspch.h"
#include "TaskAllocator.h"
#include "../../Utilities/Function.h"
#include "ConcurrentPrimitives.h"

namespace ECSEngine {

	TaskAllocator& TaskAllocator::operator=(const TaskAllocator& other)
	{
		top = other.top.load(ECS_ACQUIRE);
		marker = other.marker;
		buffer = other.buffer;
		capacity = other.capacity;
		return *this;
	}

	void TaskAllocator::Clear()
	{
		top = 0;
	}

	void TaskAllocator::SetMarker()
	{
		marker = top;
	}

	void TaskAllocator::ReturnToMarker()
	{
		top = marker;
	}

	void* TaskAllocator::Allocate(size_t size, size_t alignment)
	{
		size_t previous_top = top.fetch_add(size + alignment);
		uintptr_t ptr = (uintptr_t)buffer;
		ptr += previous_top;
		ptr = function::AlignPointer(ptr, alignment);
		return (void*)ptr;
	}

	void TaskAllocator::Deallocate(const void* buffer) {}

}