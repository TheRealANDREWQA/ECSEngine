#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"

namespace ECSEngine {

	// a - b
	ECS_INLINE size_t PointerDifference(const void* a, const void* b) {
		return (uintptr_t)a - (uintptr_t)b;
	}

	ECS_INLINE void* OffsetPointer(const void* pointer, int64_t offset) {
		return (void*)((int64_t)pointer + offset);
	}

	ECS_INLINE void* OffsetPointer(Stream<void> pointer) {
		return OffsetPointer(pointer.buffer, pointer.size);
	}

	ECS_INLINE void* OffsetPointer(CapacityStream<void> pointer) {
		return OffsetPointer(pointer.buffer, pointer.size);
	}

	// The alignment needs to be a power of two
	ECS_INLINE uintptr_t AlignPointer(uintptr_t pointer, size_t alignment) {
		size_t mask = alignment - 1;
		return (pointer + mask) & ~mask;
	}

	ECS_INLINE void* AlignPointer(const void* pointer, size_t alignment) {
		return (void*)AlignPointer((uintptr_t)pointer, alignment);
	}

	// It will remap the pointer from the first base into the second base
	ECS_INLINE void* RemapPointer(const void* first_base, const void* second_base, const void* pointer) {
		return OffsetPointer(second_base, PointerDifference(pointer, first_base));
	}

	// Used to copy data into a pointer passed by its address
	struct CopyPointer {
		void** destination;
		void* data;
		size_t data_size;
	};

	ECS_INLINE void CopyPointers(Stream<CopyPointer> copy_data) {
		for (size_t index = 0; index < copy_data.size; index++) {
			memcpy(*copy_data[index].destination, copy_data[index].data, copy_data[index].data_size);
		}
	}

	// Given a fresh buffer, it will set the destinations accordingly
	ECS_INLINE void CopyPointersFromBuffer(Stream<CopyPointer> copy_data, void* data) {
		uintptr_t ptr = (uintptr_t)data;
		for (size_t index = 0; index < copy_data.size; index++) {
			*copy_data[index].destination = (void*)ptr;
			memcpy((void*)ptr, copy_data[index].data, copy_data[index].data_size);
			ptr += copy_data[index].data_size;
		}
	}

	// Returns true if the pointer >= base && pointer < base + size
	ECS_INLINE bool IsPointerRange(const void* base, size_t size, const void* pointer) {
		return pointer >= base && PointerDifference(pointer, base) < size;
	}

	ECS_INLINE bool AreAliasing(Stream<void> first, Stream<void> second) {
		auto test = [](Stream<void> first, Stream<void> second) {
			return first.buffer >= second.buffer && (first.buffer < OffsetPointer(second.buffer, second.size));
		};
		return test(first, second) || test(second, first);
	}

	ECS_INLINE void* RemapPointerIfInRange(const void* base, size_t size, const void* new_base, const void* pointer) {
		if (IsPointerRange(base, size, pointer)) {
			return RemapPointer(base, new_base, pointer);
		}
		return (void*)pointer;
	}

	/* Supports alignments up to 256 bytes */
	ECS_INLINE uintptr_t AlignPointerStack(uintptr_t pointer, size_t alignment) {
		uintptr_t first_aligned_pointer = AlignPointer(pointer, alignment);
		return first_aligned_pointer + alignment * ((first_aligned_pointer - pointer) == 0);
	}

	// Extends the 47th bit into the 48-63 range
	ECS_INLINE void* SignExtendPointer(const void* pointer) {
		intptr_t ptr = (intptr_t)pointer;
		ptr <<= 16;
		ptr >>= 16;
		return (void*)ptr;
	}

	// Shifts the pointer 3 positions to the right in order to provide significant digits for hashing functions
	// like power of two that use the lower bits in order to hash the element inside the table.
	// It will clip to only 24 bits - 3 bytes - that's the precision the hash table work with
	ECS_INLINE unsigned int PointerHash(const void* ptr) {
		return (unsigned int)(((uintptr_t)ptr >> 3) & 0x0000000000FFFFFF);
	}

}