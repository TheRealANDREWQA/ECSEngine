#pragma once
#include "../Core.h"
#include "BasicTypes.h"

namespace ECSEngine {

	ECS_INLINE bool IsPowerOfTwo(int x) {
		return (x & (x - 1)) == 0;
	}

	// Determines how many slots are needed to hold the given count with the chunk size
	ECS_INLINE size_t SlotsFor(size_t count, size_t chunk_size) {
		return count / chunk_size + ((count % chunk_size) != 0);
	}

	template<typename Integer>
	ECS_INLINE Integer IsolateLowBits(Integer integer, int bit_count) {
		return integer & ((1 << bit_count) - 1);
	}

	// Combines first and second as follows
		// The low bits of first are keept the same and the low bits of second are placed after those first bits
	template<typename Integer>
	ECS_INLINE Integer BlendBits(Integer first, Integer second, int first_bit_count, int second_bit_count) {
		return IsolateLowBits(first, first_bit_count) | (IsolateLowBits(second, second_bit_count) << first_bit_count);
	}

	// Retrieves the bits from the a blended integer
	template<typename Integer>
	ECS_INLINE void RetrieveBlendedBits(Integer integer, int first_bit_count, int second_bit_count, Integer& first, Integer& second) {
		first = IsolateLowBits(integer, first_bit_count);
		second = integer >> first_bit_count;
		second = IsolateLowBits(second, second_bit_count);
	}

	template<typename T>
	ECS_INLINE T IndexTexture(const T* texture_data, size_t row, size_t column, size_t width) {
		return texture_data[row * width + column];
	}

	// This version takes the row byte size instead of the width
	template<typename T>
	ECS_INLINE T IndexTextureEx(const T* texture_data, size_t row, size_t column, size_t row_byte_size) {
		return *(T*)OffsetPointer(texture_data, row * row_byte_size + column * sizeof(T));
	}

	// The functor receives as parameter a T or T&/const T&
	// Return true inside the functor if you want to exit from the loop.
	// Returns the true when an early exit was done, else false
	template<bool early_exit = false, typename Functor, typename T, size_t size>
	ECS_INLINE bool ForEach(const T(&static_array)[size], Functor&& functor) {
		for (size_t index = 0; index < size; index++) {
			if constexpr (early_exit) {
				if (functor(static_array[index])) {
					return true;
				}
			}
			else {
				functor(static_array[index]);
			}
		}
		return false;
	}

	// Returns true if the element is found in the static array, else false
	template<typename T, size_t size>
	ECS_INLINE bool ExistsStaticArray(T element, const T(&static_array)[size]) {
		for (size_t index = 0; index < size; index++) {
			if (static_array[index] == element) {
				return true;
			}
		}
		return false;
	}

	// Returns true if the element is found in the static array, else false
	template<typename T, size_t size>
	ECS_INLINE bool ExistsStaticArray(const T* element, const T(&static_array)[size]) {
		for (size_t index = 0; index < size; index++) {
			if (static_array[index] == *element) {
				return true;
			}
		}
		return false;
	}

	ECS_INLINE bool HasFlag(size_t configuration, size_t flag) {
		return (configuration & flag) != 0;
	}

	// We're not using any macros or variadic templates. Just write up to 5 args
	ECS_INLINE size_t HasFlag(size_t configuration, size_t flag0, size_t flag1) {
		return HasFlag(HasFlag(configuration, flag0), flag1);
	}

	ECS_INLINE size_t HasFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2) {
		return HasFlag(HasFlag(configuration, flag0), flag1, flag2);
	}

	ECS_INLINE size_t HasFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2, size_t flag3) {
		return HasFlag(HasFlag(configuration, flag0), flag1, flag2, flag3);
	}

	ECS_INLINE size_t HasFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2, size_t flag3, size_t flag4) {
		return HasFlag(HasFlag(configuration, flag0), flag1, flag2, flag3, flag4);
	}

	ECS_INLINE bool HasFlagAtomic(const std::atomic<size_t>& configuration, size_t flag) {
		return (configuration.load(std::memory_order_relaxed) & flag) != 0;
	}

	ECS_INLINE size_t ClearFlag(size_t configuration, size_t flag) {
		return configuration & (~flag);
	}

	// We're not using any macros or variadic templates. Just write up to 5 args
	ECS_INLINE size_t ClearFlag(size_t configuration, size_t flag0, size_t flag1) {
		return ClearFlag(ClearFlag(configuration, flag0), flag1);
	}

	ECS_INLINE size_t ClearFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2) {
		return ClearFlag(ClearFlag(configuration, flag0), flag1, flag2);
	}

	ECS_INLINE size_t ClearFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2, size_t flag3) {
		return ClearFlag(ClearFlag(configuration, flag0), flag1, flag2, flag3);
	}

	ECS_INLINE size_t ClearFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2, size_t flag3, size_t flag4) {
		return ClearFlag(ClearFlag(configuration, flag0), flag1, flag2, flag3, flag4);
	}

	ECS_INLINE void ClearFlagAtomic(std::atomic<size_t>& configuration, size_t flag) {
		configuration.fetch_and(~flag, std::memory_order_relaxed);
	}

	ECS_INLINE size_t SetFlag(size_t configuration, size_t flag) {
		return configuration | flag;
	}

	// We're not using any macros or variadic templates. Just write up to 5 args
	ECS_INLINE size_t SetFlag(size_t configuration, size_t flag0, size_t flag1) {
		return SetFlag(SetFlag(configuration, flag0), flag1);
	}

	ECS_INLINE size_t SetFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2) {
		return SetFlag(SetFlag(configuration, flag0), flag1, flag2);
	}

	ECS_INLINE size_t SetFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2, size_t flag3) {
		return SetFlag(SetFlag(configuration, flag0), flag1, flag2, flag3);
	}

	ECS_INLINE size_t SetFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2, size_t flag3, size_t flag4) {
		return SetFlag(SetFlag(configuration, flag0), flag1, flag2, flag3, flag4);
	}

	ECS_INLINE void SetFlagAtomic(std::atomic<size_t>& configuration, size_t flag) {
		configuration.fetch_or(flag, std::memory_order_relaxed);
	}

	// Returns a / b + ((a % b) != 0)
	// On x86, the divide also produces the remainder, so this is actually a single divide operation
	template<typename T>
	ECS_INLINE T DivideCount(T a, T b) {
		return a / b + ((a % b) != 0);
	}

	template<typename T>
	ECS_INLINE bool IsInRange(T value, T low_bound, T high_bound) {
		return low_bound <= value && value <= high_bound;
	}

	template<typename T>
	ECS_INLINE bool IsInRangeStrict(T value, T low_bound, T high_bound) {
		return low_bound < value&& value < high_bound;
	}

	ECS_INLINE bool FloatCompare(float a, float b, float epsilon = 0.00001f) {
		return fabsf(a - b) < epsilon;
	}

	// Returns the index of the first most significant bit set, -1 if no bit is set
		// (it is like a reverse search inside the bits)
	ECS_INLINE unsigned int FirstMSB64(size_t number) {
		unsigned long value = 0;
		return _BitScanReverse64(&value, number) == 0 ? -1 : value;
	}

	// Returns the index of the first most significant bit set, -1 if no bit is set
	// (it is like a reverse search inside the bits)
	ECS_INLINE unsigned int FirstMSB(unsigned int number) {
		unsigned long value = 0;
		return _BitScanReverse(&value, number) == 0 ? -1 : value;
	}

	// Returns the index of the first least significant bit set, -1 if no bit is set
	// (it is like a forward search inside bits)
	ECS_INLINE unsigned int FirstLSB64(size_t number) {
		unsigned long value = 0;
		return _BitScanForward64(&value, number) == 0 ? -1 : value;
	}

	// Returns the index of the first least significant bit set, -1 if no bit is set
	// (it is like a forward search inside bits)
	ECS_INLINE unsigned int FirstLSB(unsigned int number) {
		unsigned long value = 0;
		return _BitScanForward(&value, number) == 0 ? -1 : value;
	}

	// Returns a pair of { value, exponent } which represents the actual value which is greater
	// and the exponent of the base 2 that gives you that number. Example { 16, 4 }
	ECS_INLINE ulong2 PowerOfTwoGreaterEx(size_t number) {
		// Use bitscan to quickly find this out
		// Example 00011010 -> 00100000

		unsigned int index = FirstMSB(number);
		// This works out even when index is -1 (that is number is 0, index + 1 will be 0 so the returned value will be 1)
		return { (size_t)1 << (index + 1), (size_t)index + 1 };
	}

	ECS_INLINE size_t PowerOfTwoGreater(size_t number) {
		return PowerOfTwoGreaterEx(number).x;
	}

	// pointers should be aligned preferably to 32 bytes at least
	ECSENGINE_API void avx2_copy(void* destination, const void* source, size_t bytes);

	ECS_INLINE size_t GetSimdCount(size_t count, size_t vector_size) {
		return count & (-vector_size);
	}

	constexpr ECS_INLINE float CalculateFloatPrecisionPower(size_t precision) {
		float value = 1.0f;
		for (size_t index = 0; index < precision; index++) {
			value *= 10.0f;
		}
		return value;
	}

	constexpr ECS_INLINE double CalculateDoublePrecisionPower(size_t precision) {
		double value = 1.0;
		for (size_t index = 0; index < precision; index++) {
			value *= 10.0;
		}
		return value;
	}

	// If the pointer is null, it will commit the message
	ECS_INLINE void SetErrorMessage(CapacityStream<char>* error_message, Stream<char> message) {
		if (error_message != nullptr) {
			error_message->AddStreamAssert(message);
		}
	}

	// Uses a fast SIMD compare, in this way you don't need to rely on the
	// compiler to generate for you the SIMD search. Returns -1 if it doesn't
	// find the value. Only types of 1, 2, 4 or 8 bytes are accepted
	ECSENGINE_API size_t SearchBytes(const void* data, size_t element_count, size_t value_to_search, size_t byte_size);

	// Uses a fast SIMD compare, in this way you don't need to rely on the
	// compiler to generate for you the SIMD search. Returns -1 if it doesn't
	// find the value. Only types of 1, 2, 4 or 8 bytes are accepted
	ECSENGINE_API size_t SearchBytesReversed(const void* data, size_t element_count, size_t value_to_search, size_t byte_size);

	// Uses a more generalized approach to comparing. It will use the fast SIMD compare for sizes of 1, 2, 4 and 8
	// but for all the others is going to fall back on memcmp. Returns -1 if it doesn't find the value
	ECSENGINE_API size_t SearchBytesEx(const void* data, size_t element_count, const void* value_to_search, size_t byte_size);

	// Uses a more generalized approach to comparing. It will use the fast SIMD compare for sizes of 1, 2, 4 and 8
	// but for all the others is going to fall back on memcmp. Returns -1 if it doesn't find the value
	ECSENGINE_API size_t SearchBytesExReversed(const void* data, size_t element_count, const void* value_to_search, size_t byte_size);

	template<typename Allocator>
	ECS_INLINE void* Copy(Allocator* allocator, const void* data, size_t data_size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO) {
		void* allocation = allocator->Allocate(data_size, alignment, debug_info);
		memcpy(allocation, data, data_size);
		return allocation;
	}

	ECS_INLINE Stream<void> Copy(AllocatorPolymorphic allocator, Stream<void> data, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO) {
		void* allocation = Allocate(allocator, data.size, alignment, debug_info);
		memcpy(allocation, data.buffer, data.size);
		return { allocation, data.size };
	}

	ECS_INLINE void* Copy(AllocatorPolymorphic allocator, const void* data, size_t data_size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO) {
		return Copy(allocator, { data, data_size }, alignment, debug_info).buffer;
	}

	// If data size is 0, it will return the data back
	ECS_INLINE void* CopyNonZero(AllocatorPolymorphic allocator, const void* data, size_t data_size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (data_size > 0) {
			return Copy(allocator, data, data_size, alignment, debug_info);
		}
		return (void*)data;
	}

	// If data size is 0, it will return the data back
	ECS_INLINE Stream<void> CopyNonZero(AllocatorPolymorphic allocator, Stream<void> data, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (data.size > 0) {
			return Copy(allocator, data, alignment, debug_info);
		}
		return data;
	}

	// If data size is zero, it will return data, else it will make a copy and return that instead
	template<typename Allocator>
	void* CopyNonZero(Allocator* allocator, const void* data, size_t data_size, size_t alignment = 8, DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (data_size > 0) {
			void* allocation = allocator->Allocate(data_size, alignment, debug_info);
			memcpy(allocation, data, data_size);
			return allocation;
		}
		return (void*)data;
	}

	// The functor receives as parameters (const Input* input_values, Output* output_values, size_t element_count)
	// And must return how many elements to write (this value is ignored when a full iteration is in place, it is
	// needed only when there is a remainder - this value must be expressed in elements of output type)
	template<typename InputStreamType, typename OutputStreamType, typename Functor>
	void ApplySIMD(InputStreamType input, OutputStreamType output, size_t simd_elements, size_t output_count_per_iteration, Functor&& functor) {
		size_t simd_count = GetSimdCount(input.size, simd_elements);
		size_t output_element_index = 0;
		for (size_t index = 0; index < simd_count; index += simd_elements) {
			// No need to marshal here
			functor(input.buffer + index, output.buffer + output_element_index, simd_elements);
			output_element_index += output_count_per_iteration;
		}

		if (simd_count < input.size) {
			// We need to perform another iteration here but with redirection
			size_t temporary_memory[ECS_SIMD_SIZE_T_SIZE];
			size_t write_count = functor(input.buffer + simd_count, (decltype(output.buffer))temporary_memory, input.size - simd_count);
			memcpy(output.buffer + output_element_index, temporary_memory, output.MemoryOf(1) * write_count);
		}
	}

	// Idiot C++ thingy, char and int8_t does not equal to the same according to the compiler, only signed char
	// and int8_t; So make all the signed version separately
	template<typename Integer>
	ECS_INLINE void IntegerRange(Integer& min, Integer& max) {
		if constexpr (std::is_same_v<Integer, char>) {
			min = CHAR_MIN;
			max = CHAR_MAX;
		}
		else if constexpr (std::is_same_v<Integer, short>) {
			min = SHORT_MIN;
			max = SHORT_MAX;
		}
		else if constexpr (std::is_same_v<Integer, int>) {
			min = INT_MIN;
			max = INT_MAX;
		}
		else if constexpr (std::is_same_v<Integer, long long>) {
			min = LLONG_MIN;
			max = LLONG_MAX;
		}
		else if constexpr (std::is_same_v<Integer, int8_t>) {
			min = CHAR_MIN;
			max = CHAR_MAX;
		}
		else if constexpr (std::is_same_v<Integer, uint8_t>) {
			min = 0;
			max = UCHAR_MAX;
		}
		else if constexpr (std::is_same_v<Integer, int16_t>) {
			min = SHORT_MIN;
			max = SHORT_MAX;
		}
		else if constexpr (std::is_same_v<Integer, uint16_t>) {
			min = 0;
			max = USHORT_MAX;
		}
		else if constexpr (std::is_same_v<Integer, int32_t>) {
			min = INT_MIN;
			max = INT_MAX;
		}
		else if constexpr (std::is_same_v<Integer, uint32_t>) {
			min = 0;
			max = UINT_MAX;
		}
		else if constexpr (std::is_same_v<Integer, int64_t>) {
			min = LLONG_MIN;
			max = LLONG_MAX;
		}
		else if constexpr (std::is_same_v<Integer, uint64_t>) {
			min = 0;
			max = ULLONG_MAX;
		}
	}

}