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

		template<typename FirstPointer, typename... Pointers>
		void SoAInitializeImpl(size_t count, uintptr_t ptr, FirstPointer** first_parameter, Pointers... parameters) {
			*first_parameter = (FirstPointer*)ptr;
			ptr += count * sizeof(FirstPointer);

			if constexpr (sizeof...(Pointers) > 0) {
				SoAInitializeImpl(count, ptr, parameters...);
			}
		}

	}

	// Pointers needs to be the addresses of the pointers you want to initialize
	template<typename... Pointers>
	void SoAInitialize(AllocatorPolymorphic allocator, size_t count, Pointers... pointers) {
		void* allocation = nullptr;
		if (count > 0) {
			size_t allocation_size = internal::SoACalculateSizeImpl(count, pointers...);
			allocation = AllocateEx(allocator, allocation_size);
		}

		uintptr_t ptr = (uintptr_t)allocation;
		internal::SoAInitializeImpl(count, ptr, pointers...);
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
		void SoAResizeImpl(uintptr_t ptr, size_t size, size_t new_size, FirstPointer** first_pointer, Pointers... pointers) {
			FirstPointer* new_pointer = (FirstPointer*)ptr;
			size_t copy_size = size < new_size ? size : new_size;
			memcpy(new_pointer, *first_pointer, sizeof(FirstPointer) * copy_size);
			*first_pointer = new_pointer;

			ptr += sizeof(FirstPointer) * new_size;

			if constexpr (sizeof...(Pointers) > 0) {
				SoAResizeImpl(ptr, size, new_size, pointers...);
			}
		}

	}

	template<typename FirstPointer, typename... Pointers>
	void SoAResize(AllocatorPolymorphic allocator, size_t size, size_t new_size, FirstPointer** first_pointer, Pointers... pointers) {
		void* new_allocation = nullptr;
		if (new_size > 0) {
			size_t allocation_size = internal::SoACalculateSizeImpl(new_size, first_pointer, pointers...);
			new_allocation = AllocateEx(allocator, allocation_size);
		}

		uintptr_t ptr = (uintptr_t)new_allocation;
		internal::SoAResizeImpl(ptr, size, new_size, first_pointer, pointers...);

		if (size > 0 && first_pointer != nullptr) {
			DeallocateEx(allocator, *first_pointer);
		}
	}

}