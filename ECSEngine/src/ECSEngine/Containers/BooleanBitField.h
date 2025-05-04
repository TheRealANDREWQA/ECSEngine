#pragma once
#include "../Core.h"
#include "../Allocators/AllocatorTypes.h"

namespace ECSEngine {

	struct ECSENGINE_API BooleanBitField
	{
		BooleanBitField() : m_buffer(nullptr), m_size(0) {}
		BooleanBitField(void* buffer, size_t size);
		
		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(BooleanBitField);
		
		// Element count must be the logical number of elemenets, not the actual byte size
		void Initialize(AllocatorPolymorphic allocator, size_t element_count);

		void Deallocate(AllocatorPolymorphic allocator);

		// Iterates over all values and calls the functor with parameters (size_t index, bool value).
		// The functor must return true when early exit is activated, else it can return void.
		// This function returns true if it early exited, else false.
		template<bool early_exit = false, typename Functor>
		bool ForEach(Functor&& functor) {
			size_t byte_index = 0;
			size_t bit = 1;

			// Keep a local value for the current byte, such that the compiler
			// Doesn't generate a read for each iteration.
			size_t current_byte = m_size == 0 ? 0 : m_buffer[0];
			for (size_t index = 0; index < m_size; index++) {
				if constexpr (early_exit) {
					if (functor(index, (current_byte & bit) != 0)) {
						return true;
					}
				}
				else {
					functor(index, (current_byte & bit) != 0);
				}

				bit <<= 1;
				if (bit > UCHAR_MAX) {
					byte_index++;
					bit = 1;
					current_byte = m_buffer[byte_index];
				}
			}

			return false;
		}
			
		void Set(size_t index);

		void Clear(size_t index);

		bool Get(size_t index) const;

		// Returns true if the index is already set, if not, it will set it and returns false.
		bool GetAndSet(size_t index);

		static size_t MemoryOf(size_t count);

		unsigned char* m_buffer;
		// The number of "logical" values (i.e. 5 for 5 elements, not 1 byte).
		size_t m_size;
	};

}

