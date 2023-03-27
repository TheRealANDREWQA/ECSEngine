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

#define ECS_FORMAT_TEMP_STRING(string_name, base_characters, ...) ECS_TEMP_ASCII_STRING(string_name, 2048); \
string_name.size = function::FormatString(string_name.buffer, base_characters, __VA_ARGS__); \
string_name.AssertCapacity();

#define ECS_FORMAT_STRING(string, base_characters, ...) (string).size += function::FormatString((string).buffer + (string).size, base_characters, __VA_ARGS__); \
(string).AssertCapacity();

#define ECS_FORMAT_ERROR_MESSAGE(error_message, base_characters, ...) if (error_message != nullptr) { \
	error_message->size += function::FormatString(error_message->buffer + error_message->size, base_characters, __VA_ARGS__); \
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
		ECSENGINE_API size_t FindWhitespaceCharactersCount(Stream<char> string, char separator_token = ' ', CapacityStream<unsigned int>*stack_buffer = nullptr);

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
		size_t ConvertFloatingPointToChars(Stream& chars, FloatingType value, size_t precision = 2) {
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
		char ConvertCharactersToBool(Stream<char> characters);
		
		// Returns 0 or 1 if it is a boolean (i.e. false or true in the characters)
		// Else returns -1
		char ConvertCharactersToBool(Stream<wchar_t> characters);

		// It will advance the characters such that it will be at the end of the parsed range
		template<typename CharacterType, typename ReturnType, typename Functor>
		ReturnType ParseType(Stream<CharacterType>* characters, CharacterType delimiter, Functor&& functor) {
			const CharacterType* initial_buffer = characters->buffer;

			CharacterType current_character = characters->buffer[0];
			bool continue_search = current_character != delimiter;
			while (characters->size > 0 && continue_search) {
				characters->buffer++;
				characters->size--;
				current_character = characters->buffer[0];
				continue_search = current_character != delimiter;
			}

			Stream<CharacterType> parse_range = { initial_buffer, function::PointerDifference(characters->buffer, initial_buffer) / sizeof(CharacterType) };
			return functor(parse_range);
		}

		// It will advance the pointer such that it will be at the delimiter after the value
		template<typename Integer, typename CharacterType>
		Integer ParseInteger(Stream<CharacterType>* characters, CharacterType delimiter) {
			return ParseType<CharacterType, Integer>(characters, delimiter, [](Stream<CharacterType> stream_characters) {
				return ConvertCharactersToInt<Integer>(stream_characters);
			});
		}

		// It will advance the pointer such that it will be at the delimiter after the value
		template<typename CharacterType>
		ECSENGINE_API float ParseFloat(Stream<CharacterType>* characters, CharacterType delimiter);

		// It will advance the pointer such that it will be at the delimiter after the value
		template<typename CharacterType>
		ECSENGINE_API double ParseDouble(Stream<CharacterType>* characters, CharacterType delimiter);

		// It will advance the pointer such that it will be at the delimiter after the value
		template<typename CharacterType>
		ECSENGINE_API bool ParseBool(Stream<CharacterType>* characters, CharacterType delimiter);

		// Parses any type that is a bool/int/float/double of at most 4 components
		// It will use a pair {} to parse multiple values and if it founds the ignore tag, it will return a double4 with DBL_MAX as values
		template<typename CharacterType>
		ECSENGINE_API double4 ParseDouble4(
			Stream<CharacterType>* characters, 
			CharacterType external_delimiter, 
			CharacterType internal_delimiter = Character<CharacterType>(','),
			Stream<CharacterType> ignore_tag = { nullptr, 0 }
		);

		// Parses the values as ints.
		template<typename Integer, typename CharacterType>
		void ParseIntegers(Stream<CharacterType> characters, CharacterType delimiter, CapacityStream<Integer>& values) {
			while (characters.size > 0) {
				values.AddSafe(ParseInteger<Integer>(&characters, delimiter));
			}
		}

		// Parses the values as floats.
		template<typename CharacterType>
		ECSENGINE_API void ParseFloats(
			Stream<CharacterType> characters, 
			CharacterType delimiter, 
			CapacityStream<float>& values
		);

		// Parses the values as doubles
		template<typename CharacterType>
		ECSENGINE_API void ParseDoubles(
			Stream<CharacterType> characters,
			CharacterType delimiter,
			CapacityStream<double>& values
		);

		// Parses the values as bools
		template<typename CharacterType>
		ECSENGINE_API void ParseBools(
			Stream<CharacterType> characters,
			CharacterType delimiter,
			CapacityStream<bool>& values
		);

		// Parses any "fundamental type" (including vectors of 2, 3 and 4 components)
		// This can be used for a generalized read
		template<typename CharacterType>
		ECSENGINE_API void ParseDouble4s(
			Stream<CharacterType> characters,
			CapacityStream<double4>& values,
			CharacterType external_delimiter,
			CharacterType internal_delimiter = Character<CharacterType>(','),
			Stream<CharacterType> ignore_tag = { nullptr, 0 }
		);

		template<typename Allocator>
		void* Copy(Allocator* allocator, const void* data, size_t data_size, size_t alignment = 8) {
			void* allocation = allocator->Allocate(data_size, alignment);
			memcpy(allocation, data, data_size);
			return allocation;
		}

		ECSENGINE_API void* Copy(AllocatorPolymorphic allocator, const void* data, size_t data_size, size_t alignment = 8);

		ECSENGINE_API Stream<void> Copy(AllocatorPolymorphic allocator, Stream<void> data, size_t alignment = 8);

		// If data size is 0, it will return the data back
		ECSENGINE_API void* CopyNonZero(AllocatorPolymorphic allocator, const void* data, size_t data_size, size_t alignment = 8);

		// If data size is 0, it will return the data back
		ECSENGINE_API Stream<void> CopyNonZero(AllocatorPolymorphic allocator, Stream<void> data, size_t alignment = 8);

		// If data size is zero, it will return data, else it will make a copy and return that instead
		template<typename Allocator>
		void* CopyNonZero(Allocator* allocator, const void* data, size_t data_size, size_t alignment = 8) {
			if (data_size > 0) {
				void* allocation = allocator->Allocate(data_size, alignment);
				memcpy(allocation, data, data_size);
				return allocation;
			}
			return (void*)data;
		}

		// The offset is added to the sequence
		template<typename Stream>
		ECSENGINE_API void MakeSequence(Stream stream, size_t offset = 0);

		// The offset is added to the sequence
		template<typename Stream>
		ECSENGINE_API void MakeDescendingSequence(Stream stream, size_t offset = 0);

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
		template<typename Integer, typename CharacterType, typename Stream>
		Integer ConvertCharactersToIntImpl(Stream stream, size_t& digit_count) {
			Integer integer = Integer(0);
			size_t starting_index = stream[0] == Character<CharacterType>('-') ? 1 : 0;
			digit_count = 0;

			for (size_t index = starting_index; index < stream.size; index++) {
				if (stream[index] >= Character<CharacterType>('0') && stream[index] <= Character<CharacterType>('9')) {
					integer = integer * 10 + stream[index] - Character<CharacterType>('0');
					digit_count++;
				}
			}
			integer = starting_index == 1 ? -integer : integer;

			return integer;
		}

		// non digit characters are discarded
		template<typename Integer, typename CharacterType, typename Stream>
		Integer ConvertCharactersToIntImpl(Stream stream) {
			// Dummy
			size_t digit_count = 0;
			return ConvertCharactersToIntImpl<Integer, CharacterType>(stream, digit_count);
		}

		template<typename Stream, typename Value, typename Function>
		void GetMinFromStream(const Stream& input, Value& value, Function&& function) {
			for (size_t index = 0; index < input.size; index++) {
				Value current_value = function(input[index]);
				value = std::min(value, current_value);
			}
		}

		template<typename Stream, typename Value, typename Function>
		void GetMaxFromStream(const Stream& input, Value& value, Function&& function) {
			for (size_t index = 0; index < input.size; index++) {
				Value current_value = function(input[index]);
				value = std::max(value, current_value);
			}
		}

		template<typename Stream, typename Value, typename Function>
		void GetExtremesFromStream(const Stream& input, Value& min, Value& max, Function&& accessor) {
			for (size_t index = 0; index < input.size; index++) {
				Value current_value = accessor(input[index]);
				min = std::min(min, current_value);
				max = std::max(max, current_value);
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

		namespace Internal {
			// Returns how many total characters have been written in the x component
		// and in the y component the offset into the base characters where the string was found
			template<typename Parameter>
			ulong2 FormatStringInternal(
				char* end_characters,
				const char* base_characters,
				Parameter parameter
			) {
				const char* string_ptr = strstr(base_characters, "{#}");
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

			extern template ECSENGINE_API ulong2 FormatStringInternal<const char*>(char*, const char*, const char*);
			extern template ECSENGINE_API ulong2 FormatStringInternal<const wchar_t*>(char*, const char*, const wchar_t*);
			extern template ECSENGINE_API ulong2 FormatStringInternal<Stream<char>>(char*, const char*, Stream<char>);
			extern template ECSENGINE_API ulong2 FormatStringInternal<Stream<wchar_t>>(char*, const char*, Stream<wchar_t>);
			extern template ECSENGINE_API ulong2 FormatStringInternal<CapacityStream<char>>(char*, const char*, CapacityStream<char>);
			extern template ECSENGINE_API ulong2 FormatStringInternal<CapacityStream<wchar_t>>(char*, const char*, CapacityStream<wchar_t>);
			extern template ECSENGINE_API ulong2 FormatStringInternal<unsigned int>(char*, const char*, unsigned int);
			extern template ECSENGINE_API ulong2 FormatStringInternal<void*>(char*, const char*, void*);
			extern template ECSENGINE_API ulong2 FormatStringInternal<float>(char*, const char*, float);
			extern template ECSENGINE_API ulong2 FormatStringInternal<double>(char*, const char*, double);

			template<typename FirstParameter, typename... Parameters>
			void FormatStringImpl(char* destination, const char* base_characters, ulong2& written_characters, const FirstParameter& first_parameter, Parameters... parameters) {
				ulong2 current_written_characters = FormatStringInternal(destination + written_characters.x, base_characters + written_characters.y, first_parameter);
				written_characters.x += current_written_characters.x;
				written_characters.y += current_written_characters.y + 3;

				if constexpr (sizeof...(Parameters) > 0) {
					FormatStringImpl(destination, base_characters, written_characters, parameters...);
				}
			}

		}

		// Returns the count of the characters written
		template<typename... Parameters>
		size_t FormatString(char* destination, const char* base_characters, Parameters... parameters) {
			ulong2 written_characters = { 0, 0 };
			Internal::FormatStringImpl(destination, base_characters, written_characters, parameters...);

			// The remaining characters still need to be copied
			size_t base_character_count = strlen(base_characters);
			memcpy(destination + written_characters.x, base_characters + written_characters.y, base_character_count - written_characters.y);
			written_characters.x += base_character_count - written_characters.y;
			destination[written_characters.x] = '\0';
			return written_characters.x;
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

		// -----------------------------------------------------------------------------------------------------------------------

		// Returns a + b
		// If it overflows, it will set it to the maximum value for that integer
		template<typename Integer>
		ECS_INLINE Integer SaturateAdd(Integer a, Integer b) {
			Integer new_value = a + b;
			Integer min, max;
			function::IntegerRange(min, max);
			return new_value < a ? max : new_value;
		}

		// Stores the result in the given pointer and returns the previous value at that pointer
		// If it overflows, it will set it to the maximum value for that integer
		template<typename Integer>
		ECS_INLINE Integer SaturateAdd(Integer* a, Integer b) {
			Integer old_value = *a;
			*a = SaturateAdd(*a, b);
			return old_value;
		}

		// Returns a - b
		// If it overflows, it will set it to the min value for that integer
		template<typename Integer>
		ECS_INLINE Integer SaturateSub(Integer a, Integer b) {
			Integer new_value = a - b;
			Integer min, max;
			function::IntegerRange(min, max);
			return new_value > a ? min : new_value;
		}

		// Stores the result in the given pointer and returns the previous value at that pointer
		// If it overflows, it will set it to the min value for that integer
		template<typename Integer>
		ECS_INLINE Integer SaturateSub(Integer* a, Integer b) {
			Integer old_value = *a;
			*a = SaturateSub(*a, b);
			return old_value;
		}

		// -----------------------------------------------------------------------------------------------------------------------

		// Writes the stream data right after the type and sets the size
		// Returns the total write size
		template<typename Type>
		unsigned int CoallesceStreamIntoType(Type* type, Stream<void> stream) {
			void* location = function::OffsetPointer(type, sizeof(*type));
			memcpy(location, stream.buffer, stream.size);
			*type->Size() = stream.size;
			return sizeof(*type) + stream.size;
		}

		// -----------------------------------------------------------------------------------------------------------------------

		template<typename Type>
		inline Stream<void> GetCoallescedStreamFromType(Type* type) {
			void* location = function::OffsetPointer(type, sizeof(*type));
			return { location, *type->Size() };
		}

		// -----------------------------------------------------------------------------------------------------------------------

		template<typename Type>
		inline Type* CreateCoallescedStreamIntoType(void* buffer, Stream<void> stream, unsigned int* write_size) {
			Type* type = (Type*)buffer;
			*write_size = CoallesceStreamIntoType(type, stream);
			return type;
		}

		// -----------------------------------------------------------------------------------------------------------------------

		// Does not set the data size inside the type
		template<typename Type>
		Type* CreateCoallescedStreamsIntoType(void* buffer, Stream<Stream<void>> buffers, unsigned int* write_size) {
			Type* type = (Type*)buffer;
			unsigned int offset = 0;
			for (size_t index = 0; index < buffers.size; index++) {
				buffers[index].CopyTo(function::OffsetPointer(buffer, sizeof(*type) + offset));
				offset += buffers[index].size;
			}
			*write_size = sizeof(*type) + offset;
			return type;
		}

		// Sizes needs to be byte sizes of the coallesced streams
		template<typename Type>
		Stream<void> GetCoallescedStreamFromType(Type* type, unsigned int stream_index, unsigned int* sizes) {
			unsigned int offset = 0;
			for (unsigned int index = 0; index < stream_index; index++) {
				offset += sizes[index];
			}
			return { function::OffsetPointer(type, sizeof(*type) + offset), sizes[stream_index] };
		}

		// -----------------------------------------------------------------------------------------------------------------------

		template<typename FirstStream, typename... Streams>
		size_t StreamsTotalSize(FirstStream first_stream, Streams... streams) {
			size_t total_size = 0;
			if constexpr (std::is_pointer_v<FirstStream>) {
				total_size = first_stream->MemoryOf(first_stream->size);
			}
			else {
				total_size = first_stream.MemoryOf(first_stream.size);
			}
			
			if constexpr (sizeof...(Streams) > 0) {
				total_size += StreamsTotalSize(streams...);
			}
			return total_size;
		}

		template<typename FirstStream, typename... Streams>
		void CoallesceStreamsEmptyImplementation(void* allocation, FirstStream* first_stream, Streams... streams) {
			// Use memcpy to assign the pointer since in this case we don't need to cast to the stream type
			memcpy(&first_stream->buffer, &allocation, sizeof(allocation));

			if constexpr (sizeof...(Streams) > 0) {
				allocation = function::OffsetPointer(allocation, first_stream->MemoryOf(first_stream->size));
				CoallesceStreamsEmptyImplementation(allocation, streams...);
			}
		}

		template<typename FirstStream, typename... Streams>
		void CoallesceStreamsEmpty(AllocatorPolymorphic allocator, FirstStream* first_stream, Streams... streams) {
			size_t total_size = StreamsTotalSize(first_stream, streams...);
			void* allocation = AllocateEx(allocator, total_size);

			CoallesceStreamsEmptyImplementation(allocastion, first_stream, streams...);
		}
		
		// -----------------------------------------------------------------------------------------------------------------------

		template<typename FirstStream, typename... Streams>
		void CoallesceStreamsImplementation(void* allocation, FirstStream* first_stream, Streams... streams) {
			// Use memcpy to assign the pointer since in this case we don't need to cast to the stream type
			first_stream->CopyTo(allocation);
			memcpy(&first_stream->buffer, &allocation, sizeof(allocation));

			if constexpr (sizeof...(Streams) > 0) {
				allocation = function::OffsetPointer(allocation, first_stream->MemoryOf(first_stream->size));
				CoallesceStreamsImplementation(allocation, streams...);
			}
		}

		template<typename FirstStream, typename... Streams>
		void CoallesceStreams(AllocatorPolymorphic allocator, FirstStream* first_stream, Streams... streams) {
			size_t total_size = stream0.MemoryOf(stream0.size) + stream1.MemoryOf(stream1.size);
			void* allocation = AllocateEx(allocator, total_size);

			CoallesceStreamsImplementation(allocation, first_stream, streams...);
		}

		// -----------------------------------------------------------------------------------------------------------------------

		// The functor needs to return the string from the type
		template<typename Type, typename CharacterType, typename Functor>
		unsigned int FindString(Stream<CharacterType> string, Stream<Type> other, Functor&& functor) {
			for (unsigned int index = 0; index < other.size; index++) {
				if (function::CompareStrings(string, functor(other[index]))) {
					return index;
				}
			}
			return -1;
		}

		// -----------------------------------------------------------------------------------------------------------------------

	}

}