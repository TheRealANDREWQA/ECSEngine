#include "ecspch.h"
#include "Utilities.h"
#include "PointerUtilities.h"
#include "StringUtilities.h"

// NOTE: This file has optimization overrides for the Release build such that it offers very good performance
// In that configuration as well, since the SearchBytes function is quite needed.

namespace ECSEngine {

	void swap(void* a, void* b, void* temporary_storage, size_t copy_size) {
		memcpy(temporary_storage, a, copy_size);
		memcpy(a, b, copy_size);
		memcpy(b, temporary_storage, copy_size);
	}

	template<typename Functor>
	static void AddressFlagBit(void* value, unsigned char bit_index, Functor&& functor) {
		if (bit_index < sizeof(unsigned char) * 8) {
			functor((unsigned char*)value);
		}
		else if (bit_index < sizeof(unsigned short) * 8) {
			functor((unsigned short*)value);
		}
		else if (bit_index < sizeof(unsigned int) * 8) {
			functor((unsigned int*)value);
		}
		else if (bit_index < sizeof(size_t) * 8) {
			functor((size_t*)value);
		}
		else {
			ECS_ASSERT(false);
		}
	}

	void SetBitFlag(void* value, unsigned char bit_index) {
		AddressFlagBit(value, bit_index, [bit_index](auto* integral_value) {
			using IntegralType = std::remove_reference_t<decltype(*integral_value)>;
			*integral_value = *integral_value | ((IntegralType)1 << (IntegralType)bit_index);
		});
	}

	void ClearBitFlag(void* value, unsigned char bit_index) {
		AddressFlagBit(value, bit_index, [bit_index](auto* integral_value) {
			using IntegralType = std::remove_reference_t<decltype(*integral_value)>;
			*integral_value = *integral_value & ~((IntegralType)1 << (IntegralType)bit_index);
		});
	}

	bool HasBitFlag(const void* value, unsigned char bit_index) {
		bool has = false;
		AddressFlagBit((void*)value, bit_index, [bit_index, &has](auto* integral_value) {
			using IntegralType = std::remove_reference_t<decltype(*integral_value)>;
			has = (*integral_value & ((IntegralType)1 << (IntegralType)bit_index)) != 0;
		});
		return has;
	}

	void FlipBitFlag(void* value, unsigned char bit_index) {
		AddressFlagBit(value, bit_index, [bit_index](auto* integral_value) {
			using IntegralType = std::remove_reference_t<decltype(*integral_value)>;
			*integral_value = *integral_value ^ ((IntegralType)1 << (IntegralType)bit_index);
		});
	}

	// AVX2 accelerated memcpy, works best for workloads of at least 1KB
	template<bool unaligned>
	static void avx2_copy_32multiple(void* destination, const void* source, size_t bytes) {
		// destination, source -> alignment 32 bytes 
		// bytes -> multiple 32

		auto* destination_vector = (__m256i*)destination;
		const auto* source_vector = (__m256i*)source;
		size_t iterations = bytes / sizeof(__m256i);

		for (; iterations > 0; iterations--, destination_vector++, source_vector++) {
			if constexpr (!unaligned) {
				const __m256i temp = _mm256_load_si256(source_vector);
				_mm256_store_si256(destination_vector, temp);
			}
			else {
				const __m256i temp = _mm256_loadu_si256(source_vector);
				_mm256_storeu_si256(destination_vector, temp);
			}
		}
	}

	// AVX2 accelerated memcpy, works best for workloads of at least 1KB
	template<bool unaligned>
	static void avx2_copy_128multiple(void* destination, const void* source, size_t bytes) {
		// destination, source -> alignment 128 bytes
		// bytes -> multiple of 128

		auto* destination_vector = (__m256i*)destination;
		const auto* source_vector = (__m256i*)source;
		size_t iterations = bytes / sizeof(__m256i);

		for (; iterations > 0; iterations -= 4, destination_vector += 4, source_vector += 4) {
			if constexpr (!unaligned) {
				_mm256_store_si256(destination_vector, _mm256_load_si256(source_vector));
				_mm256_store_si256(destination_vector + 1, _mm256_load_si256(source_vector + 1));
				_mm256_store_si256(destination_vector + 2, _mm256_load_si256(source_vector + 2));
				_mm256_store_si256(destination_vector + 3, _mm256_load_si256(source_vector + 3));
			}
			else {
				_mm256_storeu_si256(destination_vector, _mm256_loadu_si256(source_vector));
				_mm256_storeu_si256(destination_vector + 1, _mm256_loadu_si256(source_vector + 1));
				_mm256_storeu_si256(destination_vector + 2, _mm256_loadu_si256(source_vector + 2));
				_mm256_storeu_si256(destination_vector + 3, _mm256_loadu_si256(source_vector + 3));
			}
		}
	}

	// --------------------------------------------------------------------------------------------------

	// pointers should be aligned preferably to 128 bytes
	void avx2_copy(void* destination, const void* source, size_t bytes) {
		// 64 B
		if (bytes < 64) {
			if (((uintptr_t)destination & 32) == 0 && ((uintptr_t)source & 32) == 0) {
				// if multiple of 32
				size_t rounded_bytes_32 = bytes & (-32);
				if (rounded_bytes_32 == bytes)
					avx2_copy_32multiple<false>(destination, source, bytes);
				else {
					memcpy(destination, source, bytes - rounded_bytes_32);
					avx2_copy_32multiple<true>(destination, source, rounded_bytes_32);
				}
			}
			else {
				// if multiple of 32
				size_t rounded_bytes_32 = bytes & (-32);
				if (rounded_bytes_32 == bytes)
					avx2_copy_32multiple<true>(destination, source, bytes);
				else {
					memcpy(destination, source, bytes - rounded_bytes_32);
					avx2_copy_32multiple<true>(destination, source, rounded_bytes_32);
				}
			}
		}
		// 1 MB
		else if (bytes < 1'048'576) {
			if (((uintptr_t)destination & 128) == 0 && ((uintptr_t)source & 128) == 0) {
				// if multiple of 128
				size_t rounded_bytes_128 = bytes & (-128);
				if (rounded_bytes_128 == bytes)
					avx2_copy_128multiple<false>(destination, source, bytes);
				else {
					memcpy(destination, source, bytes - rounded_bytes_128);
					avx2_copy_128multiple<true>(destination, source, bytes);
				}
			}
			else {
				// if multiple of 128
				size_t rounded_bytes_128 = bytes & (-128);
				if (rounded_bytes_128 == bytes)
					avx2_copy_128multiple<true>(destination, source, bytes);
				else {
					memcpy(destination, source, bytes - rounded_bytes_128);
					avx2_copy_128multiple<true>(destination, source, bytes);
				}
			}
		}
		// over 1 MB
		else {
			memcpy(destination, source, bytes);
		}
	}

	// --------------------------------------------------------------------------------------------------

	template<bool reverse, typename IntegerType>
	size_t SearchBytesImpl(const void* data, size_t element_count, IntegerType value_to_search) {
		auto loop = [value_to_search](const void* data, size_t element_count, auto values, auto simd_value_to_search) {
			constexpr size_t byte_size = sizeof(IntegerType);
			size_t simd_width = simd_value_to_search.size();
			size_t simd_count = GetSimdCount(element_count, simd_width);
			for (size_t index = 0; index < simd_count; index += simd_width) {
				const void* ptr = OffsetPointer(data, byte_size * index);
				if constexpr (reverse) {
					ptr = OffsetPointer(data, byte_size * (element_count - simd_width - index));
				}

				values.load(ptr);

				auto compare = values == simd_value_to_search;
				if (horizontal_or(compare)) {
					unsigned int bits = to_bits(compare);
					unsigned int first = 0;
					if constexpr (reverse) {
						first = FirstMSB(bits);
					}
					else {
						first = FirstLSB(bits);
					}

					if (first != -1) {
						if constexpr (reverse) {
							return element_count - simd_width - index + first;
						}
						else {
							// We have a match
							return index + first;
						}
					}
				}
			}

			// The last elements
			size_t last_element_count = element_count - simd_count;
			const void* ptr = OffsetPointer(data, byte_size * simd_count);
			
			const void* iteration_ptr = ptr;
			if constexpr (reverse) {
				ptr = data;
				iteration_ptr = OffsetPointer(data, byte_size * (last_element_count - 1));
			}

			for (size_t index = 0; index < last_element_count; index++) {
				if constexpr (std::is_same_v<decltype(values), Vec32uc>) {
					if (*(unsigned char*)iteration_ptr == value_to_search) {
						if constexpr (reverse) {
							return last_element_count - 1 - index;
						}
						else {
							return simd_count + index;
						}
					}
				}
				else if constexpr (std::is_same_v<decltype(values), Vec16us>) {
					if (*(unsigned short*)iteration_ptr == value_to_search) {
						if constexpr (reverse) {
							return last_element_count - 1 - index;
						}
						else {
							return simd_count + index;
						}
					}
				}
				else if constexpr (std::is_same_v<decltype(values), Vec8ui>) {
					if (*(unsigned int*)iteration_ptr == value_to_search) {
						if constexpr (reverse) {
							return last_element_count - 1 - index;
						}
						else {
							return simd_count + index;
						}
					}
				}
				else if constexpr (std::is_same_v<decltype(values), Vec4uq>) {
					if (*(size_t*)iteration_ptr == value_to_search) {
						if constexpr (reverse) {
							return last_element_count - 1 - index;
						}
						else {
							return simd_count + index;
						}
					}
				}
				else {
					static_assert(false, "Invalid vector type");
				}

				if constexpr (reverse) {
					iteration_ptr = OffsetPointer(iteration_ptr, -(int64_t)byte_size);
				}
				else {
					iteration_ptr = OffsetPointer(iteration_ptr, byte_size);
				}
			}

			// This SIMD variant that uses load partial seems to be quite slow for few entries, disable it for the time being
			//values.load_partial(last_element_count, ptr);
			//auto compare = values == simd_value_to_search;
			//if (horizontal_or(compare)) {
			//	unsigned int bits = to_bits(compare);
			//	unsigned int first = 0;
			//	if constexpr (reverse) {
			//		first = FirstMSB(bits);
			//	}
			//	else {
			//		first = FirstLSB(bits);
			//	}

			//	if (first != -1 && (size_t)first < last_element_count) {
			//		if constexpr (reverse) {
			//			return (size_t)first;
			//		}
			//		else {
			//			return simd_count + first;
			//		}
			//	}
			//	else {
			//		if constexpr (reverse) {
			//			// If reversed, then try again until the bits integer is 0 or we get a value that is in bounds
			//			bits &= ~(1 << first);
			//			while (bits != 0) {
			//				first = FirstMSB(bits);
			//				if (first != -1 && (size_t)first < last_element_count) {
			//					return (size_t)first;
			//				}
			//				bits &= ~(1 << first);
			//			}
			//		}
			//	}
			//}
			return (size_t)-1;
		};

		if (element_count == 0) {
			return -1;
		}

		if constexpr (sizeof(IntegerType) == 1) {
			// If there are few elements, use a normal loop to avoid the cost of initializing a SIMD register.
			Vec32uc values;
			if (element_count < values.size()) {
				const unsigned char* typed_data = (unsigned char*)data;
				for (size_t index = 0; index < element_count; index++) {
					size_t lookup_index = index;
					if constexpr (reverse) {
						lookup_index = element_count - index - 1;
					}
					if (typed_data[lookup_index] == value_to_search) {
						return lookup_index;
					}
				}
				return -1;
			}

			Vec32uc simd_value_to_search((unsigned char)value_to_search);

			return loop(data, element_count, values, simd_value_to_search);
		}
		else if constexpr (sizeof(IntegerType) == 2) {
			Vec16us values;
			if (element_count < values.size()) {
				const unsigned short* typed_data = (unsigned short*)data;
				for (size_t index = 0; index < element_count; index++) {
					size_t lookup_index = index;
					if constexpr (reverse) {
						lookup_index = element_count - index - 1;
					}
					if (typed_data[lookup_index] == value_to_search) {
						return lookup_index;
					}
				}
				return -1;
			}

			Vec16us simd_value_to_search((unsigned short)value_to_search);

			return loop(data, element_count, values, simd_value_to_search);
		}
		else if constexpr (sizeof(IntegerType) == 4) {
			Vec8ui values;
			if (element_count < values.size()) {
				const unsigned int* typed_data = (unsigned int*)data;
				for (size_t index = 0; index < element_count; index++) {
					size_t lookup_index = index;
					if constexpr (reverse) {
						lookup_index = element_count - index - 1;
					}
					if (typed_data[lookup_index] == value_to_search) {
						return lookup_index;
					}
				}
				return -1;
			}

			Vec8ui simd_value_to_search((unsigned int)value_to_search);

			return loop(data, element_count, values, simd_value_to_search);
		}
		else if constexpr (sizeof(IntegerType) == 8) {
			Vec4uq values;
			if (element_count < values.size()) {
				const size_t* typed_data = (size_t*)data;
				for (size_t index = 0; index < element_count; index++) {
					size_t lookup_index = index;
					if constexpr (reverse) {
						lookup_index = element_count - index - 1;
					}
					if (typed_data[lookup_index] == value_to_search) {
						return lookup_index;
					}
				}
				return -1;
			}

			Vec4uq simd_value_to_search(value_to_search);

			return loop(data, element_count, values, simd_value_to_search);
		}
		else {
			ECS_ASSERT(false);
		}

		return -1;
	}

	template<typename IntegerType>
	size_t SearchBytes(const void* data, size_t element_count, IntegerType value_to_search) {
		return SearchBytesImpl<false>(data, element_count, value_to_search);
	}

	ECS_TEMPLATE_FUNCTION_4_AFTER(size_t, SearchBytes, unsigned char, unsigned short, unsigned int, size_t, const void*, size_t);
	// Add the signed variants just in case
	ECS_TEMPLATE_FUNCTION_4_AFTER(size_t, SearchBytes, char, short, int, int64_t, const void*, size_t);
	// Add the boolean version as well
	ECS_TEMPLATE_FUNCTION(size_t, SearchBytes, const void*, size_t, bool);

	size_t SearchBytes(const void* data, size_t element_count, size_t value_to_search, size_t byte_size)
	{
		switch (byte_size) {
		case 1:
			return SearchBytes(data, element_count, (unsigned char)value_to_search);
		case 2:
			return SearchBytes(data, element_count, (unsigned short)value_to_search);
		case 4:
			return SearchBytes(data, element_count, (unsigned int)value_to_search);
		case 8:
			return SearchBytes(data, element_count, (size_t)value_to_search);
		}

		ECS_ASSERT(false);
		return -1;
	}

	// --------------------------------------------------------------------------------------------------

	template<typename IntegerType>
	size_t SearchBytesReversed(const void* data, size_t element_count, IntegerType value_to_search) {
		return SearchBytesImpl<true>(data, element_count, value_to_search);
	}

	ECS_TEMPLATE_FUNCTION_4_AFTER(size_t, SearchBytesReversed, unsigned char, unsigned short, unsigned int, size_t, const void*, size_t);
	// Add the signed variants just in case
	ECS_TEMPLATE_FUNCTION_4_AFTER(size_t, SearchBytesReversed, char, short, int, int64_t, const void*, size_t);
	// Add the boolean version as well
	ECS_TEMPLATE_FUNCTION(size_t, SearchBytesReversed, const void*, size_t, bool);

	size_t SearchBytesReversed(const void* data, size_t element_count, size_t value_to_search, size_t byte_size)
	{
		switch (byte_size) {
		case 1:
			return SearchBytesReversed(data, element_count, (unsigned char)value_to_search);
		case 2:
			return SearchBytesReversed(data, element_count, (unsigned short)value_to_search);
		case 4:
			return SearchBytesReversed(data, element_count, (unsigned int)value_to_search);
		case 8:
			return SearchBytesReversed(data, element_count, (size_t)value_to_search);
		}

		ECS_ASSERT(false);
		return -1;
	}

	// --------------------------------------------------------------------------------------------------

	template<bool reversed>
	size_t SearchBytesExImpl(const void* data, size_t element_count, const void* value_to_search, size_t byte_size) {
		if (byte_size == 1) {
			return SearchBytesImpl<reversed>(data, element_count, *(unsigned char*)value_to_search);
		}
		else if (byte_size == 2) {
			return SearchBytesImpl<reversed>(data, element_count, *(unsigned short*)value_to_search);
		}
		else if (byte_size == 4) {
			return SearchBytesImpl<reversed>(data, element_count, *(unsigned int*)value_to_search);
		}
		else if (byte_size == 8) {
			return SearchBytesImpl<reversed>(data, element_count, *(size_t*)value_to_search);
		}

		// Not in the fast case, use FindFirstTokenOffset or FindTokenReverseOffset, since this is what essentially we need to do
		size_t offset = -1;
		if constexpr (reversed) {
			offset = FindTokenReverseOffset(Stream<char>(data, element_count * byte_size), Stream<char>(value_to_search, byte_size));
		}
		else {
			offset = FindFirstTokenOffset(Stream<char>(data, element_count * byte_size), Stream<char>(value_to_search, byte_size)) / byte_size;
		}
		return offset == -1 ? -1 : offset / byte_size;
	}

	size_t SearchBytesEx(const void* data, size_t element_count, const void* value_to_search, size_t byte_size)
	{
		return SearchBytesExImpl<false>(data, element_count, value_to_search, byte_size);
	}

	size_t SearchBytesExReversed(const void* data, size_t element_count, const void* value_to_search, size_t byte_size)
	{
		return SearchBytesExImpl<true>(data, element_count, value_to_search, byte_size);
	}

	// --------------------------------------------------------------------------------------------------

}