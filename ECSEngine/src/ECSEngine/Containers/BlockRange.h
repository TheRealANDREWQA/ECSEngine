#pragma once
#include "../Core.h"

namespace ECSEngine {

	/* BlockRange is used to express chunks of varying sizes by saving each as a non-overlapping pair 
	* of start - end stored in random order. Start values are stored from [0; m_capacity] 
	* range and end values are stored in [m_capacity; m_capacity * 2] range.
	* Capacity represents how many blocks it can have
	* The a start and end pair is stored like this [start; end) (start inclusive, end exclusive)
	* such that no addition needs to be done when checking the size
	*/
	struct ECSENGINE_API BlockRange
	{
		BlockRange(void* buffer, unsigned int capacity, unsigned int max_index);

		BlockRange& operator = (const BlockRange& other) = default;

		void AddFreeBlock(unsigned int start, unsigned int end);

		void AddUsedBlock(unsigned int start, unsigned int end);

		void RemoveFreeBlock(unsigned int index);

		void RemoveUsedBlock(unsigned int index);

		// Returns -1 if cannot fulfill the request
		unsigned int Request(unsigned int size);

		// Size will be 0 if all the blocks have been allocated or there is no memory left
		void RequestBiggestBlock(unsigned int& buffer_offset, unsigned int& size);

		// Returns the new location of the block, if it changed, else the block has been resized and returns the same start
		// It can return -1 if the request cannot be fullfilled (no more blocks capacity or no enough size)
		unsigned int ReallocateBlock(unsigned int start, unsigned int new_size);

		// Returns the new location of the block, if it changed, else the block has been resized and returns the same start
		// It can return -1 if the request cannot be fullfilled (no more blocks capacity or no enough size)
		// If the copy_size is left at 0, it will presume that the entire block is to be copied
		// It will copy the memory from the given storage_buffer
		unsigned int ReallocateBlockAndCopy(unsigned int start, unsigned int new_size, void* storage_buffer, unsigned int copy_size = 0);

		// The return value is only useful when using assert_if_not_found set to false
		// in which case it will return true if the deallocation was performed, else false
		template<bool assert_if_not_found = true>
		bool Free(unsigned int start);

		void Clear();

		// This is not the actual block range byte capacity
		// Instead is the maximum number of blocks that there can be at the same time
		unsigned int GetCapacity() const;

		unsigned int GetStart(unsigned int index) const;

		unsigned int GetEnd(unsigned int index) const;

		unsigned int GetFreeBlockCount() const;

		unsigned int GetUsedBlockCount() const;

		const void* GetAllocatedBuffer() const;

		void SetStart(unsigned int index, unsigned int value);

		void SetEnd(unsigned int index, unsigned int value);

		static size_t MemoryOf(unsigned int number);

		unsigned int* m_buffer;
		unsigned int m_capacity;
		unsigned int m_free_block_count;
		unsigned int m_used_block_count;
	};

}

