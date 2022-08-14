#pragma once
#include "ecspch.h"
#include "../Core.h"
#include "../Containers/Stream.h"
#include "BasicTypes.h"
#include "Assert.h"
#include "../Allocators/AllocatorTypes.h"

#define ECS_ASSERT_TRIGGER

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

		template<typename CharacterType>
		CharacterType Character(char character) {
			if constexpr (std::is_same_v<CharacterType, char>) {
				return character;
			}
			else if constexpr (std::is_same_v<CharacterType, wchar_t>) {
				return (wchar_t)((int)L'\0' + (int)character);
			}
		}

		inline bool IsPowerOfTwo(int x) {
			return (x & (x - 1)) == 0;
		}

		inline uintptr_t AlignPointer(uintptr_t pointer, size_t alignment) {
			ECS_ASSERT(IsPowerOfTwo(alignment));

			size_t mask = alignment - 1;
			return (pointer + mask) & ~mask;
		}

		/* Supports alignments up to 256 bytes */
		inline uintptr_t AlignPointerStack(uintptr_t pointer, size_t alignment) {
			uintptr_t first_aligned_pointer = AlignPointer(pointer, alignment);
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

		// Extends the 47th bit into the 48-63 range
		inline void* SignExtendPointer(const void* pointer) {
			intptr_t ptr = (intptr_t)pointer;
			ptr <<= 16;
			ptr >>= 16;
			return (void*)ptr;
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
			Stream<wchar_t> wide_chars,
			CapacityStream<char>& ascii_chars
		) {
			size_t written_chars = 0;
			errno_t status = wcstombs_s(&written_chars, ascii_chars.buffer + ascii_chars.size, ascii_chars.capacity - ascii_chars.size, wide_chars.buffer, wide_chars.size);
			ECS_ASSERT(status == 0);
			ascii_chars.size += written_chars - 1;
		}

		// it searches for spaces and next line characters
		ECSENGINE_API size_t ParseWordsFromSentence(Stream<char> sentence, char separator_token = ' ');

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

		// finds the tokens that appear in the current string
		ECSENGINE_API void FindToken(Stream<char> string, char token, CapacityStream<unsigned int>& tokens);

		// finds the tokens that appear in the current string
		ECSENGINE_API void FindToken(Stream<char> string, Stream<char> token, CapacityStream<unsigned int>& tokens);

		ECSENGINE_API void FindToken(Stream<wchar_t> string, wchar_t token, CapacityStream<unsigned int>& tokens);

		ECSENGINE_API void FindToken(Stream<wchar_t> string, Stream<wchar_t> token, CapacityStream<unsigned int>& tokens);

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

		// It will search from the end of the characters string till its start
		// It uses SIMD to speed up the find
		ECSENGINE_API Stream<char> FindTokenReverse(Stream<char> characters, Stream<char> token);

		// It will search from the end of the characters string till its start
		// It uses SIMD to speed up the find
		ECSENGINE_API Stream<wchar_t> FindTokenReverse(Stream<wchar_t> characters, Stream<wchar_t> token);

		// It will search the string from ending character until lower bound
		ECSENGINE_API Stream<char> FindCharacterReverse(Stream<char> characters, char character);

		inline void Capitalize(char* character) {
			if (*character >= 'a' && *character <= 'z') {
				*character = *character - 32;
			}
		}

		// Used to copy data into a pointer passed by its address
		struct CopyPointer {
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

		inline bool HasFlag(size_t configuration, size_t flag) {
			return (configuration & flag) != 0;
		}

		// We're not using any macros or variadic templates. Just write up to 5 args
		inline size_t HasFlag(size_t configuration, size_t flag0, size_t flag1) {
			return HasFlag(HasFlag(configuration, flag0), flag1);
		}

		inline size_t HasFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2) {
			return HasFlag(HasFlag(configuration, flag0), flag1, flag2);
		}

		inline size_t HasFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2, size_t flag3) {
			return HasFlag(HasFlag(configuration, flag0), flag1, flag2, flag3);
		}

		inline size_t HasFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2, size_t flag3, size_t flag4) {
			return HasFlag(HasFlag(configuration, flag0), flag1, flag2, flag3, flag4);
		}

		inline bool HasFlagAtomic(const std::atomic<size_t>& configuration, size_t flag) {
			return (configuration.load(std::memory_order_relaxed) & flag) != 0;
		}

		inline size_t ClearFlag(size_t configuration, size_t flag) {
			return configuration & (~flag);
		}

		// We're not using any macros or variadic templates. Just write up to 5 args
		inline size_t ClearFlag(size_t configuration, size_t flag0, size_t flag1) {
			return ClearFlag(ClearFlag(configuration, flag0), flag1);
		}

		inline size_t ClearFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2) {
			return ClearFlag(ClearFlag(configuration, flag0), flag1, flag2);
		}

		inline size_t ClearFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2, size_t flag3) {
			return ClearFlag(ClearFlag(configuration, flag0), flag1, flag2, flag3);
		}

		inline size_t ClearFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2, size_t flag3, size_t flag4) {
			return ClearFlag(ClearFlag(configuration, flag0), flag1, flag2, flag3, flag4);
		}

		inline void ClearFlagAtomic(std::atomic<size_t>& configuration, size_t flag) {
			configuration.fetch_and(~flag, std::memory_order_relaxed);
		}

		inline size_t SetFlag(size_t configuration, size_t flag) {
			return configuration | flag;
		}

		// We're not using any macros or variadic templates. Just write up to 5 args
		inline size_t SetFlag(size_t configuration, size_t flag0, size_t flag1) {
			return SetFlag(SetFlag(configuration, flag0), flag1);
		}

		inline size_t SetFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2) {
			return SetFlag(SetFlag(configuration, flag0), flag1, flag2);
		}

		inline size_t SetFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2, size_t flag3) {
			return SetFlag(SetFlag(configuration, flag0), flag1, flag2, flag3);
		}

		inline size_t SetFlag(size_t configuration, size_t flag0, size_t flag1, size_t flag2, size_t flag3, size_t flag4) {
			return SetFlag(SetFlag(configuration, flag0), flag1, flag2, flag3, flag4);
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

		// It will remap the pointer from the first base into the second base
		ECSENGINE_API void* RemapPointer(const void* first_base, const void* second_base, const void* pointer);

		ECSENGINE_API unsigned int FindString(const char* ECS_RESTRICT string, Stream<const char*> other);

		ECSENGINE_API unsigned int FindString(Stream<char> string, Stream<Stream<char>> other);

		ECSENGINE_API unsigned int FindString(const wchar_t* ECS_RESTRICT string, Stream<const wchar_t*> other);

		ECSENGINE_API unsigned int FindString(Stream<wchar_t> string, Stream<Stream<wchar_t>> other);

		inline void* OffsetPointer(const void* pointer, int64_t offset) {
			return (void*)((int64_t)pointer + offset);
		}

		inline void* OffsetPointer(Stream<void> pointer) {
			return OffsetPointer(pointer.buffer, pointer.size);
		}

		inline void* OffsetPointer(CapacityStream<void> pointer) {
			return OffsetPointer(pointer.buffer, pointer.size);
		}

		// a - b
		inline size_t PointerDifference(const void* a, const void* b) {
			return (uintptr_t)a - (uintptr_t)b;
		}

		// Returns true if the pointer >= base && pointer < base + size
		inline bool IsPointerRange(const void* base, size_t size, const void* pointer) {
			return pointer >= base && PointerDifference(pointer, base) < size;
		}

		inline void* RemapPointerIfInRange(const void* base, size_t size, const void* new_base, const void* pointer) {
			if (IsPointerRange(base, size, pointer)) {
				return RemapPointer(base, new_base, pointer);
			}
			return (void*)pointer;
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

		// Can use the increment to go backwards by setting it to -1
		inline const char* SkipSpace(const char* pointer, int increment = 1) {
			while (*pointer == ' ') {
				pointer += increment;
			}
			return pointer;
		}

		// If the increment is negative, it will start from the last character to the first
		// and return the value into stream.buffer + stream.size. Example |value   | ->
		// will be returned as |value| -> stream.buffer = 'v', stream.buffer + stream.size = 'e'
		ECSENGINE_API Stream<char> SkipSpaceStream(Stream<char> characters, int increment = 1);

		// Tabs and spaces
		// Can use the increment to go backwards by setting it to -1
		inline const char* SkipWhitespace(const char* pointer, int increment = 1) {
			while (*pointer == ' ' || *pointer == '\t') {
				pointer += increment;
			}
			return pointer;
		}

		// Can use the increment to go backwards by setting it to -1
		inline const wchar_t* SkipWhitespace(const wchar_t* pointer, int increment = 1) {
			while (*pointer == L' ' || *pointer == L'\t') {
				pointer += increment;
			}
			return pointer;
		}
		
		// If the increment is negative, it will start from the last character to the first
		// and return the value into stream.buffer + stream.size. Example |value  \n | ->
		// will be returned as |value| -> stream.buffer = 'v', stream.buffer + stream.size = 'e'
		ECSENGINE_API Stream<char> SkipWhitespace(Stream<char> characters, int increment = 1);

		// Can use the increment to go backwards by setting it to -1
		inline const char* SkipCodeIdentifier(const char* pointer, int increment = 1) {
			while (IsCodeIdentifierCharacter(*pointer)) {
				pointer += increment;
			}
			return pointer;
		}

		// If the increment is negative, it will start from the last character to the first
		// and return the value into stream.buffer + stream.size. Example |hey   value| ->
		// will be returned as |hey   | -> stream.buffer = 'h', stream.buffer + stream.size = ' '
		ECSENGINE_API Stream<char> SkipCodeIdentifier(Stream<char> characters, int increment = 1);

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

		// The value will not be less than min
		template<typename Value>
		Value ClampMin(Value value, Value min) {
			return value < min ? min : value;
		}

		// The value will not be greater than max
		template<typename Value>
		Value ClampMax(Value value, Value max) {
			return value > max ? max : value;
		}

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

	}

}

