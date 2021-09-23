#include "ecspch.h"
#include "BlockRange.h"
#include "../Utilities/Function.h"

namespace ECSEngine {

	namespace containers {


		BlockRange::BlockRange(void* buffer, unsigned int capacity, unsigned int max_index) : 
			m_buffer((unsigned int*)buffer), m_free_block_count(1), m_used_block_count(0), m_capacity(capacity)
		{
			// initalizing the first block to be the whole range
			if (buffer != nullptr) {
				SetStart(0, 0);
				SetEnd(0, max_index - 1);
			}
#ifdef ECSENGINE_DEBUG
			m_debug_starts = nullptr;
#endif
		}

		void BlockRange::AddDebugStart(unsigned int start)
		{
#ifdef ECSENGINE_DEBUG
			if (m_debug_starts != nullptr) {
				m_debug_starts[m_used_block_count - 1] = start;
			}
#endif
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

			unsigned int difference = 0xFFFFFFFF;
			unsigned int closest_index = 0xFFFFFFFF;
			bool is_debug_true = IsDebugStart(start, difference, closest_index);
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
				flag = horizontal_find_first(match);
			}

			size_t next_block_index = i + flag - temp.size();

			// linear SIMD search to find if there is a previous freed block 
			temp = start - 1;
			int flag2 = -1;
			for (i = 0; flag2 == -1 && i < m_free_block_count; i += temp.size()) {
				section.load((const void*)(m_buffer + m_capacity + i));
				match = section == temp;
				flag2 = horizontal_find_first(match);
			}

			// previous block 
			size_t previous_block_index = i + flag2 - temp.size();

			// if no forward or previous block was found, swap the current first used block and make this one be free
			unsigned int pogu = 0;
			size_t pogu1 = 0;
			size_t pogu2 = 0;
			size_t pogu3 = 0;
			size_t pogu4 = 0;
			size_t pogu5 = 0;
			size_t pogu6 = 0;
			if ((flag == -1 || next_block_index >= m_free_block_count) && (flag2 == -1 || previous_block_index >= m_free_block_count)) {
				size_t first_used_block_start = GetStart(m_free_block_count);
				size_t first_used_block_end = GetEnd(m_free_block_count);
				pogu1 = first_used_block_start;
				pogu2 = first_used_block_end;
				SetStart(m_free_block_count, start);
				SetEnd(m_free_block_count, end);
				if (index != m_free_block_count) {
					m_free_block_count++;
					pogu3 = GetStart(index);
					pogu4 = GetEnd(index);
					SetStart(m_free_block_count + m_used_block_count - 1, first_used_block_start);
					SetEnd(m_free_block_count + m_used_block_count - 1, first_used_block_end);
				}
				else {
					m_free_block_count++;
				}
				pogu = 0;
			}
			// if both a previous and a forward block were found, place the previous block start at the next block
			// and replace the previous block with the last free block and the last free block with the last used block
			else if (flag != -1 && flag2 != -1 && next_block_index < m_free_block_count && previous_block_index < m_free_block_count) {
				pogu1 = GetStart(previous_block_index);
				pogu2 = GetStart(m_free_block_count - 1);
				pogu3 = GetEnd(m_free_block_count - 1);
				pogu4 = GetStart(m_free_block_count + m_used_block_count - 1);
				pogu5 = GetEnd(m_free_block_count + m_used_block_count - 1);
				SetStart(next_block_index, GetStart(previous_block_index));
				SetStart(previous_block_index, GetStart(m_free_block_count - 1));
				SetEnd(previous_block_index, GetEnd(m_free_block_count - 1));
				SetStart(m_free_block_count - 1, GetStart(m_free_block_count + m_used_block_count - 1));
				SetEnd(m_free_block_count - 1, GetEnd(m_free_block_count + m_used_block_count - 1));
				m_free_block_count--;
				pogu = 1;
			}
			else if (flag2 != -1 && previous_block_index < m_free_block_count) {
				pogu1 = GetEnd(previous_block_index);
				ECS_ASSERT(GetEnd(previous_block_index) < end);
				SetEnd(previous_block_index, end);
				pogu = 2;
			}
			else if (next_block_index < m_free_block_count) {
				pogu1 = GetStart(next_block_index);
				ECS_ASSERT(GetStart(next_block_index) > start);
				SetStart(next_block_index, start);
				pogu = 3;
			}
			RemoveDebugStart(start);
			unsigned int debug_index = DebugCheckUsedBlocks();
			ECS_ASSERT(0xFFFFFFFF == debug_index);
		}

		ECS_TEMPLATE_FUNCTION_BOOL(void, BlockRange::Free, unsigned int);

		bool BlockRange::IsDebugStart(unsigned int start, unsigned int& difference, unsigned int& closest_index) const
		{
#ifdef ECSENGINE_DEBUG
			if (m_debug_starts != nullptr) {
				for (size_t index = 0; index < m_used_block_count; index++) {
					if (m_debug_starts[index] > start) {
						unsigned int diff = m_debug_starts[index] - start;
						if (difference > diff) {
							difference = diff;
							closest_index = index;
						}
					}
					else if (m_debug_starts[index] < start) {
						unsigned int diff = m_debug_starts[index] - start;
						if (difference > diff) {
							difference = diff;
							closest_index = index;
						}
					}
					else if (m_debug_starts[index] == start)
						return true;
				}
			}
			return false;
#else
			return true;
#endif
		}

		void BlockRange::RemoveDebugStart(unsigned int start)
		{
#ifdef ECSENGINE_DEBUG
			if (m_debug_starts != nullptr) {
				bool ok = false;
				for (size_t index = 0; index < m_used_block_count + 1; index++) {
					if (m_debug_starts[index] == start) {
						m_debug_starts[index] = m_debug_starts[m_used_block_count];
						ok = true;
						break;
					}
				}
				ECS_ASSERT(ok);
			}
#endif
		}

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
					AddDebugStart(block_start);
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
				AddDebugStart(block_start);
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

		unsigned int BlockRange::DebugCheckUsedBlocks() const
		{
#ifdef ECSENGINE_DEBUG
			if (m_debug_starts != nullptr) {
				for (size_t subindex = 0; subindex < m_used_block_count; subindex++) {
					bool ok = false;
					for (size_t index = m_free_block_count; index < m_used_block_count + m_free_block_count; index++) {
						if (GetStart(index) == m_debug_starts[subindex]) {
							ok = true;
							break;
						}
					}
					if (ok == false) {
						return subindex;
					}
				}
			}
			return 0xFFFFFFFF;
#else
			return 0xFFFFFFFF;
#endif
		}

		unsigned int BlockRange::GetStart(unsigned int index) const {
			ECS_ASSERT(index < m_free_block_count + m_used_block_count && index >= 0);
			return m_buffer[index];
		}

		unsigned int BlockRange::GetEnd(unsigned int index) const {
			ECS_ASSERT(index < m_free_block_count + m_used_block_count && index >= 0);
			return m_buffer[index + m_capacity];
		}

		void BlockRange::SetStart(unsigned int index, unsigned int value) {
			ECS_ASSERT(index < m_free_block_count + m_used_block_count && index >= 0);
			m_buffer[index] = value;
		}

		void BlockRange::SetEnd(unsigned int index, unsigned int value) {
			ECS_ASSERT(index < m_free_block_count + m_used_block_count && index >= 0);
			m_buffer[index + m_capacity] = value;
		}

		void BlockRange::SetDebugBuffer(unsigned int* buffer)
		{
#ifdef ECSENGINE_DEBUG
			m_debug_starts = buffer;
#endif
		}

		const void* BlockRange::GetAllocatedBuffer() const {
			return m_buffer;
		}

		unsigned int BlockRange::GetFreeBlockCount() const {
			return m_free_block_count;
		}

		unsigned int BlockRange::GetUsedBlockCount() const {
			return m_used_block_count;
		}

		unsigned int BlockRange::GetCapacity() const {
			return m_capacity;
		}

		size_t BlockRange::MemoryOf(unsigned int number) {
			return (sizeof(unsigned int) * number) * 2;
		}

		size_t BlockRange::MemoryOfDebug(unsigned int number)
		{
			return sizeof(unsigned int) * number;
		}
	}

}
