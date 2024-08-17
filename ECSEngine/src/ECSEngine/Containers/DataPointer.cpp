#include "ecspch.h"
#include "DataPointer.h"
#include "../Allocators/AllocatorPolymorphic.h"

constexpr uintptr_t DATA_MASK = 0x000000000000FFFF;
constexpr uintptr_t POINTER_MASK = ~0xFFFF000000000000;

namespace ECSEngine {
	
	void DataPointer::Deallocate(AllocatorPolymorphic allocator)
	{
		void* allocation = GetPointer();
		if (allocation != nullptr && GetData() > 0) {
			ECSEngine::Deallocate(allocator, allocation);
			pointer = nullptr;
		}
	}

	unsigned short DataPointer::DecrementData(unsigned short count)
	{
		unsigned short data = GetData();
		bool is_zero = data < count;
		data = is_zero ? 0 : data - count;
		SetData(data);

		return data;
	}

	unsigned short DataPointer::IncrementData(unsigned short count) {
		unsigned short data = GetData();
		bool is_max = (USHORT_MAX - data) < count;
		data = is_max ? USHORT_MAX : data + count;
		SetData(data);

		return data;
	}

	unsigned short DataPointer::GetData() const
	{
		return (unsigned short)((uintptr_t)pointer >> 48 & DATA_MASK);
	}

	void DataPointer::SetData(unsigned short _data)
	{
		uintptr_t ptr = (uintptr_t)pointer;
		uintptr_t shifted_data = (uintptr_t)_data << 48;
		ptr &= POINTER_MASK;
		ptr |= shifted_data;
		pointer = (void*)ptr;
	}

	void* DataPointer::GetPointer() const
	{
		// Sign extension
		intptr_t ptr = (intptr_t)pointer;
		ptr <<= 16;
		ptr >>= 16;
		return (void*)ptr;
	}

	void DataPointer::SetPointer(void* new_pointer)
	{
		unsigned short data = GetData();
		pointer = new_pointer;
		SetData(data);
	}

}