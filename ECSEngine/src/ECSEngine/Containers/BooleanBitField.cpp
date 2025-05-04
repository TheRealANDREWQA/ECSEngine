#include "ecspch.h"
#include "BooleanBitField.h"
#include "../Utilities/Assert.h"
#include "../Allocators/AllocatorPolymorphic.h"

namespace ECSEngine {

	ECS_INLINE static size_t GetByte(size_t index) {
		return index & (~(size_t)0x07);
	}

	ECS_INLINE static size_t GetBit(size_t index) {
		return (size_t)1 << (index & (size_t)0x07);
	}

	void BooleanBitField::Initialize(AllocatorPolymorphic allocator, size_t element_count) {
		m_size = element_count;
		if (element_count > 0) {
			m_buffer = (unsigned char*)Allocate(allocator, MemoryOf(element_count), alignof(unsigned char));
		}
		else {
			m_buffer = nullptr;
		}
	}

	void BooleanBitField::Deallocate(AllocatorPolymorphic allocator) {
		if (m_buffer != nullptr && m_size > 0) {
			ECSEngine::Deallocate(allocator, m_buffer);
		}
	}

	BooleanBitField::BooleanBitField(void* buffer, size_t size) : m_buffer((unsigned char*)buffer), m_size(size) {}

	void BooleanBitField::Set(size_t index) {
		m_buffer[GetByte(index)] |= GetBit(index);
	}

	void BooleanBitField::Clear(size_t index) {
		m_buffer[GetByte(index)] &= ~GetBit(index);
	}

	bool BooleanBitField::Get(size_t index) const {
		return m_buffer[GetByte(index)] & GetBit(index);
	}

	bool BooleanBitField::GetAndSet(size_t index)
	{
		size_t byte = GetByte(index);
		size_t bit = GetBit(index);
		if ((m_buffer[byte] & bit) == 0) {
			m_buffer[byte] |= bit;
			return false;
		}
		return true;
	}

	size_t BooleanBitField::MemoryOf(size_t number) {
		return (number >> 3) + ((number & 7) != 0);
	}


}