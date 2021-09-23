#include "ecspch.h"
#include "BooleanBitField.h"
#include "../Utilities/Assert.h"

namespace ECSEngine {
	
	
	namespace containers {

		unsigned char byte::GetValue() const {
			return m_value;
		}

		void byte::SetBits(unsigned char mask) {
			m_value |= mask;
		}

		void byte::ClearBits(unsigned char mask) {
			SetBits(mask);
			m_value ^= mask;
		}

		unsigned char byte::GetBits(unsigned char mask) const {
			return m_value & mask;
		}

		unsigned char byte::GetBit(unsigned char index) const {
			return GetBits(1 << index);
		}

		/*From lowest to highest, sets the bit to 1*/
		void byte::SetBit(size_t index) {
			m_value |= 1 << index;
		}

		/*From lowest to highest, sets the bit to 0*/
		void byte::ClearBit(size_t index) {
			SetBit(index);
			m_value ^= 1 << index;
		}

		BooleanBitField::BooleanBitField(void* buffer, size_t size) : m_buffer((byte64*)buffer), m_size(size) {}
		BooleanBitField::BooleanBitField(void* buffer, size_t size, bool should_clear) : m_buffer((byte64*)buffer), m_size(size) { Reset(); }
		
		void BooleanBitField::Free(size_t index) {
			ECS_ASSERT(index >= 0 && index < m_size);

			size_t byte64_index = index >> 6;
			size_t byte_index = (index - (index & (-64)) ) >> 3;
			unsigned char bit_index = index & 7;

			m_buffer[byte64_index].bytes[byte_index].SetBit(bit_index);

		}

		size_t BooleanBitField::Request() {
			unsigned long index;
			size_t byte64_index = 0;

			// the byte64_index is shifted to left 6 times to do a faster * 64

			unsigned char result = _BitScanForward64(&index, m_buffer[byte64_index].value);
			while ( result == 0 && byte64_index << 6 < m_size ) {
				byte64_index++;
				result = _BitScanForward64(&index, m_buffer[byte64_index].value);
			}
			if (byte64_index << 6 < m_size) {
				
				// index >> 3 is the same as index / 8
				// index & 7 is the same as index % 8
				// the bit index must be reversed to work
				// with the byte API

				m_buffer[byte64_index].bytes[index >> 3].ClearBit(index & 7);

				return (size_t)((byte64_index << 6) + index);
			}
			return m_size;
		}

		void BooleanBitField::Reset() {
			size_t bytes = m_size >> 3;
			memset((void*)m_buffer, 255, bytes);
		}

		/* sets the item to 1 */
		void BooleanBitField::SetItem(size_t index) {
			ECS_ASSERT(index >= 0 && index < m_size);

			size_t byte64_index = index >> 6;
			size_t byte_index = (index - (index & (-64))) >> 3;
			unsigned char bit_index = index & 7;

			m_buffer[byte64_index].bytes[byte_index].SetBit(bit_index);
		}
		/* sets the item to 0*/
		void BooleanBitField::ClearItem(size_t index) {
			ECS_ASSERT(index >= 0 && index < m_size);

			size_t byte64_index = index >> 6;
			size_t byte_index = (index - (index & (-64))) >> 3;
			unsigned char bit_index = index & 7;

			m_buffer[byte64_index].bytes[byte_index].ClearBit(bit_index);
		}

		const void* BooleanBitField::GetAllocatedBuffer() const {
			return m_buffer;
		}

		unsigned char BooleanBitField::GetBit(size_t index) const {
			ECS_ASSERT(index >= 0 && index < m_size);

			size_t byte64_index = index >> 6;
			size_t byte_index = (index - (index & (-64))) >> 3;
			unsigned char bit_index = index & 7;

			return m_buffer[byte64_index].bytes[byte_index].GetBit(bit_index);
		}

		size_t BooleanBitField::MemoryOf(size_t number) {
			return number >> 3 + ((number & 7) == 0) * 8;
		}
	}
}