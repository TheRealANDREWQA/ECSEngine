#pragma once
#include "../Core.h"
#include "../Allocators/AllocatorPolymorphic.h"

namespace ECSEngine {

	namespace internal {

		template<typename FirstPointer, typename... Pointers>
		size_t SoACalculateSizeImpl(size_t count, FirstPointer** first_parameter, Pointers... parameters) {
			size_t byte_size = sizeof(FirstPointer) * count;

			if constexpr (sizeof...(Pointers) > 0) {
				return byte_size + SoACalculateSizeImpl(count, parameters...);
			}
			return byte_size;
		}

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
			size_t allocation_size = internal::SoACalculateSizeImpl(count, first_pointer, pointers...);
			allocation = AllocateEx(allocator, allocation_size);
		}

		uintptr_t ptr = (uintptr_t)allocation;
		SoAInitializeFromBuffer(count, ptr, first_pointer, pointers...);
	}

	template<typename CountSize, typename FirstPointer, typename... Pointers>
	void SoARemoveSwapBack(CountSize& count, size_t index, FirstPointer first_pointer, Pointers... pointers) {
		first_pointer[index] = first_pointer[count];
		
		if constexpr (sizeof...(Pointers) > 0) {
			SoARemoveSwapBack(count, index, pointers...);
		}
		else {
			count--;
		}
	}

	namespace internal {

		template<typename FirstPointer, typename... Pointers>
		void SoACopyImpl(uintptr_t ptr, size_t copy_count, size_t capacity, FirstPointer** first_pointer, Pointers... pointers) {
			FirstPointer* new_pointer = (FirstPointer*)ptr;
			memcpy(new_pointer, *first_pointer, sizeof(FirstPointer) * copy_count);
			*first_pointer = new_pointer;

			ptr += sizeof(FirstPointer) * capacity;

			if constexpr (sizeof...(Pointers) > 0) {
				SoACopyImpl(ptr, copy_count, capacity, pointers...);
			}
		}

	}

	// It will overwrite the given pointer. These pointers need to be copies of the fixed structure
	// You can specify a different capacity from the size to be copied
	template<typename FirstPointer, typename... Pointers>
	void SoACopy(AllocatorPolymorphic allocator, size_t size, size_t capacity, FirstPointer** first_pointer, Pointers... pointers) {
		size_t allocation_size = internal::SoACalculateSizeImpl(capacity, first_pointer, pointers...);

		void* allocation = AllocateEx(allocator, allocation_size);
		uintptr_t ptr = (uintptr_t)allocation;

		internal::SoACopyImpl(ptr, size, capacity, first_pointer, pointers...);
	}

	template<typename FirstPointer, typename... Pointers>
	void SoACopyDataOnly(size_t size, FirstPointer pointer_to_write, const FirstPointer pointer_to_read, Pointers... pointers) {
		static_assert(sizeof...(Pointers) % 2 == 0);

		size_t copy_size = sizeof(*pointer_to_write) * size;
		memcpy(pointer_to_write, pointer_to_read, copy_size);

		if constexpr (sizeof...(Pointers) > 0) {
			SoACopyDataOnly(size, pointers...);
		}
	}

	template<typename FirstPointer, typename... Pointers>
	void SoAResize(AllocatorPolymorphic allocator, size_t size, size_t new_size, FirstPointer** first_pointer, Pointers... pointers) {
		// We can use the copy function - we cannot use the ReallocateEx since we have to copy from the last pointer
		// to the first one if we want to do that, and without some weird template magic (that I do not know yet), that cannot
		// be done and is not worth wasting time on that
		size_t copy_size = std::min(size, new_size);
		void* previous_allocation = *first_pointer;
		SoACopy(allocator, copy_size, new_size, first_pointer, pointers...);
		if (size > 0 && first_pointer != nullptr) {
			DeallocateEx(allocator, previous_allocation);
		}
	}

	template<typename FirstPointer, typename... Pointers>
	void SoADisplaceElements(size_t size, size_t offset, int64_t displacement, FirstPointer first_pointer, Pointers... pointers) {
		size_t count = size - offset;
		memmove(first_pointer + (int64_t)offset + displacement, first_pointer + offset, count * sizeof(*first_pointer));

		if constexpr (sizeof...(Pointers) > 0) {
			SoADisplaceElements(size, offset, displacement, pointers...);
		}
	}

}