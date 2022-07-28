#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "../Containers/Stream.h"
#include "BasicTypes.h"
#include "File.h"

namespace ECSEngine {

#ifndef ECS_BYTE_EXTRACT
	#define ECS_BYTE_EXTRACT
	#define ECS_SCALAR_BYTE1_EXTRACT [](unsigned int a) { return a & 0x000000FF; }
	#define ECS_SCALAR_BYTE2_EXTRACT [](unsigned int a) { return (a & 0x0000FF00) >> 8; }
	#define ECS_SCALAR_BYTE3_EXTRACT [](unsigned int a) { return (a & 0x00FF0000) >> 16; }
	#define ECS_SCALAR_BYTE4_EXTRACT [](unsigned int a) { return (a & 0xFF000000) >> 24; }
	#define ECS_SCALAR_BYTE12_EXTRACT [](unsigned int a) { return (unsigned int)(a & 0x0000FFFF); }
	#define ECS_SCALAR_BYTE34_EXTRACT [](unsigned int a) { return (unsigned int)((a & 0xFFFF0000) >> 16); }
	#define ECS_VECTOR_BYTE1_EXTRACT [](Vec8ui a) { return a & 0x000000FF; }
	#define ECS_VECTOR_BYTE2_EXTRACT [](Vec8ui a) { return (a & 0x0000FF00) >> 8; }
	#define ECS_VECTOR_BYTE3_EXTRACT [](Vec8ui a) { return (a & 0x00FF0000) >> 16; }
	#define ECS_VECTOR_BYTE4_EXTRACT [](Vec8ui a) { return (a & 0xFF000000) >> 24; }
	#define ECS_VECTOR_BYTE12_EXTRACT [](Vec8ui a) { return a & 0x0000FFFF; }
	#define ECS_VECTOR_BYTE34_EXTRACT [](Vec8ui a) { return (a & 0xFFFF0000) >> 16; }
	#define ECS_SCALAR_BYTE1_EXTRACT_REDUCTION [](unsigned int a, unsigned int reduction) { return (a - reduction) & 0x000000FF; }
	#define ECS_SCALAR_BYTE2_EXTRACT_REDUCTION [](unsigned int a, unsigned int reduction) { return ((a - reduction) & 0x0000FF00) >> 8; }
	#define ECS_SCALAR_BYTE3_EXTRACT_REDUCTION [](unsigned int a, unsigned int reduction) { return ((a - reduction) & 0x00FF0000) >> 16; }
	#define ECS_SCALAR_BYTE4_EXTRACT_REDUCTION [](unsigned int a, unsigned int reduction) { return ((a - reduction) & 0xFF000000) >> 24; }
	#define ECS_SCALAR_BYTE12_EXTRACT_REDUCTION [](unsigned int a, unsigned int reduction) { return (a - reduction) & 0x0000FFFF; }
	#define ECS_SCALAR_BYTE34_EXTRACT_REDUCTION [](unsigned int a, unsigned int reduction) { return ((a - reduction) & 0xFFFF0000) >> 16; }
	#define ECS_VECTOR_BYTE1_EXTRACT_REDUCTION [](Vec8ui a, Vec8ui reduction) { return (a - reduction) & 0x000000FF; }
	#define ECS_VECTOR_BYTE2_EXTRACT_REDUCTION [](Vec8ui a, Vec8ui reduction) { return ((a - reduction) & 0x0000FF00) >> 8; }
	#define ECS_VECTOR_BYTE3_EXTRACT_REDUCTION [](Vec8ui a, Vec8ui reduction) { return ((a - reduction) & 0x00FF0000) >> 16; }
	#define ECS_VECTOR_BYTE4_EXTRACT_REDUCTION [](Vec8ui a, Vec8ui reduction) { return ((a - reduction) & 0xFF000000) >> 24; }
	#define ECS_VECTOR_BYTE12_EXTRACT_REDUCTION [](Vec8ui a, Vec8ui reduction) { return (a - reduction) & 0x0000FFFF; }
	#define ECS_VECTOR_BYTE34_EXTRACT_REDUCTION [](Vec8ui a, Vec8ui reduction) { return ((a - reduction) & 0xFFFF0000) >> 16; }

#endif

#define ECS_FORMAT_TEMP_STRING(string_name, base_characters, ...) ECS_TEMP_ASCII_STRING(string_name, 512); \
string_name.size = function::FormatString(string_name.buffer, base_characters, __VA_ARGS__); \
string_name.AssertCapacity();

#define ECS_FORMAT_STRING(string, base_characters, ...) (string).size = function::FormatString((string).buffer, base_characters, __VA_ARGS__); \
(string).AssertCapacity();

#define ECS_FORMAT_ERROR_MESSAGE(error_message, base_characters, ...) if (error_message != nullptr) { \
	error_message->size = function::FormatString(error_message->buffer, base_characters, __VA_ARGS__); \
}

	namespace function {

		// AVX2 accelerated memcpy, works best for workloads of at least 1KB
		template<bool unaligned = false>
		void avx2_copy_32multiple(void* destination, const void* source, size_t bytes);

		// AVX2 accelerated memcpy, works best for workloads of at least 1KB
		template<bool unaligned = false>
		void avx2_copy_128multiple(void* destination, const void* source, size_t bytes);

		// allocates memory on the heap;
		// if exact_length is false, a maximum of 1024 characters can be converted
		// if exact_length is true, the pointer will be strlen-ed 
		template<bool exact_length = false>
		wchar_t* ConvertASCIIToWide(const char* pointer);

		// Calculates the integral and fractional parts and then commits them into a float each that then get summed up
		template<typename Stream>
		float ConvertCharactersToFloat(Stream characters);
		
		// Calculates the integral and fractional parts and then commits them into a double each that then get summed up
		template<typename Stream>
		double ConvertCharactersToDouble(Stream characters);

		// Finds the number of characters that needs to be allocated and a stack buffer can be supplied in order
		// to fill it if the count is smaller than its capacity
		// The token can be changed from ' ' to another supplied value (e.g. \\ for paths or /)
		ECSENGINE_API size_t FindWhitespaceCharactersCount(const char* string, char separator_token = ' ', CapacityStream<unsigned int>*stack_buffer = nullptr);

		// it searches for spaces and next line characters
		template<typename Stream>
		void FindWhitespaceCharacters(Stream& spaces, const char* string, char separator_token = ' ');

		// finds the tokens that appear in the current string
		template<typename Stream>
		void FindToken(const char* string, char token, Stream& tokens);

		// finds the tokens that appear in the current string
		template<typename Stream>
		void FindToken(const char* ECS_RESTRICT string, const char* ECS_RESTRICT token, Stream& tokens);

		// it searches for spaces and next line characters; Stream must be uint2
		template<typename Stream>
		void ParseWordsFromSentence(const char* sentence, Stream& words);

		// given a token, it will separate words out from it; Stream must be uint2
		template<typename Stream>
		void ParseWordsFromSentence(const char* sentence, char token, Stream& words);

		// given multiple tokens, it will separate words based from them; Stream must be uint2
		template<typename Stream>
		void ParseWordsFromSentence(const char* ECS_RESTRICT sentence, const char* ECS_RESTRICT tokens, Stream& words);

		// condition ? first_value : second_value
		// The only full template because it is a one liner
		template<typename Type>
		Type Select(bool condition, Type first_value, Type second_value) {
			return condition ? first_value : second_value;
		}

		// Calculates the integral and fractional parts and then commits them into a floating point type each that then get summed up 
		// Returns the count of characters written
		template<typename Stream>
		size_t ConvertFloatingPointIntegerToChars(Stream& chars, int64_t integer, size_t precision);

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
		template<typename Integer, typename Stream>
		Integer ConvertCharactersToInt(Stream stream);

		// Non digit characters are discarded
		template<typename Integer, typename Stream>
		Integer ConvertCharactersToInt(Stream stream, size_t& digit_count);

		// Non digit characters are discarded
		template<typename FloatingPoint, typename Stream>
		FloatingPoint ConvertCharactersToFloatingPoint(Stream stream);

		template<typename CharacterType, typename ReturnType, typename Functor>
		ReturnType ParseType(const CharacterType** characters, Stream<CharacterType> delimiters, Functor&& functor) {
			Stream<CharacterType> stream_characters(*characters, 0);

			CharacterType current_character = **characters;
			bool continue_search = true;

			auto not_delimiter = [delimiters](CharacterType character) {
				for (size_t index = 0; index < delimiters.size; index++) {
					if (delimiters[index] == character) {
						return false;
					}
				}
				return true;
			};

			continue_search = not_delimiter(current_character);
			while (continue_search) {
				current_character = **characters;
				*characters++;
				continue_search = not_delimiter(current_character);
			}

			stream_characters.size = *characters - stream_characters.buffer;
			return functor(stream_characters);
		}

		template<typename Integer, typename CharacterType>
		Integer ParseInteger(const CharacterType** characters, Stream<CharacterType> delimiters) {
			return ParseType<CharacterType, Integer>(characters, delimiters, [](Stream<CharacterType> stream_characters) {
				return ConvertCharactersToInt<Integer>(stream_characters);
			});
		}

		template<typename CharacterType>
		ECSENGINE_API float ParseFloat(const CharacterType** characters, Stream<CharacterType> delimiters);

		template<typename CharacterType>
		ECSENGINE_API double ParseDouble(const CharacterType** characters, Stream<CharacterType> delimiters);

		template<typename CharacterType>
		ECSENGINE_API bool ParseBool(const CharacterType** characters, Stream<CharacterType> delimiters);

		ECSENGINE_API void* Copy(AllocatorPolymorphic allocator, const void* data, size_t data_size, size_t alignment = 8);

		ECSENGINE_API Stream<void> Copy(AllocatorPolymorphic allocator, Stream<void> data, size_t alignment = 8);

		// If data size is 0, it will return the data back
		ECSENGINE_API void* CopyNonZero(AllocatorPolymorphic allocator, void* data, size_t data_size, size_t alignment = 8);

		// If data size is 0, it will return the data back
		ECSENGINE_API Stream<void> CopyNonZero(AllocatorPolymorphic allocator, Stream<void> data, size_t alignment = 8);

		// It will copy the null termination character
		ECSENGINE_API Stream<char> StringCopy(AllocatorPolymorphic allocator, const char* string);

		// It will copy the null termination character
		ECSENGINE_API Stream<char> StringCopy(AllocatorPolymorphic allocator, Stream<char> string);

		ECSENGINE_API Stream<wchar_t> StringCopy(AllocatorPolymorphic allocator, const wchar_t* string);

		ECSENGINE_API Stream<wchar_t> StringCopy(AllocatorPolymorphic allocator, Stream<wchar_t> string);

		template<typename Stream>
		ECSENGINE_API void MakeSequence(Stream stream);

		template<typename Stream>
		ECSENGINE_API void MakeDescendingSequence(Stream stream);

		// The size of data must contain the byte size of the element
		template<typename IndexStream>
		ECSENGINE_API void CopyStreamWithMask(void* ECS_RESTRICT buffer, Stream<void> data, IndexStream indices);

		// The size of data must contain the byte size of the element
		// Reports if an error occurred during writing
		template<typename IndexStream>
		ECSENGINE_API bool CopyStreamWithMask(ECS_FILE_HANDLE file, Stream<void> data, IndexStream indices);

		template<bool ascending = true, typename T>
		void insertion_sort(T* buffer, size_t size, int increment = 1) {
			size_t i = 0;
			while (i + increment < size) {
				if constexpr (ascending) {
					while (i + increment < size && buffer[i] <= buffer[i + increment]) {
						i += increment;
					}
					int64_t j = i + increment;
					if (j >= size) {
						return;
					}
					while (j - increment >= 0 && buffer[j] < buffer[j - increment]) {
						T temp = buffer[j];
						buffer[j] = buffer[j - increment];
						buffer[j - increment] = temp;
						j -= increment;
					}
				}
				else {
					while (i + increment < size && buffer[i] >= buffer[i + increment]) {
						i += increment;
					}
					int64_t j = i + increment;
					if (j >= size) {
						return;
					}
					while (j - increment >= 0 && buffer[j] < buffer[j - increment]) {
						T temp = buffer[j];
						buffer[j] = buffer[j - increment];
						buffer[j - increment] = temp;
						j -= increment;
					}
				}
			}
		}

		template<typename BufferType, typename ExtractKey>
		void byte_counting_sort(BufferType buffer, size_t size, BufferType result, ExtractKey&& extract_key)
		{
			size_t counts[257] = { 0 };

			// building the frequency count
			for (size_t i = 0; i < size; i++)
			{
				counts[extract_key(buffer[i]) + 1]++;
			}

			// building the array positions
			for (int i = 2; i < 257; i++)
			{
				counts[i] += counts[i - 1];
			}

			// placing the elements into the array
			for (size_t i = 0; i < size; i++) {
				auto key = extract_key(buffer[i]);
				result[counts[key]++] = buffer[i];
			}

		}

		template<typename BufferType, typename ExtractKey>
		void byte_counting_sort(BufferType buffer, size_t size, BufferType result, ExtractKey&& extract_key, unsigned int* counts)
		{
			// building the frequency count
			for (size_t i = 0; i < size; i++)
			{
				counts[extract_key(buffer[i]) + 1]++;
			}

			// building the array positions
			for (int i = 2; i < 65537; i++)
			{
				counts[i] += counts[i - 1];
			}

			// placing the elements into the array
			for (size_t i = 0; i < size; i++) {
				auto key = extract_key(buffer[i]);
				result[counts[key]++] = buffer[i];
			}

		}

		template<typename BufferType, typename ExtractKey>
		void byte_counting_sort(BufferType buffer, size_t size, BufferType result, ExtractKey&& extract_key, unsigned int reduction)
		{
			size_t counts[257] = { 0 };

			// building the frequency count
			for (size_t i = 0; i < size; i++)
			{
				counts[extract_key(buffer[i], reduction) + 1]++;
			}

			// building the array positions
			for (int i = 2; i < 257; i++)
			{
				counts[i] += counts[i - 1];
			}

			// placing the elements into the array
			for (size_t i = 0; i < size; i++) {
				auto key = extract_key(buffer[i], reduction);
				result[counts[key]++] = buffer[i];
			}

		}

		template<typename BufferType, typename ExtractKey>
		void byte_counting_sort(BufferType buffer, size_t size, BufferType result, ExtractKey&& extract_key, unsigned int reduction, unsigned int* counts)
		{
			// building the frequency count
			for (size_t i = 0; i < size; i++)
			{
				counts[extract_key(buffer[i], reduction) + 1]++;
			}

			// building the array positions
			for (int i = 2; i < 65537; i++)
			{
				counts[i] += counts[i - 1];
			}

			// placing the elements into the array
			for (size_t i = 0; i < size; i++) {
				auto key = extract_key(buffer[i], reduction);
				result[counts[key]++] = buffer[i];
			}

		}

		// returns true if the sorted buffer is the intermediate one
		template<bool randomize = false>
		bool unsigned_integer_radix_sort(
			unsigned int* buffer,
			unsigned int* intermediate,
			size_t size,
			unsigned int maximum_element,
			unsigned int minimum_element
		)
		{
			if constexpr (randomize) {
				for (size_t index = size; index < size + (size >> 8); index++) {
					buffer[index] = rand() + maximum_element;
				}
				size += size >> 8;
				maximum_element += RAND_MAX;
			}
			unsigned int reduction = maximum_element - minimum_element;
			bool flag = true;
			if (reduction < 256 && maximum_element >= 256) {
				auto extract_key1 = ECS_SCALAR_BYTE1_EXTRACT_REDUCTION;
				byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), reduction);
			}
			else if (reduction < 256 && maximum_element < 256) {
				auto extract_key1 = ECS_SCALAR_BYTE1_EXTRACT;
				byte_counting_sort(buffer, size, intermediate, std::move(extract_key1));
			}
			else if (reduction < 65'536 && maximum_element >= 65'536) {
				auto extract_key1 = ECS_SCALAR_BYTE1_EXTRACT_REDUCTION;
				byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), reduction);
				auto extract_key2 = ECS_SCALAR_BYTE2_EXTRACT_REDUCTION;
				byte_counting_sort(intermediate, size, buffer, std::move(extract_key2), reduction);
				flag = false;
			}
			else if (reduction < 65'536 && maximum_element < 65'536) {
				auto extract_key1 = ECS_SCALAR_BYTE1_EXTRACT;
				byte_counting_sort(buffer, size, intermediate, std::move(extract_key1));
				auto extract_key2 = ECS_SCALAR_BYTE2_EXTRACT;
				byte_counting_sort(intermediate, size, buffer, std::move(extract_key2));
				flag = false;
			}
			else if (reduction < 16'777'216 && maximum_element >= 16'777'216) {
				auto extract_key1 = ECS_SCALAR_BYTE1_EXTRACT_REDUCTION;
				byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), reduction);
				auto extract_key2 = ECS_SCALAR_BYTE2_EXTRACT_REDUCTION;
				byte_counting_sort(intermediate, size, buffer, std::move(extract_key2), reduction);
				auto extract_key3 = ECS_SCALAR_BYTE3_EXTRACT_REDUCTION;
				byte_counting_sort(buffer, size, intermediate, std::move(extract_key3), reduction);
				flag = true;
			}
			else if (reduction < 16'777'216 && maximum_element < 16'777'216) {
				auto extract_key1 = ECS_SCALAR_BYTE1_EXTRACT;
				byte_counting_sort(buffer, size, intermediate, std::move(extract_key1));
				auto extract_key2 = ECS_SCALAR_BYTE2_EXTRACT;
				byte_counting_sort(intermediate, size, buffer, std::move(extract_key2));
				auto extract_key3 = ECS_SCALAR_BYTE3_EXTRACT;
				byte_counting_sort(buffer, size, intermediate, std::move(extract_key3));
				flag = true;
			}
			else if (reduction >= 16'777'216) {
				auto extract_key1 = ECS_SCALAR_BYTE1_EXTRACT;
				byte_counting_sort(buffer, size, intermediate, std::move(extract_key1));
				auto extract_key2 = ECS_SCALAR_BYTE2_EXTRACT;
				byte_counting_sort(intermediate, size, buffer, std::move(extract_key2));
				auto extract_key3 = ECS_SCALAR_BYTE3_EXTRACT;
				byte_counting_sort(buffer, size, intermediate, std::move(extract_key3));
				auto extract_key4 = ECS_SCALAR_BYTE4_EXTRACT;
				byte_counting_sort(intermediate, size, buffer, std::move(extract_key4));
				flag = false;
			}
			return flag;
		}

		//// 2 bytes at a time
		//static int unsigned_integer_radix_sort(unsigned int* buffer, unsigned int* intermediate, size_t size,
		//	unsigned int maximum_element, unsigned int minimum_element, unsigned int* counts)
		//{
		//	for (size_t index = size; index < size + (size >> 8); index++) {
		//		buffer[index] = rand() + maximum_element;
		//	}
		//	size += size >> 8;
		//	maximum_element += RAND_MAX;
		//	unsigned int reduction = maximum_element - minimum_element;
		//	int flag = 1;
		//	if (reduction < 65536 && maximum_element >= 65536) {
		//		auto extract_key1 = ECS_SCALAR_BYTE12_EXTRACT_REDUCTION;
		//		byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), reduction, counts);
		//	}
		//	else if (reduction < 65536 && maximum_element < 65536) {
		//		auto extract_key1 = ECS_SCALAR_BYTE12_EXTRACT;
		//		byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), counts);
		//	}
		//	else {
		//		auto extract_key1 = ECS_SCALAR_BYTE12_EXTRACT;
		//		byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), counts);
		//		memset(counts, 0, sizeof(unsigned int) * 65537);
		//		auto extract_key2 = ECS_SCALAR_BYTE34_EXTRACT;
		//		byte_counting_sort(intermediate, size, buffer, std::move(extract_key2), counts);
		//		flag = 0;
		//	}
		//	return flag;
		//}

		//// 2 bytes at a time
		//static int unsigned_integer_radix_sort(unsigned int* buffer, unsigned int* intermediate, size_t size,
		//	unsigned int maximum_element, unsigned int minimum_element, unsigned int* counts, bool randomize)
		//{
		//	unsigned int reduction = maximum_element - minimum_element;
		//	int flag = 1;
		//	if (reduction < 65536 && maximum_element >= 65536) {
		//		auto extract_key1 = ECS_SCALAR_BYTE12_EXTRACT_REDUCTION;
		//		byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), reduction, counts);
		//	}
		//	else if (reduction < 65536 && maximum_element < 65536) {
		//		auto extract_key1 = ECS_SCALAR_BYTE12_EXTRACT;
		//		byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), counts);
		//	}
		//	else {
		//		auto extract_key1 = ECS_SCALAR_BYTE12_EXTRACT;
		//		byte_counting_sort(buffer, size, intermediate, std::move(extract_key1), counts);
		//		memset(counts, 0, sizeof(unsigned int) * 65537);
		//		auto extract_key2 = ECS_SCALAR_BYTE34_EXTRACT;
		//		byte_counting_sort(intermediate, size, buffer, std::move(extract_key2), counts);
		//		flag = 0;
		//	}
		//	return flag;
		//}

		// non digit characters are discarded
		template<typename Integer, typename Stream>
		Integer ConvertCharactersToInt(Stream stream) {
			Integer integer = Integer(0);
			size_t starting_index = Select(stream[0] == '-', 1, 0);

			for (size_t index = starting_index; index < stream.size; index++) {
				if (stream[index] >= '0' && stream[index] <= '9') {
					integer = integer * 10 + stream[index] - '0';
				}
			}
			integer = Select<Integer>(starting_index == 1, -integer, integer);

			return integer;
		}

		// non digit characters are discarded
		template<typename Integer, typename Stream>
		Integer ConvertCharactersToInt(Stream stream, size_t& digit_count) {
			Integer integer = Integer(0);
			size_t starting_index = Select(stream[0] == '-', 1, 0);
			digit_count = 0;

			for (size_t index = starting_index; index < stream.size; index++) {
				if (stream[index] >= '0' && stream[index] <= '9') {
					integer = integer * 10 + stream[index] - '0';
					digit_count++;
				}
			}
			integer = Select(starting_index == 1, -integer, integer);

			return integer;
		}

		template<typename Stream, typename Value, typename Function>
		void GetMinFromStream(const Stream& input, Value& value, Function&& function) {
			for (size_t index = 0; index < input.size; index++) {
				Value current_value = function(input[index]);
				value = Select(value > current_value, current_value, value);
			}
		}

		template<typename Stream, typename Value, typename Function>
		void GetMaxFromStream(const Stream& input, Value& value, Function&& function) {
			for (size_t index = 0; index < input.size; index++) {
				Value current_value = function(input[index]);
				value = Select(value < current_value, current_value, value);
			}
		}

		template<typename Stream, typename Value, typename Function>
		void GetExtremesFromStream(const Stream& input, Value& min, Value& max, Function&& accessor) {
			for (size_t index = 0; index < input.size; index++) {
				Value current_value = accessor(input[index]);
				min = Select(min > current_value, current_value, min);
				max = Select(max < current_value, current_value, max);
			}
		}

#define ECS_CONVERT_INT_TO_HEX_DO_NOT_WRITE_0X (1 << 0)
#define ECS_CONVERT_INT_TO_HEX_ADD_NORMAL_VALUE_AFTER (1 << 1)

		template<size_t flags = 0, typename Integer>
		void ConvertIntToHex(Stream<char>& characters, Integer integer) {
			constexpr char hex_characters[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

			if constexpr ((flags & ECS_CONVERT_INT_TO_HEX_DO_NOT_WRITE_0X) == 0) {
				characters.Add('0');
				characters.Add('x');
			}
			if constexpr (std::is_same_v<Integer, int8_t> || std::is_same_v<Integer, uint8_t> || std::is_same_v<Integer, char>) {
				char low_part = integer & 0x0F;
				char high_part = (integer & 0xF0) >> 4;
				characters.Add(hex_characters[(unsigned int)high_part]);
				characters.Add(hex_characters[(unsigned int)low_part]);
			}
			else if constexpr (std::is_same_v<Integer, int16_t> || std::is_same_v<Integer, uint16_t>) {
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
				characters.Add(' ');
				characters.Add('(');
				ConvertIntToCharsFormatted(characters, static_cast<int64_t>(integer));
				characters.Add(')');
				characters.Add(' ');
			}
			characters[characters.size] = '\0';
		}

		// returns the number of characters written
		template<typename Parameter>
		ulong2 FormatStringInternal(
			char* end_characters,
			const char* base_characters,
			Parameter parameter,
			const char* string_to_replace
		) {
			const char* string_ptr = strstr(base_characters, string_to_replace);
			if (string_ptr != nullptr) {
				size_t copy_count = (uintptr_t)string_ptr - (uintptr_t)base_characters;
				memcpy(end_characters, base_characters, copy_count);

				Stream<char> temp_stream = Stream<char>(end_characters, copy_count);
				if constexpr (std::is_floating_point_v<Parameter>) {
					if constexpr (std::is_same_v<Parameter, float>) {
						function::ConvertFloatToChars(temp_stream, parameter, 3);
					}
					else if constexpr (std::is_same_v<Parameter, double>) {
						function::ConvertDoubleToChars(temp_stream, parameter, 3);
					}
				}
				else if constexpr (std::is_integral_v<Parameter>) {
					function::ConvertIntToChars(temp_stream, static_cast<int64_t>(parameter));
				}
				else if constexpr (std::is_pointer_v<Parameter>) {
					if constexpr (std::is_same_v<Parameter, const char*>) {
						size_t substring_size = strlen(parameter);
						memcpy(temp_stream.buffer + temp_stream.size, parameter, substring_size);
						temp_stream.size += substring_size;
						temp_stream[temp_stream.size] = '\0';
					}
					else if constexpr (std::is_same_v<Parameter, const wchar_t*>) {
						size_t wide_count = wcslen(parameter);
						function::ConvertWideCharsToASCII(parameter, temp_stream.buffer + temp_stream.size, wide_count, wide_count + 1);
						temp_stream.size += wide_count;
						temp_stream[temp_stream.size] = '\0';
					}
					else {
						function::ConvertIntToHex(temp_stream, (int64_t)parameter);
					}
				}
				else if constexpr (std::is_same_v<Parameter, CapacityStream<wchar_t>> || std::is_same_v<Parameter, Stream<wchar_t>>) {
					CapacityStream<char> placeholder_stream(temp_stream.buffer, temp_stream.size, temp_stream.size + parameter.size + 1);
					function::ConvertWideCharsToASCII(parameter, placeholder_stream);
					temp_stream.size += parameter.size;
					temp_stream[temp_stream.size] = '\0';
				}
				else if constexpr (std::is_same_v<Parameter, CapacityStream<char>> || std::is_same_v<Parameter, Stream<char>>) {
					temp_stream.AddStream(parameter);
				}
				return { temp_stream.size, copy_count };
			}

			return { 0, 0 };
		}

		// returns the count of the characters written;
		template<typename Parameter>
		size_t FormatString(char* destination, const char* base_characters, Parameter parameter) {
			size_t base_character_count = strlen(base_characters);

			ulong2 characters_written = FormatStringInternal(destination, base_characters, parameter, "{#}");
			characters_written.y += 3;

			memcpy(destination + characters_written.x, base_characters + characters_written.y, base_character_count - characters_written.y);
			characters_written.x += base_character_count - characters_written.y;
			destination[characters_written.x] = '\0';
			return characters_written.x;
		}

		// returns the count of the characters written
		template<typename Parameter1, typename Parameter2>
		size_t FormatString(char* destination, const char* base_characters, Parameter1 parameter1, Parameter2 parameter2) {
			size_t base_character_count = strlen(base_characters);

			ulong2 characters_written = FormatStringInternal(destination, base_characters, parameter1, "{#}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter2, "{#}");
			characters_written.y += 3;

			memcpy(destination + characters_written.x, base_characters + characters_written.y, base_character_count - characters_written.y);
			characters_written.x += base_character_count - characters_written.y;
			destination[characters_written.x] = '\0';
			return characters_written.x;
		}

		// returns the count of the characters written
		template<typename Parameter1, typename Parameter2, typename Parameter3>
		size_t FormatString(char* destination, const char* base_characters, Parameter1 parameter1, Parameter2 parameter2, Parameter3 parameter3) {
			size_t base_character_count = strlen(base_characters);

			ulong2 characters_written = FormatStringInternal(destination, base_characters, parameter1, "{#}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter2, "{#}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter3, "{#}");
			characters_written.y += 3;

			memcpy(destination + characters_written.x, base_characters + characters_written.y, base_character_count - characters_written.y);
			characters_written.x += base_character_count - characters_written.y;
			destination[characters_written.x] = '\0';
			return characters_written.x;
		}

		// returns the count of the characters written
		template<typename Parameter1, typename Parameter2, typename Parameter3, typename Parameter4>
		size_t FormatString(char* destination, const char* base_characters, Parameter1 parameter1, Parameter2 parameter2, Parameter3 parameter3, Parameter4 parameter4) {
			size_t base_character_count = strlen(base_characters);

			ulong2 characters_written = FormatStringInternal(destination, base_characters, parameter1, "{#}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter2, "{#}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter3, "{#}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter4, "{#}");
			characters_written.y += 3;

			memcpy(destination + characters_written.x, base_characters + characters_written.y, base_character_count - characters_written.y);
			characters_written.x += base_character_count - characters_written.y;
			destination[characters_written.x] = '\0';
			return characters_written.x;
		}

		// returns the count of the characters written
		template<typename Parameter1, typename Parameter2, typename Parameter3, typename Parameter4, typename Parameter5>
		size_t FormatString(char* destination, const char* base_characters, Parameter1 parameter1, Parameter2 parameter2, Parameter3 parameter3, Parameter4 parameter4, Parameter5 parameter5) {
			size_t base_character_count = strlen(base_characters);

			ulong2 characters_written = FormatStringInternal(destination, base_characters, parameter1, "{#}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter2, "{#}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter3, "{#}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter4, "{#}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter5, "{#}");
			characters_written.y += 3;

			memcpy(destination + characters_written.x, base_characters + characters_written.y, base_character_count - characters_written.y);
			characters_written.x += base_character_count - characters_written.y;
			destination[characters_written.x] = '\0';
			return characters_written.x;
		}

		// returns the count of the characters written
		template<typename Parameter1, typename Parameter2, typename Parameter3, typename Parameter4, typename Parameter5, typename Parameter6>
		size_t FormatString(char* destination, const char* base_characters, Parameter1 parameter1, Parameter2 parameter2, Parameter3 parameter3, Parameter4 parameter4, Parameter5 parameter5, Parameter6 parameter6) {
			size_t base_character_count = strlen(base_characters);

			ulong2 characters_written = FormatStringInternal(destination, base_characters, parameter1, "{#}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter2, "{#}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter3, "{#}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter4, "{#}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter5, "{#}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter6, "{#}");
			characters_written.y += 3;

			memcpy(destination + characters_written.x, base_characters + characters_written.y, base_character_count - characters_written.y);
			characters_written.x += base_character_count - characters_written.y;
			destination[characters_written.x] = '\0';
			return characters_written.x;
		}

		// Idiot C++ bug, char and int8_t does not equal to the same according to the compiler, only signed char
		// and int8_t; So make all the signed version separately
		template<typename Integer>
		void IntegerRange(Integer& min, Integer& max) {
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

		// -----------------------------------------------------------------------------------------------------------------------

		// Allocates a single chunk of memory and then partitions the buffer to each stream
		// without copying the data from the streams
		template<typename Stream0, typename Stream1>
		void CoallesceStreamsEmpty(AllocatorPolymorphic allocator, Stream0& stream0, Stream1& stream1) {
			size_t total_size = stream0.MemoryOf(stream0.size) + stream1.MemoryOf(stream1.size);
			void* allocation = AllocateEx(allocator, total_size);

			// Because we don't know the type of the buffer, we cannot cast it
			// But we can memcpy into it
			memcpy(&stream0, &allocation, sizeof(allocation));

			allocation = function::OffsetPointer(allocation, stream0.MemoryOf(stream0.size));
			memcpy(&stream1, &allocation, sizeof(allocation));
		}

		// Allocates a single chunk of memory and then partitions the buffer to each stream
		// without copying the data from the streams
		template<typename Stream0, typename Stream1, typename Stream2>
		void CoallesceStreamsEmpty(AllocatorPolymorphic allocator, Stream0& stream0, Stream1& stream1, Stream2& stream2) {
			size_t total_size = stream0.MemoryOf(stream0.size) + stream1.MemoryOf(stream1.size) + stream2.MemoryOf(stream2.size);
			void* allocation = AllocateEx(allocator, total_size);

			// Because we don't know the type of the buffer, we cannot cast it
			// But we can memcpy into it
			memcpy(&stream0, &allocation, sizeof(allocation));

			allocation = function::OffsetPointer(allocation, stream0.MemoryOf(stream0.size));
			memcpy(&stream1, &allocation, sizeof(allocation));

			allocation = function::OffsetPointer(allocation, stream1.MemoryOf(stream1.size));
			memcpy(&stream2, &allocation, sizeof(allocation));
		}

		// Allocates a single chunk of memory and then partitions the buffer to each stream
		// without copying the data from the streams
		template<typename Stream0, typename Stream1, typename Stream2, typename Stream3>
		void CoallesceStreamsEmpty(AllocatorPolymorphic allocator, Stream0& stream0, Stream1& stream1, Stream2& stream2, Stream3& stream3) {
			size_t total_size = stream0.MemoryOf(stream0.size) + stream1.MemoryOf(stream1.size) + stream2.MemoryOf(stream2.size) + stream3.MemoryOf(stream3.size);
			void* allocation = AllocateEx(allocator, total_size);

			// Because we don't know the type of the buffer, we cannot cast it
			// But we can memcpy into it
			memcpy(&stream0, &allocation, sizeof(allocation));

			allocation = function::OffsetPointer(allocation, stream0.MemoryOf(stream0.size));
			memcpy(&stream1, &allocation, sizeof(allocation));

			allocation = function::OffsetPointer(allocation, stream1.MemoryOf(stream1.size));
			memcpy(&stream2, &allocation, sizeof(allocation));

			allocation = function::OffsetPointer(allocation, stream2.MemoryOf(stream2.size));
			memcpy(&stream3, &allocation, sizeof(allocation));
		}

		// -----------------------------------------------------------------------------------------------------------------------

		// Allocates a single chunk of memory, partitions the buffer to each stream, and the copies the data into it
		template<typename Stream0, typename Stream1>
		void CoallesceStreams(AllocatorPolymorphic allocator, Stream0& stream0, Stream1& stream1) {
			size_t total_size = stream0.MemoryOf(stream0.size) + stream1.MemoryOf(stream1.size);
			void* allocation = AllocateEx(allocator, total_size);
			
			// Because we don't know the type of the buffer, we cannot cast it
			// But we can memcpy into it
			stream0.CopyTo(allocation);
			memcpy(&stream0, &allocation, sizeof(allocation));

			allocation = function::OffsetPointer(allocation, stream0.MemoryOf(stream0.size));
			stream1.CopyTo(allocation);
			memcpy(&stream1, &allocation, sizeof(allocation));
		}

		// Allocates a single chunk of memory, partitions the buffer to each stream, and the copies the data into it
		template<typename Stream0, typename Stream1, typename Stream2>
		void CoallesceStreams(AllocatorPolymorphic allocator, Stream0& stream0, Stream1& stream1, Stream2& stream2) {
			size_t total_size = stream0.MemoryOf(stream0.size) + stream1.MemoryOf(stream1.size) + stream2.MemoryOf(stream2.size);
			void* allocation = AllocateEx(allocator, total_size);

			// Because we don't know the type of the buffer, we cannot cast it
			// But we can memcpy into it
			stream0.CopyTo(allocation);
			memcpy(&stream0, &allocation, sizeof(allocation));

			allocation = function::OffsetPointer(allocation, stream0.MemoryOf(stream0.size));
			stream1.CopyTo(allocation);
			memcpy(&stream1, &allocation, sizeof(allocation));

			allocation = function::OffsetPointer(allocation, stream1.MemoryOf(stream1.size));
			stream2.CopyTo(allocation);
			memcpy(&stream2, &allocation, sizeof(allocation));
		}

		// Allocates a single chunk of memory, partitions the buffer to each stream, and the copies the data into it
		template<typename Stream0, typename Stream1, typename Stream2, typename Stream3>
		void CoallesceStreams(AllocatorPolymorphic allocator, Stream0& stream0, Stream1& stream1, Stream2& stream2, Stream3& stream3) {
			size_t total_size = stream0.MemoryOf(stream0.size) + stream1.MemoryOf(stream1.size) + stream2.MemoryOf(stream2.size) + stream3.MemoryOf(stream3.size);
			void* allocation = AllocateEx(allocator, total_size);

			// Because we don't know the type of the buffer, we cannot cast it
			// But we can memcpy into it
			stream0.CopyTo(allocation);
			memcpy(&stream0, &allocation, sizeof(allocation));

			allocation = function::OffsetPointer(allocation, stream0.MemoryOf(stream0.size));
			stream1.CopyTo(allocation);
			memcpy(&stream1, &allocation, sizeof(allocation));

			allocation = function::OffsetPointer(allocation, stream1.MemoryOf(stream1.size));
			stream2.CopyTo(allocation);
			memcpy(&stream2, &allocation, sizeof(allocation));

			allocation = function::OffsetPointer(allocation, stream2.MemoryOf(stream2.size));
			stream3.CopyTo(allocation);
			memcpy(&stream3, &allocation, sizeof(allocation));
		}

		// -----------------------------------------------------------------------------------------------------------------------

		// Uses a fast SIMD compare, in this way you don't need to rely on the
		// compiler to generate for you the SIMD search. Returns -1 if it doesn't
		// find the value. Only types of 1, 2, 4 or 8 bytes are accepted
		ECSENGINE_API size_t SearchBytes(const void* data, size_t element_count, const void* value_to_search, size_t byte_size);

	}

}