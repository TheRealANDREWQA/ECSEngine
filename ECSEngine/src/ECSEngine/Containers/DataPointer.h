#pragma once
#include "../Core.h"

namespace ECSEngine {

	namespace containers {

		// Pointer that can have packed data into the high order 2 bytes
		struct ECSENGINE_API DataPointer {
			DataPointer() : pointer(nullptr) {}
			DataPointer(void* pointer) : pointer(pointer) {}

			DataPointer(const DataPointer& other) = default;
			DataPointer& operator = (const DataPointer& other) = default;

			// Will prevent underflow; returns the new value
			unsigned short DecrementData(unsigned short count);

			// Will prevent overflow; returns the new value
			unsigned short IncrementData(unsigned short count);

			unsigned short GetData() const;

			void SetData(unsigned short data);

			// Sign extend the 47th bit then return it
			void* GetPointer() const;

			void* pointer;
		};

	}

}