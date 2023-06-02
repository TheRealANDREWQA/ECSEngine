#include "ecspch.h"
#include "BlockRange.h"
#include "../Utilities/Function.h"
#include "../Math/VCLExtensions.h"

namespace ECSEngine {

	BlockRange::BlockRange(void* buffer, unsigned int capacity, unsigned int max_index) : 
		m_buffer((unsigned int*)buffer), m_free_block_count(1), m_used_block_count(0), m_capacity(capacity)
	{
		// initalizing the first block to be the whole range
		if (buffer != nullptr) {
			SetStart(0, 0);
			SetEnd(0, max_index);
		}
	}

	void BlockRange::AddFreeBlock(unsigned int start, unsigned int end)
	{
		unsigned int first_used_start = GetStart(m_free_block_count);
		unsigned int first_used_end = GetEnd(m_free_block_count);
		SetStart(m_free_block_count, start);
		SetEnd(m_free_block_count, end);
		m_free_block_count++;
		SetStart(m_free_block_count + m_used_block_count - 1, first_used_start);
		SetEnd(m_free_block_count + m_used_block_count - 1, first_used_end);
	}

	void BlockRange::AddUsedBlock(unsigned int start, unsigned int end)
	{
		m_used_block_count++;
		SetStart(m_free_block_count + m_used_block_count - 1, start);
		SetEnd(m_free_block_count + m_used_block_count - 1, end);
	}

	void BlockRange::RemoveFreeBlock(unsigned int index)
	{
		if (index != m_free_block_count - 1) {
			unsigned int last_free_start = GetStart(m_free_block_count - 1);
			unsigned int last_free_end = GetEnd(m_free_block_count - 1);
			SetStart(index, last_free_start);
			SetEnd(index, last_free_end);
		}

		unsigned int last_used_start = GetStart(m_free_block_count + m_used_block_count - 1);
		unsigned int last_used_end = GetEnd(m_free_block_count + m_used_block_count - 1);
		m_free_block_count--;
		SetStart(m_free_block_count, last_used_start);
		SetEnd(m_free_block_count, last_used_end);
	}

	void BlockRange::RemoveUsedBlock(unsigned int index)
	{
		if (index != m_free_block_count + m_used_block_count - 1) {
			unsigned int last_used_start = GetStart(m_free_block_count + m_used_block_count - 1);
			unsigned int last_used_end = GetEnd(m_free_block_count + m_used_block_count - 1);
			m_used_block_count--;
			SetStart(index, last_used_start);
			SetEnd(index, last_used_end);
		}
	}

	template<bool assert_if_not_found>
	bool BlockRange::Free(unsigned int start) {
		size_t index = function::SearchBytes(m_buffer + m_free_block_count, m_used_block_count, start, sizeof(start));

		if constexpr (assert_if_not_found) {
			// checking if the start value is valid
			ECS_ASSERT(index != -1, "Attempting to free a block that was not allocated");
		}
		else {
			if (index == -1) {
				return false;
			}
		}

		index += m_free_block_count;

		if constexpr (assert_if_not_found) {
			ECS_ASSERT(index >= m_free_block_count && index < m_used_block_count + m_free_block_count, "Attempting to free a block that was not allocated");
		}
		else {
			if (index < m_free_block_count || index >= m_used_block_count + m_free_block_count) {
				return false;
			}
		}

		unsigned int end = GetEnd(index);
		SetStart(index, GetStart(m_free_block_count + m_used_block_count - 1));
		SetEnd(index, GetEnd(m_free_block_count + m_used_block_count - 1));
		m_used_block_count--;

		// Next block index for coalescing
		size_t next_block_index = function::SearchBytes(m_buffer, m_free_block_count, end, sizeof(end));

		// Previous block index for coalescing
		size_t previous_block_index = function::SearchBytes(m_buffer + m_capacity, m_free_block_count, start, sizeof(start));

		// if no forward or previous block was found, swap the current first used block and make this one be free
		if (next_block_index == -1 && previous_block_index == -1) {
			size_t first_used_block_start = GetStart(m_free_block_count);
			size_t first_used_block_end = GetEnd(m_free_block_count);
			SetStart(m_free_block_count, start);
			SetEnd(m_free_block_count, end);
			if (index != m_free_block_count) {
				m_free_block_count++;
				SetStart(m_free_block_count + m_used_block_count - 1, first_used_block_start);
				SetEnd(m_free_block_count + m_used_block_count - 1, first_used_block_end);
			}
			else {
				m_free_block_count++;
			}
		}
		// if both a previous and a forward block were found, place the previous block start at the next block
		// and replace the previous block with the last free block and the last free block with the last used block
		else if (next_block_index < m_free_block_count && previous_block_index < m_free_block_count) {
			SetStart(next_block_index, GetStart(previous_block_index));
			SetStart(previous_block_index, GetStart(m_free_block_count - 1));
			SetEnd(previous_block_index, GetEnd(m_free_block_count - 1));
			SetStart(m_free_block_count - 1, GetStart(m_free_block_count + m_used_block_count - 1));
			SetEnd(m_free_block_count - 1, GetEnd(m_free_block_count + m_used_block_count - 1));
			m_free_block_count--;
		}
		// Only previous block is valid
		else if (previous_block_index < m_free_block_count) {
			ECS_ASSERT(GetEnd(previous_block_index) < end);
			SetEnd(previous_block_index, end);
		}
		// Only next block is valid
		else if (next_block_index < m_free_block_count) {
			ECS_ASSERT(GetStart(next_block_index) > start);
			SetStart(next_block_index, start);
		}

		return true;
	}

	ECS_TEMPLATE_FUNCTION_BOOL(bool, BlockRange::Free, unsigned int);

	unsigned int BlockRange::Request(unsigned int size) {
		ECS_ASSERT(size > 0, "Block range: zero allocation not allowed");
		if (m_free_block_count + m_used_block_count < m_capacity) {
			Vec8ui section, temp, sizes = size;
			Vec8ib match;
			int flag = -1;
			size_t i = 0;
			for (; flag == -1 && i < m_free_block_count; i += temp.size()) {
				section.load((const void*)(m_buffer + i));
				temp.load((const void*)(m_buffer + i + m_capacity));
				match = (temp - section) >= sizes;
				flag = HorizontalFindFirst(match);
			}
			size_t index = flag + i - temp.size();
			if (flag != -1 && index < m_free_block_count) {	
				m_used_block_count++;
				size_t block_start = GetStart(index);
				size_t block_end = GetEnd(index);
				ECS_ASSERT(block_start < block_end);
				if (block_end - block_start == size) {
					m_free_block_count--;
					SetStart(index, GetStart(m_free_block_count));
					SetEnd(index, GetEnd(m_free_block_count));
					SetStart(m_free_block_count, block_start);
					SetEnd(m_free_block_count, block_end);
				}
				else {
					SetStart(index, block_start + size);
					SetStart(m_free_block_count + m_used_block_count - 1, block_start);
					SetEnd(m_free_block_count + m_used_block_count - 1, block_start + size);
				}
				return block_start;
			}
		}
		return 0xFFFFFFFF;
	}

	void BlockRange::RequestBiggestBlock(unsigned int& buffer_offset, unsigned int& size) {
		int64_t maximum_index = -1;
		size = 0;
		for (size_t index = 0; index < m_free_block_count; index++) {
			size_t block_size = GetEnd(index) - GetStart(index) + 1;
			if (block_size > size) {
				maximum_index = index;
				size = block_size;
			}
		}
		if (maximum_index != -1) {
			buffer_offset = GetStart(maximum_index);
			m_free_block_count--;
			m_used_block_count++;
			size_t block_start = GetStart(maximum_index);
			size_t block_end = GetEnd(maximum_index);
			SetStart(maximum_index, GetStart(m_free_block_count));
			SetEnd(maximum_index, GetEnd(m_free_block_count));
			SetStart(m_free_block_count, block_start);
			SetEnd(m_free_block_count, block_end);
		}
	}

	unsigned int BlockRange::ReallocateBlock(unsigned int start, unsigned int new_size)
	{
		size_t index = function::SearchBytes(m_buffer + m_free_block_count, m_used_block_count, start, sizeof(start));
		// The block should exist
		ECS_ASSERT(index != -1);

		index += m_free_block_count;

		unsigned int end = GetEnd(index);
		unsigned int block_size = end - start;
		size_t next_block_index = function::SearchBytes(m_buffer, m_free_block_count, end, sizeof(unsigned int));
		
		unsigned int request = -1;
		if (block_size > new_size) {
			// Shrink the block
			// Check to see if there is a next block that the newly freed space can be coalesced with
			// Else create a new free block entry
			if (next_block_index == -1) {
				if (m_used_block_count + m_free_block_count < m_capacity) {
					// Change the end of the current block and add a new free block
					SetEnd(index, start + new_size);
					AddFreeBlock(start + new_size, start + new_size + block_size - new_size);
					request = start;
				}
			}
			else {
				// Can be coalesced
				SetEnd(index, start + new_size);
				SetStart(next_block_index, start + new_size);
				request = start;
			}
		}
		else {
			// Grow the block
			// Check to see if there is empty space for that block already
			if (next_block_index == -1) {
				request = Request(new_size);
				if (request != -1) {
					// Deallocate this block
					Free(start);
				}
			}
			else {
				// Check to see if it has enough space
				unsigned int next_block_end = GetEnd(next_block_index);
				unsigned int next_block_difference = next_block_end - end;
				unsigned int difference = new_size - block_size;
				if (next_block_difference >= difference) {
					// Can grow the allocation
					SetStart(next_block_index, end + difference);
					SetEnd(index, end + difference);
					request = start;
				}
				else {
					// We need to request a new block and then deallocate this one
					request = Request(new_size);
					if (request != -1) {
						// Deallocate this block
						Free(start);
					}
				}
			}
		}

		return request;
	}

	unsigned int BlockRange::ReallocateBlockAndCopy(unsigned int start, unsigned int new_size, void* storage_buffer, unsigned int copy_size)
	{
		if (copy_size == 0) {
			size_t index = function::SearchBytes(m_buffer + m_free_block_count, m_used_block_count, start, sizeof(start));
			// The block should exist
			ECS_ASSERT(index != -1);

			index += m_free_block_count;

			unsigned int end = GetEnd(index);
			unsigned int block_size = end - start;
			copy_size = block_size;
		}

		unsigned int new_position = ReallocateBlock(start, new_size);
		if (new_position != start && new_position != -1) {
			memcpy(function::OffsetPointer(storage_buffer, new_position), function::OffsetPointer(storage_buffer, start), copy_size);
		}
		return new_position;
	}

	void BlockRange::Clear() {
		// Need to go through all blocks and calculate their maximum bound and keep it
		unsigned int maximum_size = 0;
		for (unsigned int index = 0; index < m_free_block_count + m_used_block_count; index++) {
			unsigned int end = GetEnd(index);
			maximum_size = std::max(end, maximum_size);
		}

		m_free_block_count = 1;
		m_used_block_count = 0;
		SetStart(0, 0);

		// Capacity is the block capacity, not the max_size of the block range
		SetEnd(0, maximum_size);
	}

	ECS_INLINE unsigned int BlockRange::GetStart(unsigned int index) const {
		ECS_ASSERT(index < m_free_block_count + m_used_block_count && index >= 0);
		return m_buffer[index];
	}

	ECS_INLINE unsigned int BlockRange::GetEnd(unsigned int index) const {
		ECS_ASSERT(index < m_free_block_count + m_used_block_count && index >= 0);
		return m_buffer[index + m_capacity];
	}

	ECS_INLINE void BlockRange::SetStart(unsigned int index, unsigned int value) {
		ECS_ASSERT(index < m_free_block_count + m_used_block_count && index >= 0);
		m_buffer[index] = value;
	}

	ECS_INLINE void BlockRange::SetEnd(unsigned int index, unsigned int value) {
		ECS_ASSERT(index < m_free_block_count + m_used_block_count && index >= 0);
		m_buffer[index + m_capacity] = value;
	}

	ECS_INLINE const void* BlockRange::GetAllocatedBuffer() const {
		return m_buffer;
	}

	ECS_INLINE unsigned int BlockRange::GetFreeBlockCount() const {
		return m_free_block_count;
	}

	ECS_INLINE unsigned int BlockRange::GetUsedBlockCount() const {
		return m_used_block_count;
	}

	ECS_INLINE unsigned int BlockRange::GetCapacity() const {
		return m_capacity;
	}

	ECS_INLINE size_t BlockRange::MemoryOf(unsigned int number) {
		return (sizeof(unsigned int) * number) * 2;
	}

}
