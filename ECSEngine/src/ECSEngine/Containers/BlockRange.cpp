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
			SetEnd(0, max_index - 1);
		}
	}


	template<bool assert_if_not_found>
	void BlockRange::Free(unsigned int start) {
		Vec8ui section, temp = start;
		Vec8ib match;
		int flag = -1;
		size_t i;
		// linear search using SIMD to find the start position in the used blocks
		for (i = m_free_block_count; flag == -1 && i < m_used_block_count + m_free_block_count; i += temp.size()) {
			section.load((const void*)(m_buffer + i));
			match = section == temp;
			flag = horizontal_find_first(match);
		}

		if constexpr (assert_if_not_found) {
			// checking if the start value is valid
			ECS_ASSERT(flag != -1, "Attempting to free a block that was not allocated");
		}

		size_t index = i + flag - temp.size();

		if constexpr (assert_if_not_found) {
			ECS_ASSERT(index >= m_free_block_count && index < m_used_block_count + m_free_block_count, "Attempting to free a block that was not allocated");
		}

		unsigned int end = GetEnd(index);
		SetStart(index, GetStart(m_free_block_count + m_used_block_count - 1));
		SetEnd(index, GetEnd(m_free_block_count + m_used_block_count - 1));
		unsigned int lul_start = GetStart(index);
		unsigned int lul_end = GetEnd(index);
		m_used_block_count--;

		// linear SIMD search to find if there is a forward freed block
		temp = end + 1;
		flag = -1;
		for (i = 0; flag == -1 && i < m_free_block_count; i += temp.size()) {
			section.load((const void*)(m_buffer + i));
			match = section == temp;
			PrecullHorizontalFindFirst(match, flag);
		}

		size_t next_block_index = i + flag - temp.size();

		// linear SIMD search to find if there is a previous freed block 
		temp = start - 1;
		int flag2 = -1;
		for (i = 0; flag2 == -1 && i < m_free_block_count; i += temp.size()) {
			section.load((const void*)(m_buffer + m_capacity + i));
			match = section == temp;
			PrecullHorizontalFindFirst(match, flag2);
		}

		// previous block 
		size_t previous_block_index = i + flag2 - temp.size();

		// if no forward or previous block was found, swap the current first used block and make this one be free
		if ((flag == -1 || next_block_index >= m_free_block_count) && (flag2 == -1 || previous_block_index >= m_free_block_count)) {
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
		else if (flag != -1 && flag2 != -1 && next_block_index < m_free_block_count && previous_block_index < m_free_block_count) {
			SetStart(next_block_index, GetStart(previous_block_index));
			SetStart(previous_block_index, GetStart(m_free_block_count - 1));
			SetEnd(previous_block_index, GetEnd(m_free_block_count - 1));
			SetStart(m_free_block_count - 1, GetStart(m_free_block_count + m_used_block_count - 1));
			SetEnd(m_free_block_count - 1, GetEnd(m_free_block_count + m_used_block_count - 1));
			m_free_block_count--;
		}
		else if (flag2 != -1 && previous_block_index < m_free_block_count) {
			ECS_ASSERT(GetEnd(previous_block_index) < end);
			SetEnd(previous_block_index, end);
		}
		else if (next_block_index < m_free_block_count) {
			ECS_ASSERT(GetStart(next_block_index) > start);
			SetStart(next_block_index, start);
		}
	}

	ECS_TEMPLATE_FUNCTION_BOOL(void, BlockRange::Free, unsigned int);


	unsigned int BlockRange::Request(unsigned int size) {
		ECS_ASSERT(size > 0, "Block range: zero allocation not allowed");
		if (m_free_block_count + m_used_block_count < m_capacity) {
			Vec8ui section, temp, sizes = size, one(1);
			Vec8ib match;
			int flag = -1;
			size_t i = 0;
			for (; flag == -1 && i < m_free_block_count; i += temp.size()) {
				section.load((const void*)(m_buffer + i));
				temp.load((const void*)(m_buffer + i + m_capacity));
				match = (temp - section + one) >= sizes;
				flag = horizontal_find_first(match);
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
					SetEnd(m_free_block_count + m_used_block_count - 1, block_start + size - 1);
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

	void BlockRange::Clear() {
		m_free_block_count = 1;
		m_used_block_count = 0;
		SetStart(0, 0);
		SetEnd(0, m_capacity - 1);
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
