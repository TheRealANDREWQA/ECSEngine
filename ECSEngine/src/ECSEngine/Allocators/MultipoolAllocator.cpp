#include "ecspch.h"
#include "../Utilities/Function.h"
#include "MultipoolAllocator.h"

namespace ECSEngine {
	
	MultipoolAllocator::MultipoolAllocator() : m_buffer(nullptr), m_size(0), m_range(nullptr, 0, 0) {}
	
	MultipoolAllocator::MultipoolAllocator(unsigned char* buffer, size_t size, size_t pool_count)
		: m_buffer((unsigned char*)buffer), m_spin_lock(), m_size(size), m_range((unsigned int*)function::OffsetPointer(buffer, size), pool_count, size) {}

	MultipoolAllocator::MultipoolAllocator(unsigned char* buffer, void* block_range_buffer, size_t size, size_t pool_count) : m_buffer(buffer), m_spin_lock(), m_size(size),
		m_range((unsigned int*)block_range_buffer, pool_count, size) {}
	
	void* MultipoolAllocator::Allocate(size_t size, size_t alignment) {
		ECS_ASSERT(alignment <= ECS_CACHE_LINE_SIZE);
		// To make sure that the allocation is aligned we are requesting a surplus of alignment from the block range.
		// Then we are aligning the pointer like the stack one and placing the offset byte before the allocation
		unsigned int index = m_range.Request(size + alignment);

		if (index == 0xFFFFFFFF)
			return nullptr;

		uintptr_t allocation = function::AlignPointerStack((uintptr_t)m_buffer + index, alignment);
		size_t offset = allocation - (uintptr_t)m_buffer;
		ECS_ASSERT(offset - index - 1 < alignment);
		m_buffer[offset - 1] = offset - index - 1;

		return (void*)allocation;
	}

	template<bool trigger_error_if_not_found>
	bool MultipoolAllocator::Deallocate(const void* block) {
		// Calculating the start index of the block by reading the byte metadata 
		uintptr_t byte_offset_position = (uintptr_t)block - (uintptr_t)m_buffer - 1;
		if constexpr (trigger_error_if_not_found) {
			ECS_ASSERT(m_buffer[byte_offset_position] < ECS_CACHE_LINE_SIZE);
		}
		else {
			if (m_buffer[byte_offset_position] >= ECS_CACHE_LINE_SIZE) {
				// Exit if the alignment is very high
				return false;
			}
		}
		return m_range.Free<trigger_error_if_not_found>((unsigned int)(byte_offset_position - m_buffer[byte_offset_position]));
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, MultipoolAllocator::Deallocate, const void*);

	template<typename Functor>
	void* ReallocateImpl(MultipoolAllocator* allocator, const void* block, size_t new_size, size_t alignment, Functor&& functor) {
		uintptr_t byte_offset_position = (uintptr_t)block - (uintptr_t)allocator->m_buffer - 1;
		ECS_ASSERT(allocator->m_buffer[byte_offset_position] < ECS_CACHE_LINE_SIZE);
		unsigned int previous_start = (unsigned int)(byte_offset_position - allocator->m_buffer[byte_offset_position]);
		unsigned int new_start = functor(previous_start);
		if (new_start != -1) {
			uintptr_t allocation = function::AlignPointerStack((uintptr_t)allocator->m_buffer + new_start, alignment);
			if (new_start != previous_start) {
				unsigned int offset = allocation - (uintptr_t)allocator->m_buffer;
				ECS_ASSERT(offset - new_start - 1 < alignment);
				allocator->m_buffer[offset - 1] = offset - new_start - 1;
			}
			return (void*)allocation;
		}
		else {
			// The request cannot be fulfilled
			return nullptr;
		}
	}

	void* MultipoolAllocator::Reallocate(const void* block, size_t new_size, size_t alignment)
	{
		return ReallocateImpl(this, block, new_size, alignment, [&](unsigned int previous_start) {
			return m_range.ReallocateBlock(previous_start, new_size + alignment);
		});
	}

	void* MultipoolAllocator::ReallocateAndCopy(const void* block, size_t new_size, size_t alignment)
	{
		return ReallocateImpl(this, block, new_size, alignment, [&](unsigned int previous_start) {
			return m_range.ReallocateBlockAndCopy(previous_start, new_size + alignment, m_buffer);
		});
	}

	void MultipoolAllocator::Clear() {
		m_range.Clear();
	}

	bool MultipoolAllocator::IsEmpty() const
	{
		return m_range.GetUsedBlockCount() == 0;
	}

	void* MultipoolAllocator::GetAllocatedBuffer() const
	{
		return m_buffer;
	}

	size_t MultipoolAllocator::GetSize() const
	{
		return m_size;
	}

	bool MultipoolAllocator::Belongs(const void* buffer) const
	{
		uintptr_t ptr = (uintptr_t)buffer;
		return ptr >= (uintptr_t)m_buffer && ptr < (uintptr_t)m_buffer + m_size;
	}

	size_t MultipoolAllocator::MemoryOf(unsigned int pool_count, unsigned int size) {
		return BlockRange::MemoryOf(pool_count) + size + 8;
	}

	size_t MultipoolAllocator::MemoryOf(unsigned int pool_count) {
		return BlockRange::MemoryOf(pool_count);
	}

	// ---------------------- Thread safe variants -----------------------------

	template<typename Functor>
	auto TsFunction(MultipoolAllocator* allocator, Functor&& functor) {
		allocator->m_spin_lock.lock();
		auto return_value = functor();
		allocator->m_spin_lock.unlock();
		return return_value;
	}

	void* MultipoolAllocator::Allocate_ts(size_t size, size_t alignment) {
		return TsFunction(this, [&]() {
			return Allocate(size, alignment);
		});
	}

	template<bool trigger_error_if_not_found>
	bool MultipoolAllocator::Deallocate_ts(const void* block) {
		return TsFunction(this, [&]() {
			return Deallocate<trigger_error_if_not_found>(block);
		});
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, MultipoolAllocator::Deallocate_ts, const void*);

	void* MultipoolAllocator::Reallocate_ts(const void* block, size_t new_size, size_t alignment)
	{
		return TsFunction(this, [&]() {
			return Reallocate(block, new_size, alignment);
		});
	}

	void* MultipoolAllocator::ReallocateAndCopy_ts(const void* block, size_t new_size, size_t alignment)
	{
		return TsFunction(this, [&]() {
			return ReallocateAndCopy_ts(block, new_size, alignment);
		});
	}
}
