#pragma once

#include "../Core.h"

namespace ECSEngine {

	// 1 means empty, 0 means full in context of block range in order to use Bitscan intrinsic
	struct ECSENGINE_API BooleanBitField
	{
		BooleanBitField(void* buffer, size_t size);
			
		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(BooleanBitField);
			
		void Set(size_t index);

		void Clear(size_t index);

		unsigned char Get(size_t index) const;

		static size_t MemoryOf(size_t count);

	//private:
		unsigned char* m_buffer;
		size_t m_size;
	};

}

