#pragma once
#include "ecspch.h"
#include "../Core.h"
#include "../Containers/Stream.h"
#include "BasicTypes.h"
#include "Assert.h"
#include "../Allocators/AllocatorTypes.h"

#define ECS_ASSERT_TRIGGER

#define ECS_TEMP_ASCII_STRING(name, size) char name##_temp_memory[size]; \
ECSEngine::containers::CapacityStream<char> name(name##_temp_memory, 0, size);

#define ECS_TEMP_STRING(name, size) wchar_t name##_temp_memory[size]; \
ECSEngine::containers::CapacityStream<wchar_t> name(name##_temp_memory, 0, size);

namespace ECSEngine {

	constexpr size_t ECS_LOCAL_TIME_FORMAT_HOUR = 1 << 0;
	constexpr size_t ECS_LOCAL_TIME_FORMAT_MINUTES = 1 << 1;
	constexpr size_t ECS_LOCAL_TIME_FORMAT_SECONDS = 1 << 2;
	constexpr size_t ECS_LOCAL_TIME_FORMAT_MILLISECONDS = 1 << 3;
	constexpr size_t ECS_LOCAL_TIME_FORMAT_DAY = 1 << 4;
	constexpr size_t ECS_LOCAL_TIME_FORMAT_MONTH = 1 << 5;
	constexpr size_t ECS_LOCAL_TIME_FORMAT_YEAR = 1 << 6;
	constexpr size_t ECS_LOCAL_TIME_FORMAT_ALL = ECS_LOCAL_TIME_FORMAT_HOUR | ECS_LOCAL_TIME_FORMAT_MINUTES | ECS_LOCAL_TIME_FORMAT_SECONDS
		| ECS_LOCAL_TIME_FORMAT_MILLISECONDS | ECS_LOCAL_TIME_FORMAT_DAY | ECS_LOCAL_TIME_FORMAT_MONTH | ECS_LOCAL_TIME_FORMAT_YEAR;

	namespace function {

		ECS_CONTAINERS;

		inline bool is_power_of_two(int x) {
			return (x & (x - 1)) == 0;
		}

		inline uintptr_t align_pointer(uintptr_t pointer, size_t alignment) {
			ECS_ASSERT(is_power_of_two(alignment));

			size_t mask = alignment - 1;
			return (pointer + mask) & ~mask;
		}

		/* Supports alignments up to 256 bytes */
		inline uintptr_t align_pointer_stack(uintptr_t pointer, size_t alignment) {
			ECS_ASSERT(is_power_of_two(alignment));

			uintptr_t first_aligned_pointer = align_pointer(pointer, alignment);
			return first_aligned_pointer + alignment * ((first_aligned_pointer - pointer) == 0);
		}

		// The x component contains the actual value and the y component the power
		inline ulong2 PowerOfTwoGreater(size_t number) {
			size_t count = 0;
			size_t value = 1;
			while (value <= number) {
				value <<= 1;
				count++;
			}
			return {value, count};
		}

		// pointers should be aligned preferably to 32 bytes at least
		ECSENGINE_API void avx2_copy(void* destination, const void* source, size_t bytes);

		inline void ConvertASCIIToWide(wchar_t* wide_string, const char* pointer, size_t max_w_string_count) {
			int result = MultiByteToWideChar(CP_ACP, 0, pointer, -1, wide_string, max_w_string_count);
		}

		inline void ConvertASCIIToWide(wchar_t* wide_string, Stream<char> pointer, size_t max_w_string_count) {
			int result = MultiByteToWideChar(CP_ACP, 0, pointer.buffer, pointer.size, wide_string, max_w_string_count);
		}

		inline void ConvertASCIIToWide(CapacityStream<wchar_t>& wide_string, Stream<char> ascii_string) {
			int result = MultiByteToWideChar(CP_ACP, 0, ascii_string.buffer, ascii_string.size, wide_string.buffer + wide_string.size, wide_string.capacity);
			wide_string.size += ascii_string.size;
		}

		inline void ConvertASCIIToWide(CapacityStream<wchar_t>& wide_string, CapacityStream<char> ascii_string) {
			int result = MultiByteToWideChar(CP_ACP, 0, ascii_string.buffer, ascii_string.size, wide_string.buffer + wide_string.size, wide_string.capacity);
			wide_string.size += ascii_string.size;
		}

		inline void ConcatenateCharPointers(
			char* pointer_to_store, 
			const char* pointer_to_add, 
			size_t offset, 
			size_t size_of_pointer_to_add = 0
		) {
			memcpy(pointer_to_store + offset, pointer_to_add, size_of_pointer_to_add == 0 ? strlen(pointer_to_add) : size_of_pointer_to_add);
		}

		inline void ConcatenateWideCharPointers(
			wchar_t* pointer_to_store,
			const wchar_t* pointer_to_add,
			size_t offset,
			size_t size_of_pointer_to_add = 0
		) {
			memcpy(pointer_to_store + offset, pointer_to_add, (size_of_pointer_to_add == 0 ? wcsnlen_s(pointer_to_add, 4096) : size_of_pointer_to_add) * sizeof(wchar_t));
		}

		ECSENGINE_API void CheckWindowsFunctionErrorCode(HRESULT hr, LPCWSTR box_name, const char* filename, unsigned int line, bool do_exit);
#define ECS_CHECK_WINDOWS_FUNCTION_ERROR_CODE(hr, box_name, do_exit) function::CheckWindowsFunctionErrorCode(hr, box_name, __FILE__, __LINE__, do_exit)

		// returns the count of decoded numbers
		ECSENGINE_API size_t ParseNumbersFromCharString(Stream<char> character_buffer, unsigned int* number_buffer);

		// returns the count of decoded numbers
		ECSENGINE_API size_t ParseNumbersFromCharString(Stream<char> character_buffer, int* number_buffer);

		inline void ConvertWideCharsToASCII(
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

		inline void ConvertWideCharsToASCII(
			const wchar_t* wide_chars,
			char* chars,
			size_t wide_char_count,
			size_t max_char_count
		) {
			size_t written_chars = 0;
			errno_t status = wcstombs_s(&written_chars, chars, max_char_count, wide_chars, wide_char_count);
			ECS_ASSERT(status == 0);
		}

		inline void ConvertWideCharsToASCII(
			CapacityStream<wchar_t> wide_chars,
			CapacityStream<char>& ascii_chars
		) {
			size_t written_chars = 0;
			errno_t status = wcstombs_s(&written_chars, ascii_chars.buffer + ascii_chars.size, ascii_chars.capacity - ascii_chars.size, wide_chars.buffer, wide_chars.size);
			ECS_ASSERT(status == 0);
			ascii_chars.size += written_chars - 1;
		}

		inline void ConvertWideCharsToASCII(
			Stream<wchar_t> wide_chars,
			CapacityStream<char>& ascii_chars
		) {
			size_t written_chars = 0;
			errno_t status = wcstombs_s(&written_chars, ascii_chars.buffer + ascii_chars.size, ascii_chars.capacity - ascii_chars.size, wide_chars.buffer, wide_chars.size);
			ECS_ASSERT(status == 0);
			ascii_chars.size += written_chars - 1;
		}

		// it searches for spaces and next line characters
		ECSENGINE_API size_t ParseWordsFromSentence(const char* sentence);

		// positions will be filled with the 4 corners of the rectangle
		ECSENGINE_API void ObliqueRectangle(float2* positions, float2 a, float2 b, float thickness);

		constexpr ECS_INLINE float CalculateFloatPrecisionPower(size_t precision) {
			float value = 1.0f;
			for (size_t index = 0; index < precision; index++) {
				value *= 10.0f;
			}
			return value;
		}

		constexpr ECS_INLINE double CalculateDoublePrecisionPower(size_t precision) {
			double value = 1.0f;
			for (size_t index = 0; index < precision; index++) {
				value *= 10.0f;
			}
			return value;
		}
		
		// duration should be expressed as milliseconds
		ECSENGINE_API void ConvertDurationToChars(size_t duration, char* characters);

		const char* FindTokenReverse(const char* ECS_RESTRICT characters, const char* ECS_RESTRICT token);

		inline void Capitalize(char* character) {
			if (*character >= 'a' && *character <= 'z') {
				*character = *character - 32;
			}
		}

		// Used to copy data into a pointer passed by its address
		struct ECSENGINE_API CopyPointer {
			void** destination;
			void* data;
			size_t data_size;
		};

		inline void CopyPointers(Stream<CopyPointer> copy_data) {
			for (size_t index = 0; index < copy_data.size; index++) {
				memcpy(*copy_data[index].destination, copy_data[index].data, copy_data[index].data_size);
			}
		}

		// Given a fresh buffer, it will set the destinations accordingly
		inline void CopyPointersFromBuffer(Stream<CopyPointer> copy_data, void* data) {
			uintptr_t ptr = (uintptr_t)data;
			for (size_t index = 0; index < copy_data.size; index++) {
				*copy_data[index].destination = (void*)ptr;
				memcpy((void*)ptr, copy_data[index].data, copy_data[index].data_size);
				ptr += copy_data[index].data_size;
			}
		}

		constexpr bool HasFlag(size_t configuration, size_t flag) {
			return (configuration & flag) != 0;
		}

		inline bool HasFlagAtomic(const std::atomic<size_t>& configuration, size_t flag) {
			return (configuration.load(std::memory_order_relaxed) & flag) != 0;
		}

		constexpr size_t ClearFlag(size_t configuration, size_t flag) {
			return configuration & (~flag);
		}

		inline void ClearFlagAtomic(std::atomic<size_t>& configuration, size_t flag) {
			configuration.fetch_and(~flag, std::memory_order_relaxed);
		}

		constexpr size_t SetFlag(size_t configuration, size_t flag) {
			return configuration | flag;
		}

		inline void SetFlagAtomic(std::atomic<size_t>& configuration, size_t flag) {
			configuration.fetch_or(flag, std::memory_order_relaxed);
		}

		inline bool CompareStrings(Stream<wchar_t> string, Stream<wchar_t> other) {
			return string.size == other.size && (memcmp(string.buffer, other.buffer, sizeof(wchar_t) * other.size) == 0);
		}

		inline bool CompareStrings(Stream<char> string, Stream<char> other) {
			return string.size == other.size && (memcmp(string.buffer, other.buffer, sizeof(char) * other.size) == 0);
		}

		inline bool CompareStrings(const wchar_t* ECS_RESTRICT string, const wchar_t* ECS_RESTRICT other) {
			size_t string_size = wcslen(string);
			size_t other_size = wcslen(other);
			return CompareStrings(Stream<wchar_t>(string, string_size), Stream<wchar_t>(other, other_size));
		}

		inline bool CompareStrings(const char* ECS_RESTRICT string, const char* ECS_RESTRICT other) {
			size_t string_size = strlen(string);
			size_t other_size = strlen(string);
			return CompareStrings(Stream<char>(string, string_size), Stream<char>(other, other_size));
		}

		ECSENGINE_API unsigned int FindString(const char* ECS_RESTRICT string, Stream<const char*> other);

		ECSENGINE_API unsigned int FindString(Stream<char> string, Stream<Stream<char>> other);

		ECSENGINE_API unsigned int FindString(const wchar_t* ECS_RESTRICT string, Stream<const wchar_t*> other);

		ECSENGINE_API unsigned int FindString(Stream<wchar_t> string, Stream<Stream<wchar_t>> other);

		inline void* OffsetPointer(const void* pointer, size_t offset) {
			return (void*)((uintptr_t)pointer + offset);
		}

		inline void* OffsetPointer(Stream<void> pointer) {
			return OffsetPointer(pointer.buffer, pointer.size);
		}

		inline void* OffsetPointer(CapacityStream<void> pointer) {
			return OffsetPointer(pointer.buffer, pointer.size);
		}

		// If data size is zero, it will return data, else it will make a copy and return that instead
		template<typename Allocator>
		void* AllocateMemory(Allocator* allocator, void* data, size_t data_size, size_t alignment = 8) {
			if (data_size > 0) {
				void* allocation = allocator->Allocate(data_size, alignment);
				memcpy(allocation, data, data_size);
				return allocation;
			}
			return data;
		}

		// If allocating a stream alongside its data, this function sets it up
		ECSENGINE_API void* CoallesceStreamWithData(void* allocation, size_t size);

		// If allocating a capacity stream alongside its data, this function sets it up
		ECSENGINE_API void* CoallesceCapacityStreamWithData(void* allocation, size_t size, size_t capacity);

		// Verifies if the characters form a valid floating point number: 
		// consisting of at maximum a dot and only number characters and at max a minus or plus as the first character
		ECSENGINE_API bool IsFloatingPointNumber(Stream<char> characters);

		// Verifies if the characters form a valid number: 
		// only number characters and at max a minus or plus as the first character
		ECSENGINE_API bool IsIntegerNumber(Stream<char> characters);

		inline bool IsNumberCharacter(char value) {
			return value >= '0' && value <= '9';
		}

		inline bool IsAlphabetCharacter(char value) {
			return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
		}

		inline bool IsCodeIdentifierCharacter(char value) {
			return IsNumberCharacter(value) || IsAlphabetCharacter(value) || value == '_';
		}

		inline const char* SkipSpace(const char* pointer) {
			while (*pointer == ' ') {
				pointer++;
			}
			return pointer;
		}

		// Tabs and spaces
		inline const char* SkipWhitespace(const char* pointer) {
			while (*pointer == ' ' || *pointer == '\t') {
				pointer++;
			}
			return pointer;
		}
		
		inline const char* SkipCodeIdentifier(const char* pointer) {
			while (IsCodeIdentifierCharacter(*pointer)) {
				pointer++;
			}
			return pointer;
		}

		// Shifts the pointer 3 positions to the right in order to provide significant digits for hashing functions
		// like power of two that use the lower bits in order to hash the element inside the table.
		// It will clip to only 24 bits - 3 bytes - that's the precision the hash table work with
		inline unsigned int PointerHash(void* ptr) {
			return (unsigned int)(((uintptr_t)ptr >> 3) & 0x0000000000FFFFFF);
		}

		inline size_t GetSimdCount(size_t count, size_t vector_size) {
			return count & (-vector_size);
		}

		template<bool is_delta = false, typename Value>
		Value Lerp(Value a, Value b, float percentage) {
			if constexpr (!is_delta) {
				return (b - a) * percentage + a;
			}
			else {
				return b * percentage + a;
			}
		}

		template<typename Value>
		float InverseLerp(Value a, Value b, Value c) {
			return (c - a) / (b - a);
		}

		template<typename Value>
		Value PlanarLerp(Value a, Value b, Value c, Value d, float x_percentage, float y_percentage) {
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
		float2 SampleFunction(float value, Function&& function) {
			return { value, function(value) };
		}

		template<typename Value>
		Value Clamp(Value value, Value min, Value max) {
			return ClampMax(ClampMin(value, min), max);
		}

		template<typename Value>
		Value ClampMin(Value value, Value min) {
			return value < min ? min : value;
		}

		template<typename Value>
		Value ClampMax(Value value, Value max) {
			return value > max ? max : value;
		}

		ECSENGINE_API unsigned int GetAlphabetIndex(char character);

		ECSENGINE_API unsigned int GetAlphabetIndex(char character, CharacterType& type);

		ECSENGINE_API void ConvertDateToString(Date date, Stream<char>& characters, size_t format_flags);

		ECSENGINE_API void ConvertDateToString(Date date, CapacityStream<char>& characters, size_t format_flags);

		// If the pointer is null, it will commit the message
		ECSENGINE_API void SetErrorMessage(CapacityStream<char>* error_message, Stream<char> message);

		// If the pointer is null, it will commit the message
		ECSENGINE_API void SetErrorMessage(CapacityStream<char>* error_message, const char* message);

		// It will first select a slice inside the XZ plane using the rotation given by the x component
		// Then it will rotate in the plane determined by the diameter of that rotation and the Y axis
		// The point is centered around the origin, if a translation is disired, it should be added afterwards
		// Rotation is in degrees
		ECSENGINE_API float3 OrbitPoint(float radius, float2 rotation);

	}

}

