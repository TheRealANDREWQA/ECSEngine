#pragma once

#include "../Core.h"

namespace ECSEngine {

	struct ECSENGINE_API BooleanBitField
	{
		BooleanBitField(void* buffer, size_t size);
			
		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(BooleanBitField);
			
		void Set(size_t index);

		void Clear(size_t index);

		bool Get(size_t index) const;

		// Returns true if the index is already set, if not, it will set it and returns false.
		bool GetAndSet(size_t index);

		static size_t MemoryOf(size_t count);

		unsigned char* m_buffer;
		size_t m_size;
	};

}

