#pragma once

#include "../Core.h"

namespace ECSEngine {

#define ECS_BIT0 unsigned char(128)
#define ECS_BIT1 unsigned char(64)
#define ECS_BIT2 unsigned char(32)
#define ECS_BIT3 unsigned char(16)
#define ECS_BIT4 unsigned char(8)
#define ECS_BIT5 unsigned char(4)
#define ECS_BIT6 unsigned char(2)
#define ECS_BIT7 unsigned char(1)

		struct ECSENGINE_API byte {
		public:
			byte() : m_value(0) {}
			byte(unsigned char value) : m_value(value) {}

			unsigned char GetValue() const;

			unsigned char GetBit(unsigned char index) const;

			unsigned char GetBits(unsigned char mask) const;

			void SetBits(unsigned char mask);

			void SetBit(size_t index);

			void ClearBits(unsigned char mask);

			void ClearBit(size_t index);

		//private:
			unsigned char m_value;
		};

		union ECSENGINE_API byte64 {
		public:
			byte64() : value(0) {}
			size_t value;
			byte bytes[8];
		};

		// 1 means empty, 0 means full in context of block range in order to use Bitscan intrinsic
		struct ECSENGINE_API BooleanBitField
		{
		public:
			BooleanBitField(void* buffer, size_t size);
			BooleanBitField(void* buffer, size_t size, bool should_clear);
			
			BooleanBitField& operator = (const BooleanBitField& other) = default;
			BooleanBitField& operator = (BooleanBitField&& other) = default;
			
			void Free(size_t index);

			void SetItem(size_t index);

			void ClearItem(size_t index);

			size_t Request();

			void Reset();

			const void* GetAllocatedBuffer() const;

			unsigned char GetBit(size_t index) const;

			static size_t MemoryOf(size_t bytes);

		//private:
			byte64* m_buffer;
			size_t m_size;
		};

}

