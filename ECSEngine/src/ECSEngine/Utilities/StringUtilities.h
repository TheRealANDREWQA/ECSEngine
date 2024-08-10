#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "BasicTypes.h"
#include "Assert.h"

namespace ECSEngine {

	enum ECS_FORMAT_DATE_FLAGS : size_t {
		ECS_FORMAT_DATE_MILLISECONDS = 1 << 0,
		ECS_FORMAT_DATE_SECONDS = 1 << 1,
		ECS_FORMAT_DATE_MINUTES = 1 << 2,
		ECS_FORMAT_DATE_HOUR = 1 << 3,
		ECS_FORMAT_DATE_DAY = 1 << 4,
		ECS_FORMAT_DATE_MONTH = 1 << 5,
		ECS_FORMAT_DATE_YEAR = 1 << 6,
		ECS_FORMAT_DATE_COLON_INSTEAD_OF_DASH = 1 << 7,

		ECS_FORMAT_DATE_ALL = ECS_FORMAT_DATE_MILLISECONDS | ECS_FORMAT_DATE_SECONDS | ECS_FORMAT_DATE_MINUTES
			| ECS_FORMAT_DATE_HOUR | ECS_FORMAT_DATE_DAY | ECS_FORMAT_DATE_MONTH | ECS_FORMAT_DATE_YEAR,
		ECS_FORMAT_DATE_ALL_FROM_MINUTES = ECS_FORMAT_DATE_MINUTES | ECS_FORMAT_DATE_HOUR | ECS_FORMAT_DATE_DAY 
			| ECS_FORMAT_DATE_MONTH | ECS_FORMAT_DATE_YEAR
	};

	ECS_ENUM_BITWISE_OPERATIONS(ECS_FORMAT_DATE_FLAGS)

#define ECS_FORMAT_TEMP_STRING(string_name, base_characters, ...) ECS_STACK_CAPACITY_STREAM(char, string_name, 2048); \
FormatString(string_name, base_characters, __VA_ARGS__);

#define ECS_FORMAT_ERROR_MESSAGE(error_message, base_characters, ...) if (error_message != nullptr) { \
	FormatString(*error_message, base_characters, __VA_ARGS__); \
}

#define ECS_FORMAT_SPECIFIER "{#}"

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

	ECS_INLINE int StringLexicographicCompare(Stream<char> left, Stream<char> right) {
		size_t smaller_size = min(left.size, right.size);
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

	// It will copy the null termination character
	ECS_INLINE Stream<char> StringCopy(AllocatorPolymorphic allocator, Stream<char> string, DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (string.size > 0) {
			Stream<char> result = { Allocate(allocator, string.MemoryOf(string.size + 1), alignof(char), debug_info), string.size };
			result.CopyOther(string);
			result[string.size] = '\0';
			return result;
		}
		return { nullptr, 0 };
	}

	// It will copy the null termination character
	ECS_INLINE Stream<wchar_t> StringCopy(AllocatorPolymorphic allocator, Stream<wchar_t> string, DebugInfo debug_info = ECS_DEBUG_INFO) {
		if (string.size > 0) {
			Stream<wchar_t> result = { Allocate(allocator, string.MemoryOf(string.size + 1), alignof(wchar_t), debug_info), string.size };
			result.CopyOther(string);
			result[string.size] = L'\0';
			return result;
		}
		return { nullptr, 0 };
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

	ECSENGINE_API bool IsHexChar(char value);

	ECSENGINE_API bool IsHexChar(wchar_t value);

	// Can use the increment to go backwards by setting it to -1
	ECS_INLINE const char* SkipSpace(const char* pointer, int increment = 1) {
		while (*pointer == ' ') {
			pointer += increment;
		}
		return pointer;
	}

	ECSENGINE_API void ConvertASCIIToWide(wchar_t* wide_string, const char* pointer, size_t max_w_string_count);

	ECSENGINE_API void ConvertASCIIToWide(wchar_t* wide_string, Stream<char> pointer, size_t max_w_string_count);

	ECSENGINE_API void ConvertASCIIToWide(CapacityStream<wchar_t>& wide_string, Stream<char> ascii_string);

	ECSENGINE_API void ConvertASCIIToWide(CapacityStream<wchar_t>& wide_string, CapacityStream<char> ascii_string);

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

	ECSENGINE_API void ConvertWideCharsToASCII(
		const wchar_t* wide_chars,
		char* chars,
		size_t wide_char_count,
		size_t destination_size,
		size_t max_char_count,
		size_t& written_chars
	);

	ECSENGINE_API void ConvertWideCharsToASCII(
		const wchar_t* wide_chars,
		char* chars,
		size_t wide_char_count,
		size_t max_char_count
	);

	ECSENGINE_API void ConvertWideCharsToASCII(
		Stream<wchar_t> wide_chars,
		CapacityStream<char>& ascii_chars
	);

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

	// it searches for spaces and next line characters
	ECSENGINE_API size_t ParseWordsFromSentence(Stream<char> sentence, char separator_token = ' ');

	// Duration should be expressed as milliseconds
		// Returns how many characters were written
	ECSENGINE_API size_t ConvertDurationToChars(size_t duration_milliseconds, char* characters);

	// finds the tokens that appear in the current string
	ECSENGINE_API void FindToken(Stream<char> string, char token, AdditionStream<unsigned int> tokens);

	// finds the tokens that appear in the current string
	ECSENGINE_API void FindToken(Stream<char> string, Stream<char> token, AdditionStream<unsigned int> tokens);

	ECSENGINE_API void FindToken(Stream<wchar_t> string, wchar_t token, AdditionStream<unsigned int> tokens);

	ECSENGINE_API void FindToken(Stream<wchar_t> string, Stream<wchar_t> token, AdditionStream<unsigned int> tokens);

	// Convenience function - the more efficient is the unsigned int version that returns offsets into the string
	ECSENGINE_API void FindToken(Stream<char> string, Stream<char> token, AdditionStream<Stream<char>> tokens);

	// Convenience function - the more efficient is the unsigned int version that returns offsets into the string
	ECSENGINE_API void FindToken(Stream<wchar_t> string, Stream<wchar_t> token, AdditionStream<Stream<wchar_t>> tokens);

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

	// The functor needs to return the string from the type
	template<typename Type, typename CharacterType, typename Functor>
	ECS_INLINE unsigned int FindString(Stream<CharacterType> string, Stream<Type> other, Functor&& functor) {
		for (unsigned int index = 0; index < other.size; index++) {
			if (string == functor(other[index])) {
				return index;
			}
		}
		return -1;
	}

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

	ECSENGINE_API void ConvertDateToString(Date date, Stream<char>& characters, ECS_FORMAT_DATE_FLAGS format_flags);

	ECSENGINE_API void ConvertDateToString(Date date, CapacityStream<char>& characters, ECS_FORMAT_DATE_FLAGS format_flags);

	ECSENGINE_API void ConvertDateToString(Date date, Stream<wchar_t>& characters, ECS_FORMAT_DATE_FLAGS format_flags);

	ECSENGINE_API void ConvertDateToString(Date date, CapacityStream<wchar_t>& characters, ECS_FORMAT_DATE_FLAGS format_flags);

	ECSENGINE_API size_t ConvertDateToStringMaxCharacterCount(ECS_FORMAT_DATE_FLAGS format_flags);

	ECSENGINE_API Date ConvertStringToDate(Stream<char> characters, ECS_FORMAT_DATE_FLAGS format_flags);

	ECSENGINE_API Date ConvertStringToDate(Stream<wchar_t> characters, ECS_FORMAT_DATE_FLAGS format_flags);

	// Returns the string enclosed in a set of parenthesis. If there are no parenthesis, it returns { nullptr, 0 }
	// Example AAAA(BBB) returns BBB
	ECSENGINE_API Stream<char> GetStringParameter(Stream<char> string);

	// Returns the string after the count'th character. If there are less than count characters of that type,
	// it will return { nullptr, 0 }
	ECSENGINE_API Stream<char> SkipCharacters(Stream<char> characters, char character, unsigned int count);

	// Returns the string after the count'th token. If there are less than count token of that type,
	// it will return { nullptr, 0 }
	ECSENGINE_API Stream<char> SkipTokens(Stream<char> characters, Stream<char> token, unsigned int count);

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

	ECSENGINE_API void ConvertByteSizeToString(size_t byte_size, Stream<char>& characters);

	ECSENGINE_API void ConvertByteSizeToString(size_t byte_size, CapacityStream<char>& characters);

	ECSENGINE_API void ConvertByteSizeToString(size_t byte_size, Stream<wchar_t>& characters);

	ECSENGINE_API void ConvertByteSizeToString(size_t byte_size, CapacityStream<wchar_t>& characters);

	ECSENGINE_API void ReplaceCharacter(Stream<char> string, char token_to_be_replaced, char replacement);

	ECSENGINE_API void ReplaceCharacter(Stream<wchar_t> string, wchar_t token_to_be_replaced, wchar_t replacement);

	ECSENGINE_API Stream<char> ReplaceToken(Stream<char> string, Stream<char> token, Stream<char> replacement, AllocatorPolymorphic allocator);

	ECSENGINE_API Stream<wchar_t> ReplaceToken(Stream<wchar_t> string, Stream<wchar_t> token, Stream<wchar_t> replacement, AllocatorPolymorphic allocator);

	ECSENGINE_API Stream<char> ReplaceToken(CapacityStream<char>& string, Stream<char> token, Stream<char> replacement);

	ECSENGINE_API Stream<wchar_t> ReplaceToken(CapacityStream<wchar_t>& string, Stream<wchar_t> token, Stream<wchar_t> replacement);

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
	ECSENGINE_API void SplitString(Stream<char> string, char delimiter, AdditionStream<Stream<char>> splits);

	// These splits will only reference the content inside the string
	ECSENGINE_API void SplitString(Stream<wchar_t> string, wchar_t delimiter, AdditionStream<Stream<wchar_t>> splits);

	// These splits will only reference the content inside the string
	ECSENGINE_API void SplitString(Stream<char> string, Stream<char> delimiter, AdditionStream<Stream<char>> splits);

	// These splits will only reference the content inside the string
	ECSENGINE_API void SplitString(Stream<wchar_t> string, Stream<wchar_t> delimiter, AdditionStream<Stream<wchar_t>> splits);

	// Returns the string isolated from other strings delimited using the given delimiter
	ECSENGINE_API Stream<char> IsolateString(Stream<char> string, Stream<char> token, Stream<char> delimiter);

	// Returns the string isolated from other strings delimited using the given delimiter
	ECSENGINE_API Stream<wchar_t> IsolateString(Stream<wchar_t> string, Stream<wchar_t> token, Stream<wchar_t> delimiter);

	// Returns the longest common starting string
	ECSENGINE_API Stream<char> StringsPrefix(Stream<Stream<char>> strings);

	// Returns the longest common starting string
	ECSENGINE_API Stream<wchar_t> StringsPrefix(Stream<Stream<wchar_t>> strings);

	// Calculates the integral and fractional parts and then commits them into a float each that then get summed up
	ECSENGINE_API float ConvertCharactersToFloat(Stream<char> characters);

	// Calculates the integral and fractional parts and then commits them into a float each that then get summed up
	ECSENGINE_API float ConvertCharactersToFloat(Stream<wchar_t> characters);

	// Calculates the integral and fractional parts and then commits them into a double each that then get summed up
	ECSENGINE_API double ConvertCharactersToDouble(Stream<char> characters);

	// Calculates the integral and fractional parts and then commits them into a double each that then get summed up
	ECSENGINE_API double ConvertCharactersToDouble(Stream<wchar_t> characters);

	// Finds the number of characters that needs to be allocated and a stack buffer can be supplied in order
	// to fill it if the count is smaller than its capacity
	// The token can be changed from ' ' to another supplied value (e.g. \\ for paths or /)
	ECSENGINE_API size_t FindWhitespaceCharactersCount(Stream<char> string, char separator_token = ' ', CapacityStream<unsigned int>* stack_buffer = nullptr);

	// it searches for spaces and next line characters
	template<typename SpaceStream>
	void FindWhitespaceCharacters(SpaceStream& spaces, Stream<char> string, char separator_token = ' ');

	// it searches for spaces and next line characters; Stream must be uint2
	template<typename WordStream>
	void ParseWordsFromSentence(Stream<char> sentence, WordStream& words);

	// given a token, it will separate words out from it; Stream must be uint2
	template<typename WordStream>
	void ParseWordsFromSentence(Stream<char> sentence, char token, WordStream& words);

	// given multiple tokens, it will separate words based from them; Stream must be uint2
	template<typename WordStream>
	void ParseWordsFromSentence(Stream<char> sentence, Stream<char> tokens, WordStream& words);

	// Shifts the float by the power of 10 to the precision that then get cast into an integer;
	// Afterwards performs the necessary parsing for 0. values and case evaluation for rounding
	// Returns the count of characters written
	template<typename Stream>
	size_t ConvertFloatToChars(Stream& chars, float value, size_t precision = 2);

	// Shifts the float by the power of 10 to the precision that then get cast into an integer;
	// Afterwards performs the necessary parsing for 0. values and case evaluation for rounding
	// Returns the count of characters written
	template<typename Stream>
	size_t ConvertDoubleToChars(Stream& chars, double value, size_t precision = 2);

	// Just forwards to the appropriate floating point variant
	template<typename FloatingType, typename Stream>
	ECS_INLINE size_t ConvertFloatingPointToChars(Stream& chars, FloatingType value, size_t precision = 2) {
		static_assert(std::is_floating_point_v<FloatingType>);

		if constexpr (std::is_same_v<FloatingType, float>) {
			return ConvertFloatToChars(chars, value, precision);
		}
		else {
			return ConvertDoubleToChars(chars, value, precision);
		}
	}

	// Works with decimal digits that then get pushed into the stream;
	// Adds apostrophes ' in order to have an easier way of distinguishing powers of 10
	// Returns the count of characters written
	template<typename Stream>
	size_t ConvertIntToCharsFormatted(Stream& chars, int64_t value);

	// Works with decimal digits that then get pushed into the stream;
	// Vanilla conversion; no apostrophes '
	// Returns the count of characters written
	template<typename Stream>
	size_t ConvertIntToChars(Stream& chars, int64_t value);

	// Non digit characters are discarded
	ECSENGINE_API int64_t ConvertCharactersToInt(Stream<char> stream);

	// Non digit characters are discarded
	ECSENGINE_API int64_t ConvertCharactersToInt(Stream<wchar_t> stream);

	// Non digit characters are discarded
	ECSENGINE_API int64_t ConvertCharactersToInt(Stream<char> stream, size_t& digit_count);

	// Non digit characters are discarded
	ECSENGINE_API int64_t ConvertCharactersToInt(Stream<wchar_t> stream, size_t& digit_count);

	// Returns 0 or 1 if it is a boolean (i.e. false or true in the characters)
	// Else returns -1
	ECSENGINE_API char ConvertCharactersToBool(Stream<char> characters);

	// Returns 0 or 1 if it is a boolean (i.e. false or true in the characters)
	// Else returns -1
	ECSENGINE_API char ConvertCharactersToBool(Stream<wchar_t> characters);

	// Non digit characters are discarded (exception x)
	ECSENGINE_API void* ConvertCharactersToPointer(Stream<char> characters);
	
	// Non digit characters are discarded (exception x)
	ECSENGINE_API void* ConvertCharactersToPointer(Stream<wchar_t> characters);

	// non digit characters are discarded
	template<typename Integer, typename CharacterType, bool strict_parsing, typename Stream>
	Integer ConvertCharactersToIntImpl(Stream stream, size_t& digit_count, bool& success) {
		Integer integer = Integer(0);
		size_t starting_index = stream[0] == Character<CharacterType>('-') ? 1 : 0;
		digit_count = 0;

		for (size_t index = starting_index; index < stream.size; index++) {
			if (stream[index] >= Character<CharacterType>('0') && stream[index] <= Character<CharacterType>('9')) {
				integer = integer * 10 + stream[index] - Character<CharacterType>('0');
				digit_count++;
			}
			else {
				if constexpr (strict_parsing) {
					success = false;
					return integer;
				}
			}
		}

		if constexpr (strict_parsing) {
			success = true;
		}
		integer = starting_index == 1 ? -integer : integer;
		return integer;
	}

	// Non digit characters are discarded
	template<typename Integer, typename CharacterType, bool strict_parsing, typename Stream>
	ECS_INLINE Integer ConvertCharactersToIntImpl(Stream stream, bool& success) {
		// Dummy
		size_t digit_count = 0;
		return ConvertCharactersToIntImpl<Integer, CharacterType, strict_parsing>(stream, digit_count, success);
	}

	// It will try to convert the string into a number using strict verification
	// It will write the success status in the given boolean
	ECSENGINE_API int64_t ConvertCharactersToIntStrict(Stream<char> string, bool& success);

	// It will try to convert the string into a number using strict verification
	// It will write the success status in the given boolean
	ECSENGINE_API int64_t ConvertCharactersToIntStrict(Stream<wchar_t> string, bool& success);

	// It will try to convert the string into a float using strict verification
	// It will write the success status in the given boolean
	ECSENGINE_API float ConvertCharactersToFloatStrict(Stream<char> string, bool& success);

	// It will try to convert the string into a float using strict verification
	// It will write the success status in the given boolean
	ECSENGINE_API float ConvertCharactersToFloatStrict(Stream<wchar_t> string, bool& success);

	// It will try to convert the string into a double using strict verification
	// It will write the success status in the given boolean
	ECSENGINE_API double ConvertCharactersToDoubleStrict(Stream<char> string, bool& success);

	// It will try to convert the string into a double using strict verification
	// It will write the success status in the given boolean
	ECSENGINE_API double ConvertCharactersToDoubleStrict(Stream<wchar_t> string, bool& success);

	// It will try to convert the string into a pointer using strict verification
	// It will write the success status in the given boolean
	ECSENGINE_API void* ConvertCharactersToPointerStrict(Stream<char> string, bool& success);

	// It will try to convert the string into a pointer using strict verification
	// It will write the success status in the given boolean
	ECSENGINE_API void* ConvertCharactersToPointerStrict(Stream<wchar_t> string, bool& success);

	// It will try to convert the string into a bool using strict verification
	// It will write the success status in the given boolean
	ECSENGINE_API bool ConvertCharactersToBoolStrict(Stream<char> string, bool& success);

	// It will try to convert the string into a bool using strict verification
	// It will write the success status in the given boolean
	ECSENGINE_API bool ConvertCharactersToBoolStrict(Stream<wchar_t> string, bool& success);


#define ECS_CONVERT_INT_TO_HEX_DO_NOT_WRITE_0X (1 << 0)
#define ECS_CONVERT_INT_TO_HEX_ADD_NORMAL_VALUE_AFTER (1 << 1)

	template<size_t flags = 0, typename Integer>
	void ConvertIntToHex(CapacityStream<char>& characters, Integer integer) {
		static char hex_characters[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

		if constexpr ((flags & ECS_CONVERT_INT_TO_HEX_DO_NOT_WRITE_0X) == 0) {
			characters.AssertCapacity(2);
			characters.Add('0');
			characters.Add('x');
		}
		if constexpr (std::is_same_v<Integer, int8_t> || std::is_same_v<Integer, uint8_t> || std::is_same_v<Integer, char>) {
			characters.AssertCapacity(2);

			char low_part = integer & 0x0F;
			char high_part = (integer & 0xF0) >> 4;
			characters.Add(hex_characters[(unsigned int)high_part]);
			characters.Add(hex_characters[(unsigned int)low_part]);
		}
		else if constexpr (std::is_same_v<Integer, int16_t> || std::is_same_v<Integer, uint16_t>) {
			characters.AssertCapacity(4);

			char digit0 = integer & 0x000F;
			char digit1 = (integer & 0x00F0) >> 4;
			char digit2 = (integer & 0x0F00) >> 8;
			char digit3 = (integer & 0xF000) >> 12;
			characters.Add(hex_characters[(unsigned int)digit3]);
			characters.Add(hex_characters[(unsigned int)digit2]);
			characters.Add(hex_characters[(unsigned int)digit1]);
			characters.Add(hex_characters[(unsigned int)digit0]);
		}
		else if constexpr (std::is_same_v<Integer, int32_t> || std::is_same_v<Integer, uint32_t>) {
			uint16_t first_two_bytes = integer & 0x0000FFFF;
			uint16_t last_two_bytes = (integer & 0xFFFF0000) >> 16;
			ConvertIntToHex<ECS_CONVERT_INT_TO_HEX_DO_NOT_WRITE_0X>(characters, last_two_bytes);
			ConvertIntToHex<ECS_CONVERT_INT_TO_HEX_DO_NOT_WRITE_0X>(characters, first_two_bytes);
		}
		else if constexpr (std::is_same_v<Integer, int64_t> || std::is_same_v<Integer, uint64_t>) {
			uint32_t first_integer = integer & 0x00000000FFFFFFFF;
			uint32_t second_integer = (integer & 0xFFFFFFFF00000000) >> 32;
			ConvertIntToHex<ECS_CONVERT_INT_TO_HEX_DO_NOT_WRITE_0X>(characters, second_integer);
			ConvertIntToHex<ECS_CONVERT_INT_TO_HEX_DO_NOT_WRITE_0X>(characters, first_integer);
		}

		if constexpr ((flags & ECS_CONVERT_INT_TO_HEX_ADD_NORMAL_VALUE_AFTER) != 0) {
			characters.AssertCapacity(2);
			characters.Add(' ');
			characters.Add('(');
			ConvertIntToCharsFormatted(characters, static_cast<int64_t>(integer));
			characters.AssertCapacity(2);
			characters.Add(')');
			characters.Add(' ');
		}
		characters.AssertCapacity(1);
		characters[characters.size] = '\0';
	}

	namespace Internal {
		// Returns how many total characters have been written in the x component
		// and in the y component the offset into the base characters where the string was found
		template<typename Parameter>
		ulong2 FormatStringInternal(
			CapacityStream<char>& end_characters,
			const char* base_characters,
			Parameter parameter
		) {
			const char* string_ptr = strstr(base_characters, ECS_FORMAT_SPECIFIER);
			if (string_ptr != nullptr) {
				unsigned int initial_end_characters_count = end_characters.size;
				size_t copy_count = (uintptr_t)string_ptr - (uintptr_t)base_characters;
				end_characters.AddStreamAssert(Stream<char>(base_characters, copy_count));

				if constexpr (std::is_floating_point_v<Parameter>) {
					if constexpr (std::is_same_v<Parameter, float>) {
						ConvertFloatToChars(end_characters, parameter, 3);
					}
					else if constexpr (std::is_same_v<Parameter, double>) {
						ConvertDoubleToChars(end_characters, parameter, 3);
					}
				}
				else if constexpr (std::is_integral_v<Parameter>) {
					ConvertIntToChars(end_characters, static_cast<int64_t>(parameter));
				}
				else if constexpr (std::is_pointer_v<Parameter>) {
					if constexpr (std::is_same_v<Parameter, const char*>) {
						size_t substring_size = strlen(parameter);
						end_characters.AddStreamAssert(Stream<char>(parameter, substring_size));
						end_characters.AssertCapacity(1);
					}
					else if constexpr (std::is_same_v<Parameter, const wchar_t*>) {
						size_t wide_count = wcslen(parameter);
						end_characters.AssertCapacity(wide_count + 1);
						ConvertWideCharsToASCII(Stream<wchar_t>(parameter, wide_count + 1), end_characters);
					}
					else {
						ConvertIntToHex(end_characters, (int64_t)parameter);
					}
				}
				else if constexpr (std::is_same_v<Parameter, CapacityStream<wchar_t>> || std::is_same_v<Parameter, Stream<wchar_t>>) {
					end_characters.AssertCapacity(parameter.size);
					ConvertWideCharsToASCII(parameter, end_characters);
				}
				else if constexpr (std::is_same_v<Parameter, CapacityStream<char>> || std::is_same_v<Parameter, Stream<char>>) {
					end_characters.AddStreamAssert(parameter);
				}
				else {
					parameter.ToString(end_characters);
				}
				return { end_characters.size - initial_end_characters_count, copy_count };
			}

			return { 0, 0 };
		}

		extern template ECSENGINE_API ulong2 FormatStringInternal<const char*>(CapacityStream<char>&, const char*, const char*);
		extern template ECSENGINE_API ulong2 FormatStringInternal<const wchar_t*>(CapacityStream<char>&, const char*, const wchar_t*);
		extern template ECSENGINE_API ulong2 FormatStringInternal<Stream<char>>(CapacityStream<char>&, const char*, Stream<char>);
		extern template ECSENGINE_API ulong2 FormatStringInternal<Stream<wchar_t>>(CapacityStream<char>&, const char*, Stream<wchar_t>);
		extern template ECSENGINE_API ulong2 FormatStringInternal<CapacityStream<char>>(CapacityStream<char>&, const char*, CapacityStream<char>);
		extern template ECSENGINE_API ulong2 FormatStringInternal<CapacityStream<wchar_t>>(CapacityStream<char>&, const char*, CapacityStream<wchar_t>);
		extern template ECSENGINE_API ulong2 FormatStringInternal<unsigned int>(CapacityStream<char>&, const char*, unsigned int);
		extern template ECSENGINE_API ulong2 FormatStringInternal<void*>(CapacityStream<char>&, const char*, void*);
		extern template ECSENGINE_API ulong2 FormatStringInternal<float>(CapacityStream<char>&, const char*, float);
		extern template ECSENGINE_API ulong2 FormatStringInternal<double>(CapacityStream<char>&, const char*, double);

		template<typename FirstParameter, typename... Parameters>
		void FormatStringImpl(CapacityStream<char>& destination, const char* base_characters, ulong2& written_characters, const FirstParameter& first_parameter, Parameters... parameters) {
			ulong2 current_written_characters = FormatStringInternal(destination, base_characters + written_characters.y, first_parameter);
			written_characters.y += current_written_characters.y + 3;

			if constexpr (sizeof...(Parameters) > 0) {
				FormatStringImpl(destination, base_characters, written_characters, parameters...);
			}
		}

	}

	template<typename... Parameters>
	void FormatString(CapacityStream<char>& destination, const char* base_characters, Parameters... parameters) {
		ulong2 written_characters = { 0, 0 };
		Internal::FormatStringImpl(destination, base_characters, written_characters, parameters...);

		// The remaining characters still need to be copied
		size_t base_character_count = strlen(base_characters);
		size_t characters_to_be_written = base_character_count - written_characters.y;
		if (characters_to_be_written > 0) {
			destination.AssertCapacity(characters_to_be_written);
			memcpy(destination.buffer + destination.size, base_characters + written_characters.y, characters_to_be_written);
			destination.size += characters_to_be_written;
		}
		destination.AssertCapacity(1);
		destination[destination.size] = '\0';
	}

	static void DebugLocationString(DebugInfo debug_info, CapacityStream<char>* string) {
		string->AddStreamAssert("File: ");
		string->AddStreamAssert(debug_info.file);
		string->AddStreamAssert(", function: ");
		string->AddStreamAssert(debug_info.function);
		string->AddStreamAssert(", line: ");
		ConvertIntToChars(*string, debug_info.line);
	}

}