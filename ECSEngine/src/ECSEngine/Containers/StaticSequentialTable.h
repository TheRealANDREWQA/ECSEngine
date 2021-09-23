#pragma once
#include "../Core.h"
#include "Stream.h"
#include "BlockRange.h"
#include "../Internal/InternalStructures.h"

namespace ECSEngine {

	ECS_CONTAINERS;

	class ECSENGINE_API StaticSequentialTable
	{
	public:
		StaticSequentialTable();
		StaticSequentialTable(void* allocation, unsigned int capacity, unsigned int max_sequences);

		StaticSequentialTable& operator = (const StaticSequentialTable& other) = default;

		void Clear();

		// size parameter should be given from GetCoalesceSequencesItemCount
		unsigned int CoalesceSequences(unsigned int new_first, unsigned int size, Stream<Substream>& copy_order);

		// size parameter should be given from GetCoalesceSequencesItemCount
		unsigned int CoalesceSequences(unsigned int new_first, unsigned int size, Stream<Substream>& copy_order, unsigned int count);
		
		// size parameter should be given from GetCoalesceSequencesItemCount
		unsigned int CoalesceSequences(unsigned int new_first, unsigned int size, Stream<Substream>& copy_order, unsigned int min_size, unsigned int max_size);
		
		// size parameter should be given from GetCoalesceSequencesItemCount
		unsigned int CoalesceSequences(unsigned int new_first, unsigned int size, Stream<Substream>& copy_order, unsigned int min_size, unsigned int max_size, unsigned int count);

		// return the first key in the sequence that is being deleted
		unsigned int DeleteSequence(unsigned int first);
		
		// return the first key in the sequence that is being deleted
		unsigned int DeleteSequence(unsigned int index, bool is_index);
		
		int Find(unsigned int key) const;
		
		unsigned int Find(unsigned int key, unsigned int sequence_index) const;
		
		// Inserts a new sequence; 0xFFFFFFFF means failed to insert 
		unsigned int Insert(unsigned int first, unsigned int size);

		unsigned int GetCapacity() const;
		
		unsigned int GetCoalesceSequencesItemCount() const;
		
		unsigned int GetCoalesceSequencesItemCount(unsigned int count) const;
		
		unsigned int GetCoalesceSequencesItemCount(unsigned int min_size, unsigned int max_size) const;
		
		unsigned int GetCoalesceSequencesItemCount(unsigned int min_size, unsigned int max_size, unsigned int count) const;
		
		unsigned int GetSequenceCount() const;
		
		unsigned int GetSequenceOf(unsigned int key) const;
		
		unsigned int GetBiggestFreeSequenceAndAllocate(unsigned int first, unsigned int& buffer_offset, unsigned int& size);
		
		Sequence* GetSequenceBuffer() const;
		
		void GetSequences(Stream<Sequence>& sequences) const;
		
		const void* GetAllocatedBuffer() const;

		static size_t MemoryOf(unsigned int max_sequences);

	//private:
		Stream<Sequence> m_sequences;
		unsigned int m_max_sequence_count;
		BlockRange m_ranges;
	};

}

