#pragma once
#include "../Core.h"
#include "BasicTypes.h"
#include "../Containers/Stream.h"
#include "../Containers/BooleanBitField.h"

namespace ECSEngine {

	template<typename U, typename T>
	ECS_INLINE U BitCast(T value) {
		// To please the C++ "Standard Comittee", use memcpy to avoid "Undefined behaviour" of using *(U*)
		U u_value;
		memcpy(&u_value, &value, sizeof(value));
		return u_value;
	}

	ECS_INLINE bool IsPowerOfTwo(size_t x) {
		return (x & (x - 1)) == 0;
	}

	// Determines how many slots are needed to hold the given count with the chunk size
	ECS_INLINE size_t SlotsFor(size_t count, size_t chunk_size) {
		return (count + chunk_size - 1) / chunk_size;
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
	ECS_INLINE void ZeroStructure(T* structure) {
		memset(structure, 0, sizeof(*structure));
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

	// This variant works in such a fashion that it addresses the smallest integral type that covers the bit index
	ECSENGINE_API void SetFlag(void* value, unsigned char bit_index);

	// This variant works in such a fashion that it addresses the smallest integral type that covers the bit index
	ECSENGINE_API void ClearFlag(void* value, unsigned char bit_index);

	// This variant works in such a fashion that it addresses the smallest integral type that covers the bit index
	ECSENGINE_API bool HasFlag(const void* value, unsigned char bit_index);

	// This variant works in such a fashion that it addresses the smallest integral type that covers the bit index
	ECSENGINE_API void FlipFlag(void* value, unsigned char bit_index);

	template<bool strict_compare = false, typename T>
	ECS_INLINE bool IsInRange(T value, T low_bound, T high_bound) {
		if constexpr (strict_compare) {
			return low_bound < value && value < high_bound;
		}
		else {
			return low_bound <= value && value <= high_bound;
		}
	}

	// Returns true if the internal [a0, b0] overlaps the [a1, b1]
	// Internval. You can choose to make this comparison strict or not
	template<bool strict_compare = false, typename T>
	ECS_INLINE bool AreIntervalsOverlapping(T a0, T b0, T a1, T b1) {
		if (IsInRange<strict_compare>(a0, a1, b1)) {
			return true;
		}
		if (IsInRange<strict_compare>(b0, a1, b1)) {
			return true;
		}
		if (IsInRange<strict_compare>(a1, a0, b0)) {
			return true;
		}
		if (IsInRange<strict_compare>(b1, a0, b0)) {
			return true;
		}
		return false;
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

	// Counts the number of bits set for the given integer
	ECS_INLINE unsigned int CountBits(unsigned int value) {
		return __popcnt(value);
	}

	// Counts the number of bits set for the given integer
	ECS_INLINE size_t CountBits(size_t value) {
		return __popcnt64(value);
	}

	// Returns a pair of { value, exponent } which represents the actual value which is greater
	// and the exponent of the base 2 that gives you that number. Example { 16, 4 }
	ECS_INLINE ulong2 PowerOfTwoGreaterEx(size_t number) {
		// Use bitscan to quickly find this out
		// Example 00011010 -> 00100000

		unsigned int index = FirstMSB64(number);
		// This works out even when index is -1 (that is number is 0, index + 1 will be 0 so the returned value will be 1)
		// When it is -1, let the exponent be 0
		return { (size_t)1 << (index + 1), index == -1 ? (size_t)0 : (size_t)index + 1 };
	}

	ECS_INLINE size_t PowerOfTwoGreater(size_t number) {
		return PowerOfTwoGreaterEx(number).x;
	}

	// Returns the byte that needs to be checked to get the value at that bit index
	ECS_INLINE size_t GetByteIndexFromBit(size_t bit_index) {
		return bit_index & (~(size_t)0x07);
	}

	// Returns the bit that needs to be checked to get the value at that bit index
	ECS_INLINE size_t GetBitIndexFromBit(size_t bit_index) {
		return (size_t)1 << (bit_index & (size_t)0x07);
	}

	ECS_INLINE void SetBit(void* bits, size_t bit_index) {
		((unsigned char*)bits)[GetByteIndexFromBit(bit_index)] |= GetBitIndexFromBit(bit_index);
	}

	ECS_INLINE void ClearBit(void* bits, size_t bit_index) {
		((unsigned char*)bits)[GetByteIndexFromBit(bit_index)] &= ~GetBitIndexFromBit(bit_index);
	}

	ECS_INLINE bool GetBit(void* bits, size_t bit_index) {
		return ((const unsigned char*)bits)[GetByteIndexFromBit(bit_index)] & GetBitIndexFromBit(bit_index);
	}

	// pointers should be aligned preferably to 32 bytes at least
	ECSENGINE_API void avx2_copy(void* destination, const void* source, size_t bytes);

	// Vector size needs to be a power of two
	ECS_INLINE size_t GetSimdCount(size_t count, size_t vector_size) {
		return count & (-(int64_t)vector_size);
	}

	template<typename T>
	ECS_INLINE void ZeroOut(T* data) {
		memset(data, 0, sizeof(*data));
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

	template<typename StreamValue>
	ECS_INLINE size_t SearchBytes(Stream<StreamValue> stream, StreamValue value_to_search) {
		static_assert(sizeof(StreamValue) == 1 || sizeof(StreamValue) == 2 || sizeof(StreamValue) == 4 || sizeof(StreamValue) == 8);
		return SearchBytes(stream.buffer, stream.size, (size_t)value_to_search, sizeof(value_to_search));
	}

	// If the data size is different from 0 and the data pointer is nullptr, it will only allocate, without copying anything
	template<typename Allocator>
	ECS_INLINE void* Copy(Allocator* allocator, const void* data, size_t data_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) {
		void* allocation = allocator->Allocate(data_size, alignment, debug_info);
		if (data != nullptr) {
			memcpy(allocation, data, data_size);
		}
		return allocation;
	}

	// If the data size is different from 0 and the data pointer is nullptr, it will only allocate, without copying anything
	ECS_INLINE Stream<void> Copy(AllocatorPolymorphic allocator, Stream<void> data, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) {
		void* allocation = Allocate(allocator, data.size, alignment, debug_info);
		if (data.buffer != nullptr) {
			memcpy(allocation, data.buffer, data.size);
		}
		return { allocation, data.size };
	}

	ECS_INLINE void* Copy(AllocatorPolymorphic allocator, const void* data, size_t data_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) {
		return Copy(allocator, { data, data_size }, alignment, debug_info).buffer;
	}

	// If data size is 0, it will return the data back
	ECS_INLINE void* CopyNonZero(AllocatorPolymorphic allocator, const void* data, size_t data_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (data_size > 0) {
			return Copy(allocator, data, data_size, alignment, debug_info);
		}
		return (void*)data;
	}

	// If data size is 0, it will return the data back
	ECS_INLINE Stream<void> CopyNonZero(AllocatorPolymorphic allocator, Stream<void> data, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (data.size > 0) {
			return Copy(allocator, data, alignment, debug_info);
		}
		return data;
	}

	// It uses Malloc, it needs to be deallocated with Free
	ECS_INLINE void* CopyNonZeroMalloc(const void* data, size_t data_size, size_t alignment = alignof(void*)) {
		if (data_size > 0) {
			void* allocation = Malloc(data_size, alignment);
			if (allocation != nullptr) {
				memcpy(allocation, data, data_size);
			}
			return allocation;
		}
		return (void*)data;
	}

	// This version can call when allocator is nullptr - the base one doesn't (but this allows the alignment to be specified)
	ECS_INLINE void* CopyNonZeroEx(AllocatorPolymorphic allocator, const void* data, size_t data_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (allocator.allocator == nullptr) {
			return CopyNonZeroMalloc(data, data_size, alignment);
		}
		else {
			return CopyNonZero(allocator, data, data_size, alignment, debug_info);
		}
	}

	// If data size is zero, it will return data, else it will make a copy and return that instead
	// If the data pointer is nullptr but a data_size is specified, then it will only allocate, without copying anything
	template<typename Allocator>
	void* CopyNonZero(Allocator* allocator, const void* data, size_t data_size, size_t alignment = alignof(void*), DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (data_size > 0) {
			return Copy(allocator, data, data_size, alignment, debug_info);
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
			alignas(alignof(void*)) char temporary_memory[ECS_SIMD_BYTE_SIZE * sizeof(output.buffer)];
			size_t write_count = functor(input.buffer + simd_count, (decltype(output.buffer))temporary_memory, input.size - simd_count);
			memcpy(output.buffer + output_element_index, temporary_memory, output.MemoryOf(1) * write_count);
		}
	}

	// The functor receives as parameters (auto is_full_iteration, size_t index, size_t count)
	// The is_full_iteration is std::true_type{} if this is a normal iteration, std::false_type{}
	// If it is the remainder
	template<typename Functor>
	void ApplySIMDConstexpr(size_t count, size_t simd_elements, Functor&& functor) {
		size_t simd_count = GetSimdCount(count, simd_elements);
		for (size_t index = 0; index < simd_count; index += simd_elements) {
			functor(std::true_type{}, index, simd_elements);
		}

		if (simd_count < count) {
			functor(std::false_type{}, simd_count, count - simd_count);
		}
	}

	// The functor receives as parameters (auto is_full_iteration, const Input* input_values, Output* output_values, size_t element_count)
	// And must return how many elements to write (this value is ignored when a full iteration is in place, it is
	// needed only when there is a remainder - this value must be expressed in elements of output type). The is_full_iteration is
	// a std::true_type{} if this is a normal iteration and std::false_type{} if it is the remainder
	template<typename InputStreamType, typename OutputStreamType, typename Functor>
	void ApplySIMDConstexpr(InputStreamType input, OutputStreamType output, size_t simd_elements, size_t output_count_per_iteration, Functor&& functor) {
		size_t simd_count = GetSimdCount(input.size, simd_elements);
		size_t output_element_index = 0;
		for (size_t index = 0; index < simd_count; index += simd_elements) {
			// No need to marshal here
			functor(std::true_type{}, input.buffer + index, output.buffer + output_element_index, simd_elements);
			output_element_index += output_count_per_iteration;
		}

		if (simd_count < input.size) {
			// We need to perform another iteration here but with redirection
			alignas(alignof(void*)) char temporary_memory[ECS_SIMD_BYTE_SIZE];
			size_t write_count = functor(std::false_type{}, input.buffer + simd_count, (decltype(output.buffer))temporary_memory, input.size - simd_count);
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

	// Returns true if the given extended value belongs in the specified integer range, else false
	template<typename Integer>
	ECS_INLINE bool EnsureUnsignedIntegerIsInRange(size_t value) {
		static_assert(std::is_unsigned_v<Integer>, "EnsureUnsignedIntegerIsInRange template parameter must be an integer!");

		Integer min;
		Integer max;
		IntegerRange<Integer>(min, max);
		return value <= (size_t)max;
	}

	// Returns true if the given extended value belongs in the specified integer range, else false
	template<typename Integer>
	ECS_INLINE bool EnsureSignedIntegerIsInRange(int64_t value) {
		static_assert(std::is_signed_v<Integer>, "EnsureSignedIntegerIsInRange template parameter must be an integer!");

		Integer min;
		Integer max;
		IntegerRange<Integer>(min, max);
		return (int64_t)min <= value && value <= (int64_t)max;
	}

	// Doubles, floats and ints
	template<typename FundamentalType>
	ECS_INLINE FundamentalType FundamentalTypeMin() {
		if constexpr (std::is_integral_v<FundamentalType>) {
			FundamentalType min, max;
			IntegerRange(min, max);
			return min;
		}
		else if constexpr (std::is_same_v<float, FundamentalType>) {
			return -FLT_MAX;
		}
		else if constexpr (std::is_same_v<double, FundamentalType>) {
			return -DBL_MAX;
		}
		else {
			static_assert(false, "Invalid fundamental type for Min");
		}
	}

	template<typename FundamentalType>
	ECS_INLINE FundamentalType FundamentalTypeMax() {
		if constexpr (std::is_integral_v<FundamentalType>) {
			FundamentalType min, max;
			IntegerRange(min, max);
			return max;
		}
		else if constexpr (std::is_same_v<float, FundamentalType>) {
			return FLT_MAX;
		}
		else if constexpr (std::is_same_v<double, FundamentalType>) {
			return DBL_MAX;
		}
		else {
			static_assert(false, "Invalid fundamental type for Min");
		}
	}

	template<typename FundamentalType>
	ECS_INLINE void FundamentalTypeMinMax(FundamentalType& min, FundamentalType& max) {
		min = FundamentalTypeMin<FundamentalType>();
		max = FundamentalTypeMax<FundamentalType>();
	}

	// Attempts to reduce the numbers of samples into averages while also maintaining spikes (high values)
	// The entire set is split into chunks for each a single sample is generated. The values are averaged,
	// But the function will try to detect spikes in order to not be averaged out and lose them out of sight
	// A spike is considered as being the highest value and it is larger than the spike_threshold multiplied by
	// the average of the chunk.
	// Returns the number of valid samples (it can happen that there are fewer entries)
	template<typename FundamentalType, typename ContainerType, typename SampleStream, typename SetSampleIndex>
	size_t ReduceSamplesImpl(SampleStream samples, ContainerType container, double spike_threshold, unsigned int sample_offset, SetSampleIndex set_index) {					
		if constexpr (!IsStreamType<FundamentalType, ContainerType>() && !IsQueueType<FundamentalType, ContainerType>()) {
			static_assert(false, "Invalid container type for ReduceSamples");
		}

		size_t entry_count = 0;
		if constexpr (IsStreamType<FundamentalType, ContainerType>()) {
			entry_count = container.size;
		}
		else {
			entry_count = container.GetSize();
		}
		if (samples.size >= entry_count) {
			if constexpr (IsStreamType<FundamentalType, ContainerType>()) {
				for (size_t index = 0; index < entry_count; index++) {
					set_index(index, container[index]);
				}
			}
			else {
				samples.size = 0;
				container.ForEach([&](FundamentalType value) {
					set_index(samples.size, (float)value);
					samples.size++;
				});
			}
			return entry_count;
		}
		else {
			size_t per_chunk_count = entry_count / samples.size;
			size_t chunk_remainder = entry_count % samples.size;
			size_t entry_offset = 0;

			size_t sample_offset_modulo = (size_t)sample_offset % per_chunk_count;
			for (size_t index = 0; index < samples.size; index++) {
				// Reduce the count by the sample offset to maintain a chunk order
				size_t current_chunk_count = per_chunk_count + (chunk_remainder != 0) - sample_offset_modulo;
				sample_offset_modulo -= sample_offset_modulo;
				chunk_remainder = chunk_remainder == 0 ? 0 : chunk_remainder - 1;

				// Use doubles for the averaging
				double chunk_average = 0.0;
				FundamentalType chunk_max = FundamentalTypeMin<FundamentalType>();
				if constexpr (IsStreamType<FundamentalType, ContainerType>()) {
					for (size_t subindex = entry_offset; subindex < entry_offset + current_chunk_count; subindex++) {
						chunk_average += (double)container[subindex];
						chunk_max = max(container[subindex], chunk_max);
					}
				}
				else {
					container.ForEachRange(entry_offset, entry_offset + current_chunk_count, [&](FundamentalType current_value) {
						chunk_average += (double)current_value;
						chunk_max = max(current_value, chunk_max);
					});
				}

				chunk_average /= (double)current_chunk_count;
				if ((double)chunk_max > spike_threshold * chunk_average) {
					set_index(index, chunk_max);
				}
				else {
					set_index(index, chunk_average);
				}

				entry_offset += current_chunk_count;
			}

			return samples.size;
		}
	}

	// Attempts to reduce the numbers of samples into averages while also maintaining spikes (high values)
	// The entire set is split into chunks for each a single sample is generated. The values are averaged,
	// But the function will try to detect spikes in order to not be averaged out and lose them out of sight
	// A spike is considered as being the highest value and it is larger than the spike_threshold multiplied by
	// the average of the chunk.
	// Returns the number of valid samples (it can happen that there are fewer entries)
	template<typename FundamentalType, typename ContainerType>
	size_t ReduceSamples(Stream<FundamentalType> samples, ContainerType container, double spike_threshold, unsigned int sample_offset) {
		return ReduceSamplesImpl<FundamentalType>(samples, container, spike_threshold, sample_offset, [&](size_t index, FundamentalType value) {
			samples[index] = value;
		});
	}

	// Attempts to reduce the numbers of samples into averages while also maintaining spikes (high values)
	// The entire set is split into chunks for each a single sample is generated. The values are averaged,
	// But the function will try to detect spikes in order to not be averaged out and lose them out of sight
	// A spike is considered as being the highest value and it is larger than the spike_threshold multiplied by
	// the average of the chunk.
	// Returns the number of valid samples (it can happen that there are fewer entries)
	template<typename FundamentalType, typename ContainerType>
	size_t ReduceSamplesToGraph(Stream<float2> samples, ContainerType container, double spike_threshold, unsigned int sample_offset) {
		return ReduceSamplesImpl<FundamentalType>(samples, container, spike_threshold, sample_offset, [&](size_t index, FundamentalType value) {
			const float step = 1.0f;
			samples[index] = { (float)index * step, (float)value };
		});
	}

	// It will group the entries that have the same group index and call the functor for each entry of the group.
	// The entry_functor receives as parameters (size_t overall_index, size_t original_index, size_t group_index).
	// The overall index is the index of the sorted entry, i.e. an index that is incremented for each entry
	// The original index is the index inside the group_indices array that can be used to reference other data
	// The group_index parameter is the index of the ordered group it belongs to
	// The group_functor receives as parameters (size_t group_index, ulong2 group_count).
	// The group_count parameter contains in the x component the group value (the one in group_indices) and in the y the count of existing entries
	// The temporary allocator is needed in order to make a temporary allocation
	// IntegerType should be a fundamental integer type
	template<typename EntryFunctor, typename GroupFunctor, typename IntegerType>
	void ForEachGroup(Stream<IntegerType> group_indices, AllocatorPolymorphic temporary_allocator, EntryFunctor&& entry_functor, GroupFunctor&& group_functor)
	{
		size_t allocation_size = BooleanBitField::MemoryOf(group_indices.size);
		void* allocation = AllocateEx(temporary_allocator, allocation_size);
		memset(allocation, 0, allocation_size);
		BooleanBitField bit_field(allocation, group_indices.size);

		ulong2 current_group;
		size_t group_index = 0;
		size_t ordered_index = 0;
		for (size_t index = 0; index < group_indices.size; index++) {
			if (!bit_field.GetAndSet(index)) {
				current_group.x = group_indices[index];
				current_group.y = 0;
				entry_functor(ordered_index, index, group_index);
				ordered_index++;

				size_t next_entry_index = SearchBytes(group_indices.buffer + index + 1, group_indices.size - index, current_group.x, sizeof(current_group.x));
				while (next_entry_index != -1) {
					current_group.y++;
					index++;
					entry_functor(ordered_index, next_entry_index, group_index);
					ordered_index++;
				
					next_entry_index = SearchBytes(group_indices.buffer + next_entry_index + 1, group_indices.size - next_entry_index, current_group.x, sizeof(current_group.x));
				}

				group_functor(group_index, current_group);
				group_index++;
			}
		}

		DeallocateEx(temporary_allocator, allocation);
	}

	// A helper function that helps you write an entry with an arbitrary number of bits.
	// The functor receives as parameters (size_t index, void* entry) where entry is a byte aligned value
	// That will be properly written afterwards. The entry_bit_count must be at max the bit count of the largest integer.
	// Returns the number of bytes written.
	template<typename Functor>
	size_t WriteBits(void* bits, size_t entry_bit_count, size_t entry_count, Functor&& functor) {
		ECS_ASSERT(entry_bit_count <= sizeof(size_t) * 8);
		unsigned char* bytes = (unsigned char*)bits;
		size_t byte_index = 0;
		unsigned char shift_count = 0;
		
		size_t functor_value = 0;
		for (size_t index = 0; index < entry_count; index++) {
			if (shift_count == 0) {
				bytes[byte_index] = 0;
			}

			functor(index, &functor_value);
			size_t current_bit_count = entry_bit_count;
			// Write the first bits first into the available byte and if the space
			// Is not enough, then continue
			unsigned char first_byte_remaining_bits = 8 - shift_count;
			bytes[byte_index] |= (unsigned char)functor_value << shift_count;
			shift_count += current_bit_count;
			if (shift_count >= 8) {
				byte_index++;
				shift_count = 0;
				current_bit_count -= first_byte_remaining_bits;
			}
			else
			{
				current_bit_count = (size_t)first_byte_remaining_bits >= current_bit_count ? 0 : current_bit_count - (size_t)first_byte_remaining_bits;
			}
			functor_value >>= first_byte_remaining_bits;

			// The parameter is needed only for its type, not for its value
			auto perform_int_operation = [&current_bit_count, &functor_value, &byte_index, bytes](auto int_type) {
				if (current_bit_count >= sizeof(decltype(int_type)) * 8) {
					// Use the int_type
					decltype(int_type)* unsigned_value = (decltype(int_type)*)(bytes + byte_index);
					*unsigned_value = functor_value;
					functor_value >>= sizeof(decltype(int_type)) * 8;
					byte_index += sizeof(decltype(int_type));
				}
			};

			// Start from the highest integer and go downwards to the smallest integer
			// If it fits into the value, use it
			perform_int_operation((unsigned int)0);
			perform_int_operation((unsigned short)0);
			perform_int_operation((unsigned char)0);

			// If we still have bits left, write them
			if (current_bit_count > 0) {
				bytes[byte_index] |= (unsigned char)functor_value;
				shift_count += (unsigned char)current_bit_count;
			}
		}

		return shift_count > 0 ? byte_index + 1 : byte_index;
	}

	// A helper function that helps you read a packed bit stream, which was written with WriteBits.
	// The functor is called with the parameters (size_t index, const void* value) where value is a byte
	// Aligned value. Entry_bit_count must not be larger than the bit count of the largest integer.
	// Returns the number of bytes read
	template<typename Functor>
	size_t ReadBits(const void* bits, size_t entry_bit_count, size_t entry_count, Functor&& functor) {
		ECS_ASSERT(entry_bit_count <= sizeof(size_t) * 8);
		const unsigned char* bytes = (const unsigned char*)bits;
		size_t byte_index = 0;
		unsigned char shift_count = 0;

		size_t functor_value = 0;
		for (size_t index = 0; index < entry_count; index++) {
			size_t current_bit_count = entry_bit_count;
			size_t functor_shift_count = 0;

			// If we have remaining bits from the current byte, read them first
			functor_value = 0;
			if (shift_count > 0) {
				functor_value |= (size_t)(bytes[byte_index] >> shift_count);
				if ((size_t)(8 - shift_count) >= current_bit_count) {
					shift_count += (unsigned char)current_bit_count;
					current_bit_count = 0;
				}
				else {
					functor_shift_count += 8 - shift_count;
					current_bit_count = current_bit_count - functor_shift_count;
					byte_index++;
					shift_count = 0;
				}
			}

			// The parameter is needed only for its type, not for its value
			auto perform_int_operation = [&current_bit_count, &functor_value, &functor_shift_count, &byte_index, bytes](auto int_type) {
				if (current_bit_count >= sizeof(decltype(int_type)) * 8) {
					const decltype(int_type)* unsigned_value = (const decltype(int_type)*)(bytes + byte_index);
					functor_value |= (size_t)*unsigned_value << functor_shift_count;
					functor_shift_count += sizeof(decltype(int_type)) * 8;
					current_bit_count -= sizeof(decltype(int_type)) * 8;
					byte_index += sizeof(decltype(int_type));
				}
			};

			// Start from the largest integer to the smallest
			perform_int_operation((size_t)0);
			perform_int_operation((unsigned int)0);
			perform_int_operation((unsigned short)0);
			perform_int_operation((unsigned char)0);

			// If there are still bits left, read them
			if (current_bit_count > 0) {
				functor_value |= (size_t)bytes[byte_index] << functor_shift_count;
				shift_count += (unsigned char)current_bit_count;
			}

			functor(index, &functor_value);
		}

		return shift_count > 0 ? byte_index + 1 : byte_index;
	}

	// Returns a normalized [0-1] value knowing the current value and the interval range
	ECS_INLINE float NormalizedValue(float value, float start, float end) {
		return (value - start) / (end - start);
	}

	// Returns the absolute value from the a normalized [0-1] range
	ECS_INLINE float AbsoluteValueFromNormalized(float normalized_value, float start, float end) {
		return start + normalized_value * (end - start);
	}

	// Remaps a value from an interval to a value that corresponds to the other interval, by normalizing the value
	// From the original interval and then retrieving the absolute value from the second interval.
	ECS_INLINE float RemapRange(float value, float original_range_start, float original_range_end, float new_range_start, float new_range_end) {
		float normalized_value = NormalizedValue(value, original_range_start, original_range_end);
		return AbsoluteValueFromNormalized(normalized_value, new_range_start, new_range_end);
	}

}