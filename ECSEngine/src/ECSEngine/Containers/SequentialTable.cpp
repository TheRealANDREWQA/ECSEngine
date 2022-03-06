#include "ecspch.h"
#include "../Utilities/Function.h"
#include "../Utilities/FunctionTemplates.h"
#include "SequentialTable.h"

namespace ECSEngine {

	SequentialTable::SequentialTable() : m_sequences(nullptr), m_ranges(nullptr, 0, 0), m_keys(nullptr, 0), m_max_sequences(0), m_sequence_size(0) {}

	SequentialTable::SequentialTable(
		void* block_range_buffer,
		void* sequence_allocation, 
		void* key_allocation, 
		unsigned int capacity, 
		unsigned int max_sequences
	) : m_sequences((Sequence*)sequence_allocation),
		m_keys(key_allocation, capacity), m_max_sequences(max_sequences),
		m_sequence_size(0), m_ranges((unsigned int*)block_range_buffer, max_sequences, capacity) {}

	SequentialTable::SequentialTable(void* allocation, unsigned int capacity, unsigned int max_sequences) 
		: m_ranges((unsigned int*)allocation, max_sequences, capacity), m_sequence_size(0), m_max_sequences(max_sequences)
	{
		void* key_pointer = (void*)((uintptr_t)allocation + m_ranges.MemoryOf(max_sequences));
		m_keys = Stream<unsigned int>((void*)function::align_pointer((uintptr_t)key_pointer, alignof(unsigned int)), capacity);
		m_sequences = (Sequence*)function::align_pointer((uintptr_t)(m_keys.buffer) + capacity * sizeof(unsigned int), alignof(Sequence));
	}

	void SequentialTable::Clear() {
		m_ranges.Clear();
		m_sequence_size = 0;
	}

	unsigned int SequentialTable::CoalesceSequences(
		Substream* sorted_sequences, 
		unsigned int new_first, 
		unsigned int size, 
		Stream<Substream>& copy_order
	) {
		for (size_t index = 0; index < m_sequence_size; index++) {
			m_ranges.Free(m_sequences[index].buffer_start);
			sorted_sequences[index].offset = m_sequences[index].buffer_start;
			sorted_sequences[index].size = m_sequences[index].size;
		}

		function::insertion_sort(sorted_sequences, m_sequence_size, 1);

		// if the size is not appropiate
		size_t buffer_offset = m_ranges.Request(size);
		ECS_ASSERT(buffer_offset != 0xFFFFFFFFFFFFFFFF, "Attempting to coalesce sequences in static sequential table with a bigger size than possible");

		// iterating until the first sequences that overlapps with the new allocated sequence
		size_t index = 0;
		while (sorted_sequences[index].offset < buffer_offset) {
			index++;
		}
		// keeping the index for later
		int64_t smaller_than_index = index;
		// copying first the sequences that overlapp starting with the lowest buffer_start until the biggest
		while (sorted_sequences[index].offset >= buffer_offset && sorted_sequences[index].offset < buffer_offset + size - 1 && index < m_sequence_size) {
			copy_order[copy_order.size++] = sorted_sequences[index++];
		}

		smaller_than_index--;
		// assigning the remaining sequences that do not overlapp; first the one that came before the new offset
		for (; smaller_than_index >= 0; smaller_than_index--) {
			copy_order[copy_order.size++] = sorted_sequences[smaller_than_index];
		}
		// the remaining sequences that come after the new offset
		for (; index < m_sequence_size; index++) {
			copy_order[copy_order.size++] = sorted_sequences[index];
		}
		// replacing the keys
		for (size_t index = 0; index < size; index++) {
			m_keys[index + buffer_offset] = new_first + index;
		}
		return buffer_offset;
	}

	unsigned int SequentialTable::CoalesceSequences(
		Substream* sorted_sequences,
		unsigned int new_first,
		unsigned int size,
		Stream<Substream>& copy_order,
		unsigned int count
	) {
		for (size_t index = 0; index < count; index++) {
			m_ranges.Free(m_sequences[index].buffer_start);
			sorted_sequences[index].offset = m_sequences[index].buffer_start;
			sorted_sequences[index].size = m_sequences[index].size;
		}

		function::insertion_sort(sorted_sequences, count, 1);

		// if the size is not appropiate
		size_t buffer_offset = m_ranges.Request(size);
		ECS_ASSERT(buffer_offset != 0xFFFFFFFFFFFFFFFF, "Attempting to coalesce sequences in static sequential table with a bigger size than possible");

		// iterating until the first sequences that overlapps with the new allocated sequence
		size_t index = 0;
		while (sorted_sequences[index].offset < buffer_offset) {
			index++;
		}
		// keeping the index for later
		int64_t smaller_than_index = index;
		// copying first the sequences that overlapp starting with the lowest buffer_start until the biggest
		while (sorted_sequences[index].offset >= buffer_offset && sorted_sequences[index].offset < buffer_offset + size - 1 && index < count) {
			copy_order[copy_order.size++] = sorted_sequences[index++];
		}

		smaller_than_index--;
		// assigning the remaining sequences that do not overlapp; first the one that came before the new offset
		for (; smaller_than_index >= 0; smaller_than_index--) {
			copy_order[copy_order.size++] = sorted_sequences[smaller_than_index];
		}
		// the remaining sequences that come after the new offset
		for (; index < count; index++) {
			copy_order[copy_order.size++] = sorted_sequences[index];
		}
		// replacing the keys
		for (size_t index = 0; index < size; index++) {
			m_keys[index + buffer_offset] = new_first + index;
		}
		return buffer_offset;
	}

	unsigned int SequentialTable::CoalesceSequences(
		Substream* sorted_sequences,
		unsigned int new_first,
		unsigned int size,
		Stream<Substream>& copy_order,
		unsigned int min_size,
		unsigned int max_size
	) {
		size_t count = 0;
		for (size_t index = 0; index < m_sequence_size; index++) {
			if (m_sequences[index].size >= min_size && m_sequences[index].size <= max_size) {
				sorted_sequences[count].offset = m_sequences[index].buffer_start;
				sorted_sequences[count++].size = m_sequences[index].size;
			}
		}

		function::insertion_sort(sorted_sequences, count, 1);

		// if the size is not appropiate
		size_t buffer_offset = m_ranges.Request(size);
		ECS_ASSERT(buffer_offset != 0xFFFFFFFFFFFFFFFF, "Attempting to coalesce sequences in static sequential table with a bigger size than possible");

		// iterating until the first sequences that overlapps with the new allocated sequence
		size_t index = 0;
		while (sorted_sequences[index].offset < buffer_offset) {
			index++;
		}
		// keeping the index for later
		int64_t smaller_than_index = index;
		// copying first the sequences that overlapp starting with the lowest buffer_start until the biggest
		while (sorted_sequences[index].offset >= buffer_offset && sorted_sequences[index].offset < buffer_offset + size - 1 && index < count) {
			copy_order[copy_order.size++] = sorted_sequences[index++];
		}

		smaller_than_index--;
		// assigning the remaining sequences that do not overlapp; first the one that came before the new offset
		for (; smaller_than_index >= 0; smaller_than_index--) {
			copy_order[copy_order.size++] = sorted_sequences[smaller_than_index];
		}
		// the remaining sequences that come after the new offset
		for (; index < count; index++) {
			copy_order[copy_order.size++] = sorted_sequences[index];
		}
		// replacing the keys
		for (size_t index = 0; index < size; index++) {
			m_keys[index + buffer_offset] = new_first + index;
		}
		return buffer_offset;
	}

	unsigned int SequentialTable::CoalesceSequences(
		Substream* sorted_sequences,
		unsigned int new_first,
		unsigned int size,
		Stream<Substream>& copy_order,
		unsigned int min_size,
		unsigned int max_size,
		unsigned int count
	) {
		size_t local_count = 0;
		for (size_t index = 0; index < m_sequence_size; index++) {
			if (m_sequences[index].size >= min_size && m_sequences[index].size <= max_size) {
				sorted_sequences[local_count].offset = m_sequences[index].buffer_start;
				sorted_sequences[local_count].size = m_sequences[index].size;
				local_count++;
				if (local_count == count)
					break;
			}
		}

		function::insertion_sort(sorted_sequences, local_count, 1);

		// if the size is not appropiate
		size_t buffer_offset = m_ranges.Request(size);
		ECS_ASSERT(buffer_offset != 0xFFFFFFFFFFFFFFFF, "Attempting to coalesce sequences in static sequential table with a bigger size than possible");

		// iterating until the first sequences that overlapps with the new allocated sequence
		size_t index = 0;
		while (sorted_sequences[index].offset < buffer_offset) {
			index++;
		}
		// keeping the index for later
		int64_t smaller_than_index = index;

		// copying first the sequences that overlapp starting with the lowest buffer_start until the biggest
		while (sorted_sequences[index].offset >= buffer_offset && sorted_sequences[index].offset < buffer_offset + size - 1 && index < local_count) {
			copy_order[copy_order.size++] = sorted_sequences[index++];
		}

		// assigning the remaining sequences that do not overlapp; first the one that came before the new offset
		smaller_than_index--;
		for (; smaller_than_index >= 0; smaller_than_index--) {
			copy_order[copy_order.size++] = sorted_sequences[smaller_than_index];
		}
		// the remaining sequences that come after the new offset
		for (; index < local_count; index++) {
			copy_order[copy_order.size++] = sorted_sequences[index];
		}
		// replacing the keys
		for (size_t index = 0; index < size; index++) {
			m_keys[index + buffer_offset] = new_first + index;
		}
		return buffer_offset;
	}

	int SequentialTable::Find(unsigned int key) const {
		ECS_ASSERT(m_sequence_size > 0);

		unsigned int first, last;
			
		for (size_t sequence_index = 0; sequence_index < m_sequence_size; sequence_index++) {
			first = m_sequences[sequence_index].first;
			last = m_sequences[sequence_index].last;
			if (key >= first && key <= last) {
				unsigned int buffer_index = m_sequences[sequence_index].buffer_start + key - first;
				return m_sequences[sequence_index].buffer_start + m_keys[buffer_index];
			}
		}

		// error handling - the index was not found
		ECS_ASSERT(false);
		return -1;
	}

	unsigned int SequentialTable::Find(unsigned int key, unsigned int sequence_index) const {
		ECS_ASSERT(key >= m_sequences[sequence_index].first && key <= m_sequences[sequence_index].last);
		unsigned int buffer_index = m_sequences[sequence_index].buffer_start + key - m_sequences[sequence_index].first;
		return m_sequences[sequence_index].buffer_start + m_keys[buffer_index];
	}

	unsigned int SequentialTable::DeleteItem(unsigned int& value) {
		ECS_ASSERT(m_sequence_size > 0);
		unsigned int first, last;
		size_t i;
		for (i = 0; i < m_sequence_size; i++) {
			first = m_sequences[i].first;
			last = m_sequences[i].last;
			if (value >= first && value <= last) {
				return DeleteItem(value, i);
			}
		}
		return 0xFFFFFFFF;
	}

	unsigned int SequentialTable::DeleteItem(unsigned int& key, Stream<unsigned int>& deleted_sequence_first) {
		ECS_ASSERT(m_sequence_size > 0);
		unsigned int first, last;
		size_t i;
		for (i = 0; i < m_sequence_size; i++) {
			first = m_sequences[i].first;
			last = m_sequences[i].last;
			if (key >= first && key <= last) {
				return DeleteItem(key, i, deleted_sequence_first);
			}
		}
		return 0xFFFFFFFF;
	}

	unsigned int SequentialTable::DeleteItem(unsigned int& value, unsigned int sequence_index) {
		ECS_ASSERT(m_sequence_size > 0 && sequence_index < m_max_sequences);
		// change the last index
		m_sequences[sequence_index].size--;
		unsigned int temp = m_keys[m_sequences[sequence_index].buffer_start + value - m_sequences[sequence_index].first];
		m_keys[m_sequences[sequence_index].buffer_start + temp] = m_keys[m_sequences[sequence_index].buffer_start + m_sequences[sequence_index].size];
		m_keys[m_sequences[sequence_index].buffer_start + m_keys[m_sequences[sequence_index].buffer_start + m_sequences[sequence_index].size]] = temp;
		value = m_sequences[sequence_index].buffer_start + temp;

		if (m_sequences[sequence_index].size == 0)
			DeleteSequence(sequence_index, true);
		return m_sequences[sequence_index].buffer_start + m_sequences[sequence_index].size;
	}

	unsigned int SequentialTable::DeleteItem(unsigned int& value, unsigned int sequence_index, Stream<unsigned int>& deleted_sequence_first) {
		ECS_ASSERT(m_sequence_size > 0 && sequence_index < m_max_sequences);
		// change the last index
		m_sequences[sequence_index].size--;
		unsigned int temp = m_keys[m_sequences[sequence_index].buffer_start + value - m_sequences[sequence_index].first];
		m_keys[m_sequences[sequence_index].buffer_start + temp] = m_keys[m_sequences[sequence_index].buffer_start + m_sequences[sequence_index].size];
		m_keys[m_sequences[sequence_index].buffer_start + m_keys[m_sequences[sequence_index].buffer_start + m_sequences[sequence_index].size]] = temp;
		value = m_sequences[sequence_index].buffer_start + temp;

		if (m_sequences[sequence_index].size == 0)
			DeleteSequence(sequence_index, deleted_sequence_first);
		return m_sequences[sequence_index].buffer_start + m_sequences[sequence_index].size;
	}

	void SequentialTable::DeleteSequence(unsigned int first) {
		ECS_ASSERT(m_sequence_size > 0);
		for (size_t index = 0; index < m_sequence_size; index++) {
			unsigned int sequence_first = m_sequences[index].first;
			if (sequence_first == first) {
				m_ranges.Free(m_sequences[index].buffer_start);
				m_sequence_size--;
				m_sequences[index] = m_sequences[m_sequence_size];
				return;
			}
		}
	}

	void SequentialTable::DeleteSequence(unsigned int first, Stream<unsigned int>& deleted_sequence_first) {
		ECS_ASSERT(m_sequence_size > 0);
		for (size_t index = 0; index < m_sequence_size; index++) {
			unsigned int sequence_first = m_sequences[index].first;
			if (sequence_first == first) {
				deleted_sequence_first[deleted_sequence_first.size++] = sequence_first;
				m_ranges.Free(m_sequences[index].buffer_start);
				m_sequence_size--;
				m_sequences[index] = m_sequences[m_sequence_size];
				return;
			}
		}
	}

	void SequentialTable::DeleteSequence(unsigned int index, bool is_index) {
		ECS_ASSERT(index >= 0 && index < m_sequence_size);
		m_ranges.Free(m_sequences[index].buffer_start);
		m_sequence_size--;
		m_sequences[index] = m_sequences[m_sequence_size];
	}

	void SequentialTable::DeleteSequence(unsigned int index, bool is_index, Stream<unsigned int>& deleted_sequence_first) {
		ECS_ASSERT(index >= 0 && index < m_sequence_size);
		m_ranges.Free(m_sequences[index].buffer_start);
		deleted_sequence_first[deleted_sequence_first.size++] = m_sequences[index].first;
		m_sequence_size--;
		m_sequences[index] = m_sequences[m_sequence_size];
	}

	unsigned int SequentialTable::Insert(unsigned int first, unsigned int size) {
		if (m_max_sequences == m_sequence_size)
			return 0xFFFFFFFF;

		size_t buffer_start = m_ranges.Request(size);
		if (buffer_start == 0xFFFFFFFFFFFFFFFF)
			return 0xFFFFFFFF;

		m_sequences[m_sequence_size].buffer_start = buffer_start;
		m_sequences[m_sequence_size].first = first;
		m_sequences[m_sequence_size].last = first + size - 1;
		m_sequences[m_sequence_size].size = size;
		m_sequence_size++;
		unsigned int assignment_index = 0;
		for (size_t index = buffer_start; index < buffer_start + size; index++, assignment_index++) {
			m_keys[index] = assignment_index;
		}

		return buffer_start;
	}

	unsigned int SequentialTable::GetCoalesceSequencesItemCount(unsigned int& sequence_index) const {
		return GetCoalesceSequencesItemCount(sequence_index, m_sequence_size);
	}

	unsigned int SequentialTable::GetCoalesceSequencesItemCount(
		unsigned int& sequence_index, 
		Stream<unsigned int>& sequence_first
	) const {
		return GetCoalesceSequencesItemCount(sequence_index, m_sequence_size, sequence_first);
	}

	unsigned int SequentialTable::GetCoalesceSequencesItemCount(unsigned int& sequence_index, unsigned int count) const {
		size_t item_total = 0;
		for (size_t index = 0; index < count; index++) {
			item_total += m_sequences[index].size;
		}
		sequence_index = m_sequence_size - count;
		return item_total;
	}

	unsigned int SequentialTable::GetCoalesceSequencesItemCount(
		unsigned int& sequence_index, 
		unsigned int count, 
		Stream<unsigned int>& sequence_first
	) const {
		size_t item_total = 0;
		for (size_t index = 0; index < count; index++) {
			item_total += m_sequences[index].size;
			sequence_first[index] = m_sequences[index].first;
		}
		sequence_first.size = count;
		sequence_index = m_sequence_size - count;
		return item_total;
	}

	unsigned int SequentialTable::GetCoalesceSequencesItemCount(
		unsigned int& sequence_index, 
		unsigned int min_size, 
		unsigned int max_size
	) const {
		size_t item_total = 0;
		sequence_index = m_sequence_size;
		for (size_t index = 0; index < m_sequence_size; index++) {
			if (m_sequences[index].size >= min_size && m_sequences[index].size <= max_size) {
				item_total += m_sequences[index].size;
				sequence_index--;
			}
		}
		return item_total;
	}

	unsigned int SequentialTable::GetCoalesceSequencesItemCount(
		unsigned int& sequence_index,
		unsigned int min_size, 
		unsigned int max_size,
		Stream<unsigned int>& sequence_first
	) const {
		size_t item_total = 0;
		sequence_index = m_sequence_size;
		for (size_t index = 0; index < m_sequence_size; index++) {
			if (m_sequences[index].size >= min_size && m_sequences[index].size <= max_size) {
				item_total += m_sequences[index].size;
				sequence_first[sequence_first.size++] = m_sequences[index].first;
				sequence_index--;
			}
		}
		return item_total;
	}

	unsigned int SequentialTable::GetCoalesceSequencesItemCount(
		unsigned int& sequence_index,
		unsigned int min_size, 
		unsigned int max_size, 
		unsigned int count, 
		Stream<unsigned int>& sequence_first
	) const {
		size_t item_total = 0;
		size_t local_count = 0;
		for (size_t index = 0; index < m_sequence_size; index++) {
			if (m_sequences[index].size >= min_size && m_sequences[index].size <= max_size) {
				item_total += m_sequences[index].size;
				sequence_first[local_count] = m_sequences[index].first;
				local_count++;
				if (local_count == count) {
					return item_total;
				}
			}
		}
		sequence_index = m_sequence_size - local_count;
		return item_total;
	}

	unsigned int SequentialTable::GetCoalesceSequencesItemCount(
		unsigned int& sequence_index, 
		unsigned int min_size, 
		unsigned int max_size, 
		unsigned int count
	) const {
		size_t item_total = 0;
		size_t local_count = 0;
		for (size_t index = 0; index < m_sequence_size; index++) {
			if (m_sequences[index].size >= min_size && m_sequences[index].size <= max_size) {
				item_total += m_sequences[index].size;
				local_count++;
				if (local_count == count) {
					return item_total;
				}
			}
		}
		sequence_index = m_sequence_size - local_count;
		return item_total;
	}

	void SequentialTable::GetBiggestFreeSequenceAndAllocate(unsigned int first, unsigned int& buffer_offset, unsigned int& size) {
		ECS_ASSERT(m_max_sequences > m_sequence_size);
		// creating local size_t's to work with the block range API that requires references to size_t's
		unsigned int local_offset, local_size;
		size = 0;
		m_ranges.RequestBiggestBlock(local_offset, local_size);
		if ( local_size != 0 ) {
			buffer_offset = local_offset;
			size = local_size;

			m_sequences[m_sequence_size].first = first;
			m_sequences[m_sequence_size].last = first + size - 1;
			m_sequences[m_sequence_size].buffer_start = buffer_offset;
			m_sequences[m_sequence_size].size = size;
			m_sequence_size++;
		}
	}

	size_t SequentialTable::GetSequenceCount() const {
		return m_sequence_size;
	}

	Sequence* SequentialTable::GetSequenceBuffer() const {
		return m_sequences;
	}

	unsigned int SequentialTable::GetSequenceOf(unsigned int key) const {
		for (size_t sequence_index = 0; sequence_index < m_sequence_size; sequence_index++) {
			if (m_sequences[sequence_index].first <= key && m_sequences[sequence_index].last >= key) {
				return sequence_index;
			}
		}
		return 0xFFFFFFFF;
	}

	void SequentialTable::GetSequences(Stream<Sequence>& sequences) const {
		sequences.buffer = m_sequences;
		sequences.size = m_sequence_size;
	}

	size_t SequentialTable::GetKeyBuffer(unsigned int*& buffer) const {
		buffer = m_keys.buffer;
		return m_keys.size;
	}

	void SequentialTable::GetKeys(Stream<unsigned int>& keys) const {
		for (size_t sequence_index = 0; sequence_index < m_sequence_size; sequence_index++) {
			for (size_t index = 0; index < m_sequences[sequence_index].size; index++) {
				keys[keys.size++] = m_keys[m_sequences[sequence_index].buffer_start + index] + m_sequences[sequence_index].first;
			}
		}
	}

	const void* SequentialTable::GetAllocatedBuffer() const
	{
		return m_ranges.GetAllocatedBuffer();
	}

	size_t SequentialTable::MemoryOfBlockRange(unsigned int max_sequences) {
		return BlockRange::MemoryOf(max_sequences);
	}

	size_t SequentialTable::MemoryOfKeys(unsigned int number) {
		return sizeof(unsigned int) * number;
	}

	size_t SequentialTable::MemoryOfSequence(unsigned int max_sequences) {
		return sizeof(Sequence) * max_sequences;
	}

	size_t SequentialTable::MemoryOf(unsigned int number, unsigned int max_sequences) {
		return sizeof(Sequence) * (max_sequences) + sizeof(unsigned int) * (number) + BlockRange::MemoryOf(max_sequences);
	}


}