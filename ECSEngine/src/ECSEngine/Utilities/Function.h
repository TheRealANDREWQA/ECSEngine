#pragma once
#include "ecspch.h"
#include "../Core.h"
#include "../Containers/Stream.h"
#include "BasicTypes.h"
#include "Assert.h"
#include "../Allocators/AllocatorTypes.h"

#define ECS_ASSERT_TRIGGER
#define ECS_C_FILE_SINGLE_LINE_COMMENT_TOKEN "//"
#define ECS_C_FILE_MULTI_LINE_COMMENT_OPENED_TOKEN "/*"
#define ECS_C_FILE_MULTI_LINE_COMMENT_CLOSED_TOKEN "*/"

namespace ECSEngine {

	constexpr size_t ECS_LOCAL_TIME_FORMAT_HOUR = 1 << 0;
	constexpr size_t ECS_LOCAL_TIME_FORMAT_MINUTES = 1 << 1;
	constexpr size_t ECS_LOCAL_TIME_FORMAT_SECONDS = 1 << 2;
	constexpr size_t ECS_LOCAL_TIME_FORMAT_MILLISECONDS = 1 << 3;
	constexpr size_t ECS_LOCAL_TIME_FORMAT_DAY = 1 << 4;
	constexpr size_t ECS_LOCAL_TIME_FORMAT_MONTH = 1 << 5;
	constexpr size_t ECS_LOCAL_TIME_FORMAT_YEAR = 1 << 6;
	constexpr size_t ECS_LOCAL_TIME_FORMAT_DASH_INSTEAD_OF_COLON = 1 << 7;
	constexpr size_t ECS_LOCAL_TIME_FORMAT_ALL = ECS_LOCAL_TIME_FORMAT_HOUR | ECS_LOCAL_TIME_FORMAT_MINUTES | ECS_LOCAL_TIME_FORMAT_SECONDS
		| ECS_LOCAL_TIME_FORMAT_MILLISECONDS | ECS_LOCAL_TIME_FORMAT_DAY | ECS_LOCAL_TIME_FORMAT_MONTH | ECS_LOCAL_TIME_FORMAT_YEAR;

	namespace function {

		// It will remap the pointer from the first base into the second base
		ECSENGINE_API void* RemapPointer(const void* first_base, const void* second_base, const void* pointer);

		template<typename CharacterType>
		ECS_INLINE CharacterType Character(char character) {
			if constexpr (std::is_same_v<CharacterType, char>) {
				return character;
			}
			else if constexpr (std::is_same_v<CharacterType, wchar_t>) {
				return (wchar_t)((int)L'\0' + (int)character);
			}
		}

		ECS_INLINE bool IsPowerOfTwo(int x) {
			return (x & (x - 1)) == 0;
		}

		ECS_INLINE uintptr_t AlignPointer(uintptr_t pointer, size_t alignment) {
			ECS_ASSERT(IsPowerOfTwo(alignment));

			size_t mask = alignment - 1;
			return (pointer + mask) & ~mask;
		}

		ECS_INLINE void* AlignPointer(const void* pointer, size_t alignment) {
			return (void*)AlignPointer((uintptr_t)pointer, alignment);
		}

		// Determines how many slots are needed to hold the given count with the chunk size
		ECS_INLINE size_t SlotsFor(size_t count, size_t chunk_size) {
			return count / chunk_size + ((count % chunk_size) != 0);
		}

		ECS_INLINE void Capitalize(char* character) {
			if (*character >= 'a' && *character <= 'z') {
				*character = *character - 32;
			}
		}

		ECS_INLINE void Uncapitalize(char* character) {
			if (*character >= 'A' && *character <= 'Z') {
				*character += 'a' - 'A';
			}
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
			return *(T*)function::OffsetPointer(texture_data, row * row_byte_size + column * sizeof(T));
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

		ECS_INLINE bool CompareStrings(Stream<wchar_t> string, Stream<wchar_t> other) {
			return string.size == other.size && (memcmp(string.buffer, other.buffer, sizeof(wchar_t) * other.size) == 0);
		}

		ECS_INLINE bool CompareStrings(Stream<char> string, Stream<char> other) {
			return string.size == other.size && (memcmp(string.buffer, other.buffer, sizeof(char) * other.size) == 0);
		}

		ECS_INLINE int StringLexicographicCompare(Stream<char> left, Stream<char> right) {
			size_t smaller_size = std::min(left.size, right.size);
			int result = memcmp(left.buffer, right.buffer, smaller_size * sizeof(char));
			if (result == 0) {
				if (left.size < right.size) {
					return -1;
				}
				else if (left.size == right.size) {
					return 0;
				}
				else {
					return 1;
				}
			}
			return result;
		}

		// This is an implementation version because if use this version then we can't have
		// conversions between capacity streams and normal streams.
		template<typename CharacterType>
		ECS_INLINE bool StringIsLessImpl(Stream<CharacterType> string, Stream<CharacterType> comparator) {
			for (size_t index = 0; index < string.size && index < comparator.size; index++) {
				if (string[index] < comparator[index]) {
					return true;
				}
				if (string[index] > comparator[index]) {
					return false;
				}
			}

			// If the string has fewer characters than the other, than it is less
			return string.size < comparator.size;
		}

		ECS_INLINE bool StringIsLess(Stream<char> string, Stream<char> comparator) {
			return StringIsLessImpl(string, comparator);
		}

		ECS_INLINE bool StringIsLess(Stream<wchar_t> string, Stream<wchar_t> comparator) {
			return StringIsLessImpl(string, comparator);
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

		// Returns a / b + ((a % b) != 0)
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
			return low_bound < value && value < high_bound;
		}

		ECS_INLINE bool FloatCompare(float a, float b, float epsilon = 0.00001f) {
			return fabsf(a - b) < epsilon;
		}

		// Returns the overlapping line as a pair of start, end (end is not included)
		// If the lines don't overlap, it will return { -1, -1 }
		ECSENGINE_API uint2 LineOverlap(
			unsigned int first_start,
			unsigned int first_end,
			unsigned int second_start,
			unsigned int second_end
		);

		// Returns the rectangle stored as xy - top left, zw - bottom right (the end is not included)
		// that is overlapping between these 2 rectangles. If zero_if_not_valid
		// is set to true then if one of the dimensions is 0, it will make the other
		// one 0 as well (useful for iteration for example, since it will result in
		// less iterations). If the rectangles don't overlap, it will return
		// { -1, -1, -1, -1 }
		ECSENGINE_API uint4 RectangleOverlap(
			uint2 first_top_left,
			uint2 first_bottom_right,
			uint2 second_top_left,
			uint2 second_bottom_right,
			bool zero_if_not_valid = true
		);

		// a - b
		ECS_INLINE size_t PointerDifference(const void* a, const void* b) {
			return (uintptr_t)a - (uintptr_t)b;
		}

		// Returns true if the pointer >= base && pointer < base + size
		ECS_INLINE bool IsPointerRange(const void* base, size_t size, const void* pointer) {
			return pointer >= base && PointerDifference(pointer, base) < size;
		}

		ECS_INLINE bool AreAliasing(Stream<void> first, Stream<void> second) {
			auto test = [](Stream<void> first, Stream<void> second) {
				return first.buffer >= second.buffer && (first.buffer < function::OffsetPointer(second.buffer, second.size));
			};
			return test(first, second) || test(second, first);
		}

		ECS_INLINE void* RemapPointerIfInRange(const void* base, size_t size, const void* new_base, const void* pointer) {
			if (IsPointerRange(base, size, pointer)) {
				return RemapPointer(base, new_base, pointer);
			}
			return (void*)pointer;
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

		/* Supports alignments up to 256 bytes */
		ECS_INLINE uintptr_t AlignPointerStack(uintptr_t pointer, size_t alignment) {
			uintptr_t first_aligned_pointer = AlignPointer(pointer, alignment);
			return first_aligned_pointer + alignment * ((first_aligned_pointer - pointer) == 0);
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

		// Extends the 47th bit into the 48-63 range
		ECS_INLINE void* SignExtendPointer(const void* pointer) {
			intptr_t ptr = (intptr_t)pointer;
			ptr <<= 16;
			ptr >>= 16;
			return (void*)ptr;
		}

		// It will copy the null termination character
		ECS_INLINE Stream<char> StringCopy(AllocatorPolymorphic allocator, Stream<char> string) {
			if (string.size > 0) {
				Stream<char> result = { Allocate(allocator, string.MemoryOf(string.size + 1)), string.size };
				result.Copy(string);
				result[string.size] = '\0';
				return result;
			}
			return { nullptr, 0 };
		}

		// It will copy the null termination character
		ECS_INLINE Stream<wchar_t> StringCopy(AllocatorPolymorphic allocator, Stream<wchar_t> string) {
			if (string.size > 0) {
				Stream<wchar_t> result = { Allocate(allocator, string.MemoryOf(string.size + 1)), string.size };
				result.Copy(string);
				result[string.size] = L'\0';
				return result;
			}
			return { nullptr, 0 };
		}

		// The type must have as its first field a size_t describing the stream size
		// It returns the total amount of data needed to copy this structure
		template<typename StreamType, typename Type>
		inline size_t EmbedStream(Type* type, Stream<StreamType> stream) {
			memcpy(function::OffsetPointer(type, sizeof(*type)), stream.buffer, stream.MemoryOf(stream.size));
			size_t* size_ptr = (size_t*)type;
			*size_ptr = stream.size;
			return stream.MemoryOf(stream.size) + sizeof(*type);
		}

		// The type must have its first field a size_t describing its stream size
		template<typename StreamType, typename Type>
		ECS_INLINE Stream<StreamType> GetEmbeddedStream(const Type* type) {
			const size_t* size_ptr = (const size_t*)type;
			return { function::OffsetPointer(type, sizeof(*type)), *size_ptr };
		}

		ECS_INLINE bool IsNumberCharacter(char value) {
			return value >= '0' && value <= '9';
		}

		ECS_INLINE bool IsNumberCharacter(wchar_t value) {
			return value >= L'0' && value <= L'9';
		}

		ECS_INLINE bool IsAlphabetCharacter(char value) {
			return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
		}

		ECS_INLINE bool IsAlphabetCharacter(wchar_t value) {
			return (value >= L'a' && value <= L'z') || (value >= L'A' && value <= L'Z');
		}

		// Spaces and tabs
		ECS_INLINE bool IsWhitespace(char value) {
			return value == ' ' || value == '\t';
		}

		// Spaces and tabs
		ECS_INLINE bool IsWhitespace(wchar_t value) {
			return value == L' ' || value == L'\t';
		}

		// Spaces, tabs and new lines
		ECS_INLINE bool IsWhitespaceEx(char value) {
			return IsWhitespace(value) || value == '\n';
		}

		ECS_INLINE bool IsWhitespaceEx(wchar_t value) {
			return IsWhitespace(value) || value == L'\n';
		}

		ECS_INLINE bool IsCodeIdentifierCharacter(char value) {
			return IsNumberCharacter(value) || IsAlphabetCharacter(value) || value == '_';
		}

		ECS_INLINE bool IsCodeIdentifierCharacter(wchar_t value) {
			return IsNumberCharacter(value) || IsAlphabetCharacter(value) || value == L'_';
		}

		// Can use the increment to go backwards by setting it to -1
		ECS_INLINE const char* SkipSpace(const char* pointer, int increment = 1) {
			while (*pointer == ' ') {
				pointer += increment;
			}
			return pointer;
		}

		// pointers should be aligned preferably to 32 bytes at least
		ECSENGINE_API void avx2_copy(void* destination, const void* source, size_t bytes);

		ECS_INLINE void ConvertASCIIToWide(wchar_t* wide_string, const char* pointer, size_t max_w_string_count) {
			int result = MultiByteToWideChar(CP_ACP, 0, pointer, -1, wide_string, max_w_string_count);
		}

		ECS_INLINE void ConvertASCIIToWide(wchar_t* wide_string, Stream<char> pointer, size_t max_w_string_count) {
			int result = MultiByteToWideChar(CP_ACP, 0, pointer.buffer, pointer.size, wide_string, max_w_string_count);
		}

		ECS_INLINE void ConvertASCIIToWide(CapacityStream<wchar_t>& wide_string, Stream<char> ascii_string) {
			int result = MultiByteToWideChar(CP_ACP, 0, ascii_string.buffer, ascii_string.size, wide_string.buffer + wide_string.size, wide_string.capacity);
			wide_string.size += ascii_string.size;
		}

		ECS_INLINE void ConvertASCIIToWide(CapacityStream<wchar_t>& wide_string, CapacityStream<char> ascii_string) {
			int result = MultiByteToWideChar(CP_ACP, 0, ascii_string.buffer, ascii_string.size, wide_string.buffer + wide_string.size, wide_string.capacity);
			wide_string.size += ascii_string.size;
		}

		// Can use the increment to go backwards by setting it to -1
		ECS_INLINE const char* SkipCodeIdentifier(const char* pointer, int increment = 1) {
			while (IsCodeIdentifierCharacter(*pointer)) {
				pointer += increment;
			}
			return pointer;
		}

		ECS_INLINE const wchar_t* SkipCodeIdentifier(const wchar_t* pointer, int increment = 1) {
			while (IsCodeIdentifierCharacter(*pointer)) {
				pointer += increment;
			}
			return pointer;
		}

		// returns the count of decoded numbers
		ECSENGINE_API size_t ParseNumbersFromCharString(Stream<char> character_buffer, unsigned int* number_buffer);

		// returns the count of decoded numbers
		ECSENGINE_API size_t ParseNumbersFromCharString(Stream<char> character_buffer, int* number_buffer);

		ECS_INLINE void ConvertWideCharsToASCII(
			const wchar_t* wide_chars,
			char* chars,
			size_t wide_char_count,
			size_t destination_size,
			size_t max_char_count,
			size_t& written_chars
		) {
			// counts the null terminator aswell
			errno_t status = wcstombs_s(&written_chars, chars, destination_size, wide_chars, max_char_count);
			if (written_chars > 0) {
				written_chars--;
			}
			ECS_ASSERT(status == 0);
		}

		ECS_INLINE void ConvertWideCharsToASCII(
			const wchar_t* wide_chars,
			char* chars,
			size_t wide_char_count,
			size_t max_char_count
		) {
			size_t written_chars = 0;
			errno_t status = wcstombs_s(&written_chars, chars, max_char_count, wide_chars, wide_char_count);
			ECS_ASSERT(status == 0);
		}

		ECS_INLINE void ConvertWideCharsToASCII(
			Stream<wchar_t> wide_chars,
			CapacityStream<char>& ascii_chars
		) {
			size_t written_chars = 0;
			errno_t status = wcstombs_s(&written_chars, ascii_chars.buffer + ascii_chars.size, ascii_chars.capacity - ascii_chars.size, wide_chars.buffer, wide_chars.size);
			ECS_ASSERT(status == 0);
			ascii_chars.size += written_chars - 1;
		}

		template<typename CharacterType>
		ECS_INLINE const CharacterType* SkipCharacter(const CharacterType* pointer, CharacterType character, int increment = 1) {
			while (*pointer == character) {
				pointer += increment;
			}
			return pointer;
		}

		// Tabs and spaces
		// Can use the increment to go backwards by setting it to -1
		ECS_INLINE const char* SkipWhitespace(const char* pointer, int increment = 1) {
			while (*pointer == ' ' || *pointer == '\t') {
				pointer += increment;
			}
			return pointer;
		}

		// Can use the increment to go backwards by setting it to -1
		ECS_INLINE const wchar_t* SkipWhitespace(const wchar_t* pointer, int increment = 1) {
			while (*pointer == L' ' || *pointer == L'\t') {
				pointer += increment;
			}
			return pointer;
		}

		// Tabs, spaces and new lines
		ECS_INLINE const char* SkipWhitespaceEx(const char* pointer, int increment = 1) {
			while (*pointer == ' ' || *pointer == '\t' || *pointer == '\n') {
				pointer += increment;
			}
			return pointer;
		}

		ECS_INLINE const wchar_t* SkipWhitespaceEx(const wchar_t* pointer, int increment = 1) {
			while (*pointer == L' ' || *pointer == L'\t' || *pointer == L'\n') {
				pointer += increment;
			}
			return pointer;
		}

		// Shifts the pointer 3 positions to the right in order to provide significant digits for hashing functions
		// like power of two that use the lower bits in order to hash the element inside the table.
		// It will clip to only 24 bits - 3 bytes - that's the precision the hash table work with
		ECS_INLINE unsigned int PointerHash(const void* ptr) {
			return (unsigned int)(((uintptr_t)ptr >> 3) & 0x0000000000FFFFFF);
		}

		ECS_INLINE size_t GetSimdCount(size_t count, size_t vector_size) {
			return count & (-vector_size);
		}

		template<bool is_delta = false, typename Value>
		ECS_INLINE Value Lerp(Value a, Value b, float percentage) {
			if constexpr (!is_delta) {
				return (b - a) * percentage + a;
			}
			else {
				return b * percentage + a;
			}
		}

		template<bool is_delta = false, typename Value>
		ECS_INLINE Value LerpDouble(Value a, Value b, double percentage) {
			if constexpr (!is_delta) {
				return (b - a) * percentage + a;
			}
			else {
				return b * percentage + a;
			}
		}

		template<typename Value>
		ECS_INLINE float InverseLerp(Value a, Value b, Value c) {
			return (c - a) / (b - a);
		}

		template<typename Value>
		inline Value PlanarLerp(Value a, Value b, Value c, Value d, float x_percentage, float y_percentage) {
			// Interpolation formula
			// a ----- b
			// |       |
			// |       |
			// |       |
			// c ----- d

			// result = a * (1 - x)(1 - y) + b * x (1 - y) + c * (1 - x) y + d * x y;

			return a * ((1.0f - x_percentage) * (1.0f - y_percentage)) + b * (x_percentage * (1.0f - y_percentage)) +
				c * ((1.0f - x_percentage) * y_percentage) + d * (x_percentage * y_percentage);
		}

		template<typename Function>
		ECS_INLINE float2 SampleFunction(float value, Function&& function) {
			return { value, function(value) };
		}

		template<typename Value>
		ECS_INLINE Value Clamp(Value value, Value min, Value max) {
			return ClampMax(ClampMin(value, min), max);
		}

		// The value will not be less than min
		template<typename Value>
		ECS_INLINE Value ClampMin(Value value, Value min) {
			return value < min ? min : value;
		}

		// The value will not be greater than max
		template<typename Value>
		ECS_INLINE Value ClampMax(Value value, Value max) {
			return value > max ? max : value;
		}

		// it searches for spaces and next line characters
		ECSENGINE_API size_t ParseWordsFromSentence(Stream<char> sentence, char separator_token = ' ');

		// Positions will be filled with the 4 corners of the rectangle
		ECSENGINE_API void ObliqueRectangle(float2* positions, float2 a, float2 b, float thickness);

		ECSENGINE_API void CopyStreamAndMemset(void* destination, size_t destination_capacity, Stream<void> data, int memset_value = 0);

		// It will advance the ptr with the destination_capacity
		ECSENGINE_API void CopyStreamAndMemset(uintptr_t& ptr, size_t destination_capacity, Stream<void> data, int memset_value = 0);

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
		
		// Duration should be expressed as milliseconds
		// Returns how many characters were written
		ECSENGINE_API size_t ConvertDurationToChars(size_t duration_milliseconds, char* characters);

		// finds the tokens that appear in the current string
		ECSENGINE_API void FindToken(Stream<char> string, char token, CapacityStream<unsigned int>& tokens);

		// finds the tokens that appear in the current string
		ECSENGINE_API void FindToken(Stream<char> string, Stream<char> token, CapacityStream<unsigned int>& tokens);

		ECSENGINE_API void FindToken(Stream<wchar_t> string, wchar_t token, CapacityStream<unsigned int>& tokens);

		ECSENGINE_API void FindToken(Stream<wchar_t> string, Stream<wchar_t> token, CapacityStream<unsigned int>& tokens);

		// Convenience function - the more efficient is the unsigned int version that returns offsets into the string
		ECSENGINE_API void FindToken(Stream<char> string, Stream<char> token, CapacityStream<Stream<char>>& tokens);

		// Convenience function - the more efficient is the unsigned int version that returns offsets into the string
		ECSENGINE_API void FindToken(Stream<wchar_t> string, Stream<wchar_t> token, CapacityStream<Stream<wchar_t>>& tokens);

		// It will return the first appereance of the token inside the character stream
		// It will not call strstr, it uses a SIMD search, this function being well suited if searching a large string
		// Returns { nullptr, 0 } if it doesn't exist, else a string that starts with the token
		// until the end of the characters string
		ECSENGINE_API Stream<char> FindFirstToken(Stream<char> characters, Stream<char> token);

		// It will return the first appereance of the token inside the character stream
		// It will not call strstr, it uses a SIMD search, this function being well suited if searching a large string
		// Returns { nullptr, 0 } if it doesn't exist, else a string that starts with the token
		// until the end of the characters string
		ECSENGINE_API Stream<wchar_t> FindFirstToken(Stream<wchar_t> characters, Stream<wchar_t> token);

		// It will return the first appereance of the token inside the character stream
		// It will not call strchr, this function being well suited if searching a large string
		// Returns { nullptr, 0 } if it doesn't exit, else a string that starts with the token
		// until the end of the character string
		ECSENGINE_API Stream<char> FindFirstCharacter(Stream<char> characters, char token);

		// It will return the first appereance of the token inside the character stream
		// It will not call strchr, this function being well suited if searching a large string
		// Returns { nullptr, 0 } if it doesn't exit, else a string that starts with the token
		// until the end of the character string
		ECSENGINE_API Stream<wchar_t> FindFirstCharacter(Stream<wchar_t> characters, wchar_t token);

		// It will search from the end of the characters string till its start
		// It uses SIMD to speed up the find
		ECSENGINE_API Stream<char> FindTokenReverse(Stream<char> characters, Stream<char> token);

		// It will search from the end of the characters string till its start
		// It uses SIMD to speed up the find
		ECSENGINE_API Stream<wchar_t> FindTokenReverse(Stream<wchar_t> characters, Stream<wchar_t> token);

		// It will search the string from the last character until the starting one
		ECSENGINE_API Stream<char> FindCharacterReverse(Stream<char> characters, char character);

		// It will search the string from the last character until the starting one
		ECSENGINE_API Stream<wchar_t> FindCharacterReverse(Stream<wchar_t> characters, wchar_t character);

		// Returns nullptr if it doesn't find a match or there is an invalid number of parenthesis
		ECSENGINE_API const char* FindMatchingParenthesis(
			const char* start_character, 
			const char* end_character, 
			char opened_char,
			char closed_char,
			unsigned int opened_count = 1
		);

		ECSENGINE_API Stream<char> FindMatchingParenthesis(
			Stream<char> range,
			char opened_char,
			char closed_char,
			unsigned int opened_count = 1
		);

		// Returns nullptr if it doesn't find a match or there is an invalid number of parenthesis
		ECSENGINE_API const wchar_t* FindMatchingParenthesis(
			const wchar_t* start_character, 
			const wchar_t* end_character, 
			wchar_t opened_char, 
			wchar_t closed_char,
			unsigned int opened_count = 1
		);

		ECSENGINE_API Stream<wchar_t> FindMatchingParenthesis(
			Stream<wchar_t> range,
			wchar_t opened_char,
			wchar_t closed_char,
			unsigned int opened_count = 1
		);

		ECSENGINE_API Stream<char> FindDelimitedString(Stream<char> range, char opened_delimiter, char closed_delimiter, bool skip_whitespace = false);

		ECSENGINE_API unsigned int FindString(const char* string, Stream<const char*> other);

		ECSENGINE_API unsigned int FindString(Stream<char> string, Stream<Stream<char>> other);

		ECSENGINE_API unsigned int FindString(const wchar_t* string, Stream<const wchar_t*> other);

		ECSENGINE_API unsigned int FindString(Stream<wchar_t> string, Stream<Stream<wchar_t>> other);

		// Looks up into a stream at the given offset to look for the string.
		// Also needs to specify if capacity or not
		ECSENGINE_API unsigned int FindStringOffset(Stream<char> string, const void* strings_buffer, size_t strings_count, size_t string_byte_size, unsigned int offset, bool capacity);

		// Looks up into a stream at the given offset to look for the string.
		// Also needs to specify if capacity or not
		ECSENGINE_API unsigned int FindStringOffset(Stream<wchar_t> string, const void* strings_buffer, size_t strings_count, size_t string_byte_size, unsigned int offset, bool capacity);

		// Generates the string variants of the given numbers
		ECSENGINE_API void FromNumbersToStrings(Stream<unsigned int> values, CapacityStream<char>& storage, Stream<char>* strings);

		// Generates the string variants of the given numbers
		ECSENGINE_API void FromNumbersToStrings(size_t count, CapacityStream<char>& storage, Stream<char>* strings, size_t offset = 0);

		// Returns the string enclosed in a set of paranthesis. If there are no paranthesis, it returns { nullptr, 0 }
		// Example AAAA(BBB) returns BBB
		ECSENGINE_API Stream<char> GetStringParameter(Stream<char> string);

		// Returns the string after the count'th character. If there are less than count characters of that type,
		// it will return { nullptr, 0 }
		ECSENGINE_API Stream<char> SkipCharacters(Stream<char> characters, char character, unsigned int count);

		// Returns the string after the count'th token. If there are less than count token of that type,
		// it will return { nullptr, 0 }
		ECSENGINE_API Stream<char> SkipTokens(Stream<char> characters, Stream<char> token, unsigned int count);

		// Returns the "new" string (it will have the same buffer, the size will change only).
		// This is done in place
		ECSENGINE_API Stream<char> RemoveSingleLineComment(Stream<char> characters, Stream<char> token);

		// Returns the "new" string (it will have the same buffer, the size will change only).
		// This is done in place. It can return { nullptr, 0 } if a multi line comment is not properly closed (it will not modify anything)
		ECSENGINE_API Stream<char> RemoveMultiLineComments(Stream<char> characters, Stream<char> open_token, Stream<char> closed_token);

		struct ConditionalPreprocessorDirectives {
			Stream<char> if_directive;
			Stream<char> else_directive;
			Stream<char> elseif_directive;
			Stream<char> end_directive;
		};

		ECSENGINE_API void GetConditionalPreprocessorDirectivesCSource(ConditionalPreprocessorDirectives* directives);

		// Returns the "new" string (it will have the same buffer, the size will change only).
		// This is done in place. It can return { nullptr, 0 } if a directive is not properly closed
		ECSENGINE_API Stream<char> RemoveConditionalPreprocessorBlock(Stream<char> characters, const ConditionalPreprocessorDirectives* directives, Stream<Stream<char>> macros);

		// Removes from the source code of a "C" like file the single line comments, the multi line comments and the conditional blocks
		// Can optionally specify if the defines that are made inside the file should be taken into account
		// Returns the "new" string (it will have the same buffer, the size will change only)
		ECSENGINE_API Stream<char> PreprocessCFile(Stream<char> source_code, Stream<Stream<char>> external_macros = { nullptr, 0 }, bool add_internal_macro_defines = true);

		// If the define_token and the defined_macros are both set, it will retrieve
		// all the macros defined with the given token
		// If the conditional_macros is given, then it will retrieve all the macros
		// which appear in an ifdef_token or elif_token definition.
		// If the allocator is not given, the Stream<char> will reference the characters
		// in the source code. Else they will be allocated individually
		struct SourceCodeMacros {
			Stream<char> define_token = { nullptr, 0 };
			CapacityStream<Stream<char>>* defined_macros = nullptr;

			Stream<char> ifdef_token = { nullptr, 0 };
			Stream<char> elif_token = { nullptr, 0 };
			CapacityStream<Stream<char>>* conditional_macros = nullptr;

			AllocatorPolymorphic allocator = { nullptr };
		};

		inline void GetSourceCodeMacrosCTokens(SourceCodeMacros* macros) {
			macros->define_token = "#define";
			macros->ifdef_token = "#ifdef";
			macros->elif_token = "#elif";
		}

		ECSENGINE_API void GetSourceCodeMacros(
			Stream<char> characters,
			const SourceCodeMacros* options
		);

		// If allocating a stream alongside its data, this function sets it up
		ECSENGINE_API void* CoallesceStreamWithData(void* allocation, size_t size);

		// If allocating a capacity stream alongside its data, this function sets it up
		ECSENGINE_API void* CoallesceCapacityStreamWithData(void* allocation, size_t size, size_t capacity);

		// Returns a stream allocated alongside its data
		ECSENGINE_API void* CoallesceStreamWithData(AllocatorPolymorphic allocator, Stream<void> data, size_t element_size);

		// Verifies if the characters form a valid floating point number: 
		// consisting of at maximum a dot and only number characters and at max a minus or plus as the first character
		ECSENGINE_API bool IsFloatingPointNumber(Stream<char> characters, bool skip_whitespace = false);

		// Verifies if the characters form a valid number: 
		// only number characters and at max a minus or plus as the first character
		ECSENGINE_API bool IsIntegerNumber(Stream<char> characters, bool skip_whitespace = false);

		// If the increment is negative, it will start from the last character to the first
		// and return the value into stream.buffer + stream.size. Example |value   | ->
		// will be returned as |value| -> stream.buffer = 'v', stream.buffer + stream.size = 'e'
		ECSENGINE_API Stream<char> SkipSpaceStream(Stream<char> characters, int increment = 1);
		
		// If the increment is negative, it will start from the last character to the first
		// and return the value into stream.buffer + stream.size. Example |value  \n | ->
		// will be returned as |value| -> stream.buffer = 'v', stream.buffer + stream.size = 'e'
		ECSENGINE_API Stream<char> SkipWhitespace(Stream<char> characters, int increment = 1);

		// If the increment is negative, it will start from the last character to the first
		// and return the value into stream.buffer + stream.size. Example |value  \n | ->
		// will be returned as |value| -> stream.buffer = 'v', stream.buffer + stream.size = 'e'
		ECSENGINE_API Stream<char> SkipWhitespaceEx(Stream<char> characters, int increment = 1);

		// If the increment is negative, it will start from the last character to the first
		// and return the value into stream.buffer + stream.size. Example |hey   value| ->
		// will be returned as |hey   | -> stream.buffer = 'h', stream.buffer + stream.size = ' '
		ECSENGINE_API Stream<char> SkipCodeIdentifier(Stream<char> characters, int increment = 1);

		// It will skip both parameterized and non-parameterized tags
		ECSENGINE_API Stream<char> SkipTag(Stream<char> characters);

		// Returns the string that is delimited by the character in reverse order
		ECSENGINE_API Stream<char> SkipUntilCharacterReverse(const char* string, const char* bound, char character);

		ECSENGINE_API unsigned int GetAlphabetIndex(char character);

		ECSENGINE_API unsigned int GetAlphabetIndex(char character, CharacterType& type);

		ECSENGINE_API void ConvertDateToString(Date date, Stream<char>& characters, size_t format_flags);

		ECSENGINE_API void ConvertDateToString(Date date, CapacityStream<char>& characters, size_t format_flags);

		ECSENGINE_API void ConvertDateToString(Date date, Stream<wchar_t>& characters, size_t format_flags);

		ECSENGINE_API void ConvertDateToString(Date date, CapacityStream<wchar_t>& characters, size_t format_flags);

		ECSENGINE_API Date ConvertStringToDate(Stream<char> characters, size_t format_flags);

		ECSENGINE_API Date ConvertStringToDate(Stream<wchar_t> characters, size_t format_flags);

		ECSENGINE_API void ConvertByteSizeToString(size_t byte_size, Stream<char>& characters);

		ECSENGINE_API void ConvertByteSizeToString(size_t byte_size, CapacityStream<char>& characters);

		ECSENGINE_API void ConvertByteSizeToString(size_t byte_size, Stream<wchar_t>& characters);

		ECSENGINE_API void ConvertByteSizeToString(size_t byte_size, CapacityStream<wchar_t>& characters);

		ECSENGINE_API bool IsDateLater(Date first, Date second);

		// If the pointer is null, it will commit the message
		ECSENGINE_API void SetErrorMessage(CapacityStream<char>* error_message, Stream<char> message);

		// If the pointer is null, it will commit the message
		ECSENGINE_API void SetErrorMessage(CapacityStream<char>* error_message, const char* message);

		ECSENGINE_API void ReplaceCharacter(Stream<char> string, char token_to_be_replaced, char replacement);

		ECSENGINE_API void ReplaceCharacter(Stream<wchar_t> string, wchar_t token_to_be_replaced, wchar_t replacement);

		ECSENGINE_API Stream<char> ReplaceToken(Stream<char> string, Stream<char> token, Stream<char> replacement, AllocatorPolymorphic allocator);

		ECSENGINE_API Stream<wchar_t> ReplaceToken(Stream<wchar_t> string, Stream<wchar_t> token, Stream<wchar_t> replacement, AllocatorPolymorphic allocator);

		template<typename CharacterType>
		struct ReplaceOccurence {
			Stream<CharacterType> string;
			Stream<CharacterType> replacement;
		};

		// If the output_string is nullptr, it will do the replacement in-place
		// Else it will use the output string
		ECSENGINE_API void ReplaceOccurrences(CapacityStream<char>& string, Stream<ReplaceOccurence<char>> occurences, CapacityStream<char>* output_string = nullptr);

		// If the output_string is nullptr, it will do the replacement in-place
		// Else it will use the output string
		ECSENGINE_API void ReplaceOccurences(CapacityStream<wchar_t>& string, Stream<ReplaceOccurence<wchar_t>> occurences, CapacityStream<wchar_t>* output_string = nullptr);

		// These splits will only reference the content inside the string
		ECSENGINE_API void SplitString(Stream<char> string, char delimiter, CapacityStream<Stream<char>>& splits);
		
		// These splits will only reference the content inside the string
		ECSENGINE_API void SplitString(Stream<wchar_t> string, wchar_t delimiter, CapacityStream<Stream<wchar_t>>& splits);

		// These splits will only reference the content inside the string
		ECSENGINE_API void SplitString(Stream<char> string, Stream<char> delimiter, CapacityStream<Stream<char>>& splits);

		// These splits will only reference the content inside the string
		ECSENGINE_API void SplitString(Stream<wchar_t> string, Stream<wchar_t> delimiter, CapacityStream<Stream<wchar_t>>& splits);

		// Returns the string isolated from other strings delimited using the given delimiter
		ECSENGINE_API Stream<char> IsolateString(Stream<char> string, Stream<char> token, Stream<char> delimiter);

		// Returns the string isolated from other strings delimited using the given delimiter
		ECSENGINE_API Stream<wchar_t> IsolateString(Stream<wchar_t> string, Stream<wchar_t> token, Stream<wchar_t> delimiter);

		enum ECS_EVALUATE_EXPRESSION_OPERATORS : unsigned char {
			ECS_EVALUATE_EXPRESSION_ADD,
			ECS_EVALUATE_EXPRESSION_MINUS,
			ECS_EVALUATE_EXPRESSION_MULTIPLY,
			ECS_EVALUATE_EXPRESSION_DIVIDE,
			ECS_EVALUATE_EXPRESSION_MODULO,
			ECS_EVALUATE_EXPRESSION_NOT,
			ECS_EVALUATE_EXPRESSION_AND,
			ECS_EVALUATE_EXPRESSION_OR,
			ECS_EVALUATE_EXPRESSION_XOR,
			ECS_EVALUATE_EXPRESSION_SHIFT_LEFT,
			ECS_EVALUATE_EXPRESSION_SHIFT_RIGHT,
			ECS_EVALUATE_EXPRESSION_LOGICAL_AND,
			ECS_EVALUATE_EXPRESSION_LOGICAL_OR,
			ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY,
			ECS_EVALUATE_EXPRESSION_LOGICAL_INEQUALITY,
			ECS_EVALUATE_EXPRESSION_LOGICAL_NOT,
			ECS_EVALUATE_EXPRESSION_LESS,
			ECS_EVALUATE_EXPRESSION_LESS_EQUAL,
			ECS_EVALUATE_EXPRESSION_GREATER,
			ECS_EVALUATE_EXPRESSION_GREATER_EQUAL,
			ECS_EVALUATE_EXPRESSION_OPERATOR_COUNT
		};

		struct EvaluateExpressionOperator {
			ECS_EVALUATE_EXPRESSION_OPERATORS operator_index;
			unsigned int index;
		};

		struct EvaluateExpressionNumber {
			double value;
			unsigned int index;
		};

		// logical operators except not it returns only the operator as single
		ECSENGINE_API ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperator(char character);

		// logical operators except not it returns only the operator as single
		ECSENGINE_API ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperator(wchar_t character);

		// It reports correctly the logical operators. It increments the index if a double operator is found
		ECSENGINE_API ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperator(char character, char next_character, unsigned int& index);

		// It reports correctly the logical operators. It increments the index if a double operator is found
		ECSENGINE_API ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperator(wchar_t character, wchar_t next_character, unsigned int& index);

		// Fills in the operators and their indices. For double operators (like logical + shifts) it reporst the second character
		ECSENGINE_API void GetEvaluateExpressionOperators(Stream<char> characters, CapacityStream<EvaluateExpressionOperator>& operators);

		// Fills in the operators and their indices. For double operators (like logical + shifts) it reporst the second character
		ECSENGINE_API void GetEvaluateExpressionOperators(Stream<wchar_t> characters, CapacityStream<EvaluateExpressionOperator>& operators);

		// Fills in a buffer with the order in which the operators must be evaluated
		ECSENGINE_API void GetEvaluateExpressionOperatorOrder(Stream<EvaluateExpressionOperator> operators, CapacityStream<unsigned int>& order);

		// If the character at the offset is not a value character, it incremenets the offset and returns DBL_MAX
		// Else it parses the value and incremenets the offset right after the value has ended
		ECSENGINE_API double EvaluateExpressionValue(Stream<char> characters, unsigned int& offset);

		// If the character at the offset is not a value character, it incremenets the offset and returns DBL_MAX
		// Else it parses the value and incremenets the offset right after the value has ended
		ECSENGINE_API double EvaluateExpressionValue(Stream<wchar_t> characters, unsigned int& offset);

		ECSENGINE_API void GetEvaluateExpressionNumbers(Stream<char> characters, CapacityStream<EvaluateExpressionNumber>& numbers);

		ECSENGINE_API void GetEvaluateExpressionNumbers(Stream<wchar_t> characters, CapacityStream<EvaluateExpressionNumber>& numbers);

		// For single operators, the right needs to be specified, the left can be missing
		ECSENGINE_API double EvaluateOperator(ECS_EVALUATE_EXPRESSION_OPERATORS operator_, double left, double right);

		// Returns the value of the constant expression
		ECSENGINE_API double EvaluateExpression(Stream<char> characters);

		// Returns the value of the constant expression
		ECSENGINE_API double EvaluateExpression(Stream<wchar_t> characters);

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

	}

}

