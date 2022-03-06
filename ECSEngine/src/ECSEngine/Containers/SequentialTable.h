#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "BlockRange.h"
#include "Stream.h"
#include "../Internal/InternalStructures.h"

namespace ECSEngine {

	// the buffer should be aligned to 8 byte boundary with max_sequence multiple of 8 and capacity multiple of 2
	struct ECSENGINE_API SequentialTable {
	public:
		SequentialTable();

		SequentialTable(
			void* block_range_buffer, 
			void* sequence_allocation, 
			void* key_allocation, 
			unsigned int capacity, 
			unsigned int max_sequences
		);

		SequentialTable(void* allocation, unsigned int capacity, unsigned int max_sequences);

		SequentialTable& operator = (const SequentialTable& other) = default;
		SequentialTable& operator = (SequentialTable&& other) = default;

		void Clear();

		// size parameter should be given from GetCoalesceSequencesItemCount
		unsigned int CoalesceSequences(
			Substream* sequences,
			unsigned int new_first, 
			unsigned int size, 
			Stream<Substream>& copy_order
		);
			
		// size parameter should be given from GetCoalesceSequencesItemCount
		unsigned int CoalesceSequences(
			Substream* sequences,
			unsigned int new_first, 
			unsigned int size, 
			Stream<Substream>& copy_order, 
			unsigned int count
		);
			
		// size parameter should be given from GetCoalesceSequencesItemCount
		unsigned int CoalesceSequences(
			Substream* sequences, 
			unsigned int new_first, 
			unsigned int size, 
			Stream<Substream>& copy_order, 
			unsigned int min_size,
			unsigned int max_size
		);
			
		// size parameter should be given from GetCoalesceSequencesItemCount
		unsigned int CoalesceSequences(
			Substream* sequences, 
			unsigned int new_first, 
			unsigned int size, 
			Stream<Substream>& copy_order, 
			unsigned int min_size,
			unsigned int max_size,
			unsigned int count
		);
			
		// return the last item that will replace the index and the index will be changed to its buffer index
		// 0xFFFFFFFF means failure; the value is not in table
		unsigned int DeleteItem(unsigned int& key);

		// return the last item that will replace the index and the index will be changed to its buffer index
		// 0xFFFFFFFF means failure; the value is not in table; if the caller wants to be notified that a sequence 
		// was deleted
		unsigned int DeleteItem(unsigned int& key, Stream<unsigned int>& deleted_sequence_first);

		// return the last item that will replace the index and the index will be changed to its buffer index
		unsigned int DeleteItem(unsigned int& key, unsigned int sequence_index);

		// return the last item that will replace the index and the index will be changed to its buffer index
		// if the caller wants to be notified that a sequence was deleted
		unsigned int DeleteItem(unsigned int& key, unsigned int sequence_index, Stream<unsigned int>& deleted_sequence_first);

		void DeleteSequence(unsigned int first);

		// if the caller wants to be notified if a sequence was deleted
		void DeleteSequence(unsigned int index, Stream<unsigned int>& deleted_sequence_first);

		void DeleteSequence(unsigned int index, bool is_index);

		// if the caller wants to be notified if a sequence was deleted
		void DeleteSequence(unsigned int index, bool is_index, Stream<unsigned int>& deleted_sequence_fist);

		int Find(unsigned int key) const;

		unsigned int Find(unsigned int key, unsigned int sequence) const;

		// Inserts a new sequence; 0xFFFFFFFF means failed to insert 
		unsigned int Insert(unsigned int first, unsigned int size);

		size_t GetSequenceCount() const;
			
		// the parameter will hold the sequence index of the future sequence
		unsigned int GetCoalesceSequencesItemCount(unsigned int& sequence_index) const;

		// the parameter will hold the sequence index of the future sequence
		unsigned int GetCoalesceSequencesItemCount(unsigned int& sequence_index, Stream<unsigned int>& sequence_first) const;
			
		// the parameter will hold the sequence index of the future sequence
		unsigned int GetCoalesceSequencesItemCount(unsigned int& sequence_index, unsigned int count) const;

		// the parameter will hold the sequence index of the future sequence
		unsigned int GetCoalesceSequencesItemCount(
			unsigned int& sequence_index, 
			unsigned int count, 
			Stream<unsigned int>& sequence_first
		) const;

		// the parameter will hold the sequence index of the future sequence
		unsigned int GetCoalesceSequencesItemCount(
			unsigned int& sequence_index, 
			unsigned int min_size, 
			unsigned int max_size
		) const;

		// the parameter will hold the sequence index of the future sequence
		unsigned int GetCoalesceSequencesItemCount(
			unsigned int& sequence_index,
			unsigned int min_size, 
			unsigned int max_size, 
			Stream<unsigned int>& sequence_first
		) const;

		// the parameter will hold the sequence index of the future sequence
		unsigned int GetCoalesceSequencesItemCount(
			unsigned int& sequence_index,
			unsigned int min_size, 
			unsigned int max_size, 
			unsigned int count, 
			Stream<unsigned int>& sequence_first
		) const;

		// the parameter will hold the sequence index of the future sequence
		unsigned int GetCoalesceSequencesItemCount(
			unsigned int& sequence_index,
			unsigned int min_size, 
			unsigned int max_size,
			unsigned int count
		) const;

		// the sequence will already be allocated after the function call; size will be 0 if there is no position left
		void GetBiggestFreeSequenceAndAllocate(unsigned int first, unsigned int& buffer_offset, unsigned int& size);

		Sequence* GetSequenceBuffer() const;

		unsigned int GetSequenceOf(unsigned int key) const;

		void GetSequences(Stream<Sequence>& sequences) const;

		size_t GetKeyBuffer(unsigned int*& buffer) const;

		void GetKeys(Stream<unsigned int>& keys) const;

		const void* GetAllocatedBuffer() const;

		static size_t MemoryOfBlockRange(unsigned int max_sequences);

		static size_t MemoryOfSequence(unsigned int max_sequences);

		static size_t MemoryOfKeys(unsigned int number);

		static size_t MemoryOf(unsigned int number, unsigned int max_sequence);

	//private:
		Sequence* m_sequences;
		unsigned int m_sequence_size;
		unsigned int m_max_sequences;
		Stream<unsigned int> m_keys;
		BlockRange m_ranges;
	};

}
