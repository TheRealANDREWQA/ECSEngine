#include "ecspch.h"
#include "../Utilities/Function.h"
#include "../Utilities/FunctionTemplates.h"
#include "StaticSequentialTable.h"

namespace ECSEngine {

	StaticSequentialTable::StaticSequentialTable() : m_max_sequence_count(0), m_ranges(nullptr, 0, 0), m_sequences() {}
	StaticSequentialTable::StaticSequentialTable(void* allocation, unsigned int capacity, unsigned int max_sequences) 
		: m_ranges(allocation, max_sequences, capacity), m_max_sequence_count(max_sequences) {
		uintptr_t buffer = (uintptr_t)allocation;
		buffer += BlockRange::MemoryOf(max_sequences);
		buffer = function::align_pointer(buffer, alignof(Sequence));
		m_sequences.buffer = (Sequence*)buffer;
		m_sequences.size = 0;
	}

	void StaticSequentialTable::Clear() {
		m_sequences.size = 0;
		m_ranges.Clear();
	}

	unsigned int StaticSequentialTable::CoalesceSequences(unsigned int new_first, unsigned int size, Stream<Substream>& copy_order) {
		Substream sorted_sequences[256];
		for (size_t index = 0; index < m_sequences.size; index++) {
			m_ranges.Free(m_sequences[index].buffer_start);
			sorted_sequences[index].offset = m_sequences[index].buffer_start;
			sorted_sequences[index].size = m_sequences[index].size;
		}

		function::insertion_sort(sorted_sequences, m_sequences.size, 1);

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
		while (sorted_sequences[index].offset >= buffer_offset && sorted_sequences[index].offset < (buffer_offset + size - 1) && index < m_sequences.size) {
			copy_order[copy_order.size++] = sorted_sequences[index++];
		}

		smaller_than_index--;
		// assigning the remaining sequences that do not overlapp; first the one that came before the new offset
		for (; smaller_than_index >= 0; smaller_than_index--) {
			copy_order[copy_order.size++] = sorted_sequences[smaller_than_index];
		}
		// the remaining sequences that come after the new offset
		for (; index < m_sequences.size; index++) {
			copy_order[copy_order.size++] = sorted_sequences[index];
		}
		return buffer_offset;
	}

	unsigned int StaticSequentialTable::CoalesceSequences(
		unsigned int new_first,
		unsigned int size, 
		Stream<Substream>& copy_order,
		unsigned int count
	) {
		Substream sorted_sequences[256];
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
		size_t offset = 0;
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
		return buffer_offset;
	}

	unsigned int StaticSequentialTable::CoalesceSequences(
		unsigned int new_first,
		unsigned int size,
		Stream<Substream>& copy_order,
		unsigned int min_size,
		unsigned int max_size
	) {
		// copying the sequences that fit in the interval and sorting them after buffer_start
		Substream sorted_sequences[256];
		unsigned int indices[256];
		size_t count = 0;
		for (size_t index = 0; index < m_sequences.size; index++) {
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
		return buffer_offset;
	}

	unsigned int StaticSequentialTable::CoalesceSequences(
		unsigned int new_first,
		unsigned int size,
		Stream<Substream>& copy_order,
		unsigned int min_size,
		unsigned int max_size,
		unsigned int count
	) {
		Substream sorted_sequences[256];
		size_t local_count = 0;
		for (size_t index = 0; index < m_sequences.size; index++) {
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

		smaller_than_index--;
		// assigning the remaining sequences that do not overlapp; first the one that came before the new offset
		for (; smaller_than_index >= 0; smaller_than_index--) {
			copy_order[copy_order.size++] = sorted_sequences[smaller_than_index];
		}
		// the remaining sequences that come after the new offset
		for (; index < local_count; index++) {
			copy_order[copy_order.size++] = sorted_sequences[index];
		}
		return buffer_offset;
	}

	unsigned int StaticSequentialTable::DeleteSequence(unsigned int first) {
		for (size_t index = 0; index < m_sequences.size; index++) {
			if (m_sequences[index].first == first) {
				return DeleteSequence(index, true);
			}
		}
	}

	unsigned int StaticSequentialTable::DeleteSequence(unsigned int index, bool is_index) {
		ECS_ASSERT(m_sequences.size > 0 && index < m_sequences.size);
		unsigned int sequence_first = m_sequences[index].first;
		m_ranges.Free(m_sequences[index].buffer_start);
		m_sequences.size--;
		m_sequences[index] = m_sequences[m_sequences.size];
		return sequence_first;
	}

	int StaticSequentialTable::Find(unsigned int key) const {
		for (size_t index = 0; index < m_sequences.size; index++) {
			if (m_sequences[index].first <= key && m_sequences[index].last >= key) {
				return m_sequences[index].buffer_start + key - m_sequences[index].first;
			}
		}
		return -1;
	}

	unsigned int StaticSequentialTable::Find(unsigned int key, unsigned int sequence_index) const {
		return m_sequences[sequence_index].buffer_start + key - m_sequences[sequence_index].first;
	}

	unsigned int StaticSequentialTable::Insert(unsigned int first, unsigned int size) {
		if (m_sequences.size == m_max_sequence_count) {
			return unsigned int(0xFFFFFFFF);
		}
		size_t buffer_offset = m_ranges.Request(size);

		if (buffer_offset == 0xFFFFFFFFFFFFFFFF) {
			return unsigned int(0xFFFFFFFF);
		}

		m_sequences[m_sequences.size].first = first;
		m_sequences[m_sequences.size].last = first + size - 1;
		m_sequences[m_sequences.size].size = size;
		m_sequences[m_sequences.size].buffer_start = buffer_offset;

		m_sequences.size++;

		return buffer_offset;
	}

	unsigned int StaticSequentialTable::GetCapacity() const {
		return m_ranges.GetCapacity();
	}

	unsigned int StaticSequentialTable::GetCoalesceSequencesItemCount() const {
		return GetCoalesceSequencesItemCount(m_sequences.size);
	}

	unsigned int StaticSequentialTable::GetCoalesceSequencesItemCount(unsigned int count) const {
		size_t item_total = 0;
		for (size_t index = 0; index < count; index++) {
			item_total += m_sequences[index].size;
		}
		return item_total;
	}

	unsigned int StaticSequentialTable::GetCoalesceSequencesItemCount(unsigned int min_size, unsigned int max_size) const {
		size_t item_total = 0;
		for (size_t index = 0; index < m_sequences.size; index++) {
			if (m_sequences[index].size >= min_size && m_sequences[index].size <= max_size) {
				item_total += m_sequences[index].size;
			}
		}
		return item_total;
	}

	unsigned int StaticSequentialTable::GetCoalesceSequencesItemCount(unsigned int min_size, unsigned int max_size, unsigned int count) const {
		size_t item_total = 0;
		size_t local_count = 0;
		for (size_t index = 0; index < m_sequences.size; index++) {
			if (m_sequences[index].size >= min_size && m_sequences[index].size <= max_size) {
				item_total += m_sequences[index].size;
				local_count++;
				if (local_count == count) {
					return item_total;
				}
			}
		}
		return item_total;
	}

	unsigned int StaticSequentialTable::GetSequenceCount() const {
		return m_sequences.size;
	}

	unsigned int StaticSequentialTable::GetBiggestFreeSequenceAndAllocate(
		unsigned int first, 
		unsigned int& buffer_offset, 
		unsigned int& size
	) {
		if (m_sequences.size == m_max_sequence_count) {
			size = 0;
			return 0xFFFFFFFF;
		}

		unsigned int local_offset, local_size;
		m_ranges.RequestBiggestBlock(local_offset, local_size);
		if (local_size != 0) {
			buffer_offset = local_offset;
			size = local_size;
			Insert(first, local_size);
			return m_sequences.size - 1;
		}
		return 0xFFFFFFFF;
	}

	Sequence* StaticSequentialTable::GetSequenceBuffer() const {
		return m_sequences.buffer;
	}

	void StaticSequentialTable::GetSequences(Stream<Sequence>& sequences) const {
		sequences = m_sequences;
	}

	const void* StaticSequentialTable::GetAllocatedBuffer() const
	{
		return m_ranges.GetAllocatedBuffer();
	}

	unsigned int StaticSequentialTable::GetSequenceOf(unsigned int entity) const {
		for (size_t index = 0; index < m_sequences.size; index++) {
			if (m_sequences[index].first <= entity && m_sequences[index].last >= entity) {
				return index;
			}
		}
		return 0xFFFFFFFF;
	}

	size_t StaticSequentialTable::MemoryOf(unsigned int max_sequences) {
		return BlockRange::MemoryOf(max_sequences) + sizeof(Sequence) * max_sequences + 8;
	}
}