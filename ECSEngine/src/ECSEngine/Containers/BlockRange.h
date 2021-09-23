#pragma once
#include "../Core.h"

namespace ECSEngine {

	namespace containers {

		/* BlockRange is used to express chunks of varying sizes by saving each as a non-overlapping pair 
		* of start - end stored in random order. Starting values are stored from [0; m_capacity / 2] 
		* range and ending values are stored in [m_capacity / 2 + 1; m_capacity] range.
		* Availabilty is determined using a BooleanBitField.
		* Capacity should be multiple of 8 and represents how many blocks it can have
		*/
		class ECSENGINE_API BlockRange
		{
		public:
			BlockRange(void* buffer, unsigned int capacity, unsigned int max_index);

			BlockRange& operator = (const BlockRange& other) = default;

			void AddDebugStart(unsigned int start);

			bool IsDebugStart(unsigned int start, unsigned int& difference, unsigned int& closest_index) const;

			void RemoveDebugStart(unsigned int start);

			// returns 0xFFFFFFFFFFFFFFFF if cannot fulfill the request
			unsigned int Request(unsigned int size);

			// size will be 0 if all the blocks have been allocated or there is no memory left
			void RequestBiggestBlock(unsigned int& buffer_offset, unsigned int& size);

			template<bool assert_if_not_found = true>
			void Free(unsigned int start);

			void Clear();

			unsigned int DebugCheckUsedBlocks() const;

			unsigned int GetCapacity() const;

			unsigned int GetStart(unsigned int index) const;

			unsigned int GetEnd(unsigned int index) const;

			unsigned int GetFreeBlockCount() const;

			unsigned int GetUsedBlockCount() const;

			const void* GetAllocatedBuffer() const;

			void SetStart(unsigned int index, unsigned int value);

			void SetEnd(unsigned int index, unsigned int value);

			void SetDebugBuffer(unsigned int* buffer);

			static size_t MemoryOf(unsigned int number);

			static size_t MemoryOfDebug(unsigned int number);

		private:
			unsigned int* m_buffer;
			unsigned int m_capacity;
			unsigned int m_free_block_count;
			unsigned int m_used_block_count;
#ifdef ECSENGINE_DEBUG
			unsigned int* m_debug_starts;
#endif
		};
	}
}

