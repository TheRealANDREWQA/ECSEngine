#pragma once
#include "../Core.h"
#include "../Allocators/AllocatorPolymorphic.h"

namespace ECSEngine {

	template<typename FirstPointer, typename... Pointers>
	size_t SoACalculateSize(size_t count, FirstPointer** first_parameter, Pointers... parameters) {
		size_t byte_size = sizeof(FirstPointer) * count;

		if constexpr (sizeof...(Pointers) > 0) {
			return byte_size + SoACalculateSize(count, parameters...);
		}
		return byte_size;
	}

	template<typename FirstPointer, typename... Pointers>
	void SoAInitializeFromBuffer(size_t count, uintptr_t& ptr, FirstPointer** first_parameter, Pointers... parameters) {
		*first_parameter = (FirstPointer*)ptr;
		ptr += count * sizeof(FirstPointer);

		if constexpr (sizeof...(Pointers) > 0) {
			SoAInitializeFromBuffer(count, ptr, parameters...);
		}
	}

	// Pointers needs to be the addresses of the pointers you want to initialize
	template<typename FirstPointer, typename... Pointers>
	void SoAInitialize(AllocatorPolymorphic allocator, size_t count, FirstPointer** first_pointer, Pointers... pointers) {
		void* allocation = nullptr;
		if (count > 0) {
			size_t allocation_size = SoACalculateSize(count, first_pointer, pointers...);
			allocation = Allocate(allocator, allocation_size);
		}

		uintptr_t ptr = (uintptr_t)allocation;
		SoAInitializeFromBuffer(count, ptr, first_pointer, pointers...);
	}

	template<typename CountSize, typename FirstPointer, typename... Pointers>
	void SoARemoveSwapBack(CountSize& count, size_t index, FirstPointer first_pointer, Pointers... pointers) {
		first_pointer[index] = first_pointer[count - 1];
		
		if constexpr (sizeof...(Pointers) > 0) {
			SoARemoveSwapBack(count, index, pointers...);
		}
		else {
			count--;
		}
	}

	namespace internal {

		template<typename FirstPointer, typename... Pointers>
		void SoACopyImpl(uintptr_t ptr, size_t offset, size_t copy_count, size_t capacity, FirstPointer** first_pointer, Pointers... pointers) {
			FirstPointer* new_pointer = (FirstPointer*)ptr;
			memcpy(new_pointer, (*first_pointer) + offset, sizeof(FirstPointer) * copy_count);
			*first_pointer = new_pointer;

			ptr += sizeof(FirstPointer) * capacity;

			if constexpr (sizeof...(Pointers) > 0) {
				SoACopyImpl(ptr, offset, copy_count, capacity, pointers...);
			}
		}

	}

	// It functions the same as the other offset, but instead of copying from the very beginning of the pointer,
	// It copies from a specified offset
	template<typename FirstPointer, typename... Pointers>
	void SoACopyWithOffset(AllocatorPolymorphic allocator, size_t offset, size_t size, size_t capacity, FirstPointer** first_pointer, Pointers... pointers) {
		if (capacity > 0) {
			size_t allocation_size = SoACalculateSize(capacity, first_pointer, pointers...);

			void* allocation = Allocate(allocator, allocation_size);
			uintptr_t ptr = (uintptr_t)allocation;

			internal::SoACopyImpl(ptr, offset, size, capacity, first_pointer, pointers...);
		}
		else {
			uintptr_t ptr = 0;
			SoAInitializeFromBuffer(0, ptr, first_pointer, pointers...);
		}
	}

	// It will overwrite the given pointers. These pointers need to be copies of the fixed structure
	// You can specify a different capacity from the size to be copied
	template<typename FirstPointer, typename... Pointers>
	void SoACopy(AllocatorPolymorphic allocator, size_t size, size_t capacity, FirstPointer** first_pointer, Pointers... pointers) {
		SoACopyWithOffset(allocator, 0, size, capacity, first_pointer, pointers...);
	}

	// It will overwrite the given pointers. These pointers need to point to data that needs to be copied
	// The current first pointer needs to be the first pointer of the SoA
	template<typename FirstPointer, typename... Pointers>
	void SoACopyReallocate(AllocatorPolymorphic allocator, size_t size, size_t capacity, FirstPointer** first_pointer, Pointers... pointers) {
		if (capacity > 0) {
			size_t allocation_size = SoACalculateSize(capacity, first_pointer, pointers...);
			
			// Unfortunately, we cannot use Reallocate because of the aliasing reason
			// Since the buffers are coalesced, we would need to write the pointers in reverse
			// Order using memmove. We could probably write that, by inversing the logic
			// Inside SoACopyImpl, but it is probably not worth the effort. At the moment, stick
			// with Allocate/Deallocate pair instead of Reallocate
			void* allocation = nullptr;
			void* initial_pointer = *first_pointer;

			allocation = Allocate(allocator, allocation_size);
			uintptr_t ptr = (uintptr_t)allocation;

			internal::SoACopyImpl(ptr, size, capacity, first_pointer, pointers...);
			if (initial_pointer != nullptr) {
				Deallocate(allocator, initial_pointer);
			}
		}
		else {
			if (*first_pointer != nullptr) {
				Deallocate(allocator, *first_pointer);
			}
		}
	}

	// It writes the source SoA data into the destination SoA data using specified source and destination offsets.
	// This function makes it easier to perform this offseting than doing it manually, which is more error prone and laborious
	template<typename FirstPointer, typename... Pointers>
	void SoACopyDataOnlyWithOffset(size_t source_offset, size_t destination_offset, size_t size, FirstPointer pointer_to_write, const FirstPointer pointer_to_read, Pointers... pointers) {
		static_assert(sizeof...(Pointers) % 2 == 0);

		size_t copy_size = sizeof(*pointer_to_write) * size;
		memcpy(pointer_to_write + destination_offset, pointer_to_read + source_offset, copy_size);

		if constexpr (sizeof...(Pointers) > 0) {
			SoACopyDataOnlyWithOffset(source_offset, destination_offset, size, pointers...);
		}
	}
	
	template<typename FirstPointer, typename... Pointers>
	void SoACopyDataOnly(size_t size, FirstPointer pointer_to_write, const FirstPointer pointer_to_read, Pointers... pointers) {
		SoACopyDataOnlyWithOffset(0, 0, size, pointer_to_write, pointer_to_read, pointers...);
	}

	template<typename FirstPointer, typename... Pointers>
	void SoAResize(AllocatorPolymorphic allocator, size_t size, size_t new_size, FirstPointer** first_pointer, Pointers... pointers) {
		// We can use the copy function - we cannot use the Reallocate since we have to copy from the last pointer
		// to the first one if we want to do that, and without some weird template magic (that I do not know yet), that cannot
		// be done and is not worth wasting time on that
		size_t copy_size = min(size, new_size);
		void* previous_allocation = *first_pointer;
		SoACopy(allocator, copy_size, new_size, first_pointer, pointers...);
		if (size > 0 && first_pointer != nullptr) {
			Deallocate(allocator, previous_allocation);
		}
	}

	template<typename CapacityInt, typename FirstPointer, typename... Pointers>
	void SoAReserve(
		AllocatorPolymorphic allocator,
		CapacityInt size,
		CapacityInt& capacity,
		CapacityInt reserve_count,
		FirstPointer** first_pointer,
		Pointers... pointers
	) {
		if (size + reserve_count > capacity) {
			// Use a small additive factor to escape from low values
			capacity = (CapacityInt)((float)capacity * 1.5f + 4);
			SoAResize(allocator, size, capacity, first_pointer, pointers...);
		}
	}

	template<typename CapacityInt, typename FirstPointer, typename... Pointers>
	void SoAResizeIfFull(AllocatorPolymorphic allocator, CapacityInt size, CapacityInt& capacity, FirstPointer** first_pointer, Pointers... pointers) {
		SoAReserve(allocator, size, capacity, (CapacityInt)1, first_pointer, pointers...);
	}

	template<typename FirstPointer, typename... Pointers>
	void SoADisplaceElements(size_t size, size_t offset, int64_t displacement, FirstPointer first_pointer, Pointers... pointers) {
		size_t count = size - offset;
		memmove(first_pointer + (int64_t)offset + displacement, first_pointer + offset, count * sizeof(*first_pointer));

		if constexpr (sizeof...(Pointers) > 0) {
			SoADisplaceElements(size, offset, displacement, pointers...);
		}
	}

	template<typename FirstPointerType, typename... Pointers>
	bool SoACompare(size_t size, FirstPointerType left_first_pointer, FirstPointerType right_first_pointer, Pointers... pointers) {
		if (memcmp(left_first_pointer, right_first_pointer, sizeof(*left_first_pointer) * size) != 0) {
			return false;
		}

		if constexpr (sizeof...(Pointers) > 0) {
			return SoACompare(size, pointers...);
		}
		return true;
	}

}