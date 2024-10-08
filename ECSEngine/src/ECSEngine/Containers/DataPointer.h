#pragma once
#include "../Core.h"
#include "../Allocators/AllocatorTypes.h"

namespace ECSEngine {

	// Pointer that can have packed data into the high order 2 bytes
	struct ECSENGINE_API DataPointer {
		ECS_INLINE DataPointer() : pointer(nullptr) {}
		ECS_INLINE DataPointer(void* pointer) : pointer(pointer) {
			SetData(0);
		}
		ECS_INLINE DataPointer(void* pointer, unsigned short data) : pointer(pointer) {
			SetData(data);
		}

		DataPointer(const DataPointer& other) = default;
		DataPointer& operator = (const DataPointer& other) = default;
		
		void Deallocate(AllocatorPolymorphic allocator);

		// Will prevent underflow; returns the new value
		unsigned short DecrementData(unsigned short count);

		// Will prevent overflow; returns the new value
		unsigned short IncrementData(unsigned short count);

		unsigned short GetData() const;

		void SetData(unsigned short data);

		// Sign extend the 47th bit then return it
		void* GetPointer() const;

		void SetPointer(void* new_pointer);

		void* pointer;
	};

}