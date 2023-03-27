#include "ecspch.h"
#include "FunctionInterfaces.h"
#include "Function.h"
#include "Assert.h"
#include "../Math/VCLExtensions.h"

namespace ECSEngine {

	namespace function {

		// ----------------------------------------------------------------------------------------------------------

		template<typename FloatingPoint, typename CharacterType>
		FloatingPoint ConvertCharactersToFloatingPoint(Stream<CharacterType> stream) {
			FloatingPoint value = 0;
			size_t starting_index = stream[0] == '-' || stream[0] == '+' ? 1 : 0;

			size_t dot_index = stream.size;
			for (size_t index = 0; index < stream.size; index++) {
				if (stream[index] == '.') {
					dot_index = index;
					break;
				}
			}
			if (dot_index < stream.size) {
				int64_t integral_part = ConvertCharactersToInt(Stream<CharacterType>(stream.buffer + starting_index, dot_index - starting_index));

				size_t fractional_digit_count = 0;
				int64_t fractional_part = ConvertCharactersToInt(Stream<CharacterType>(stream.buffer + dot_index + 1, stream.size - dot_index - 1), fractional_digit_count);

				FloatingPoint integral_float = static_cast<FloatingPoint>(integral_part);
				FloatingPoint fractional_float = static_cast<FloatingPoint>(fractional_part);

				// reversed in order to speed up calculations
				FloatingPoint fractional_power = 0.1;
				for (size_t index = 1; index < fractional_digit_count; index++) {
					fractional_power *= 0.1;
				}
				fractional_float *= fractional_power;
				FloatingPoint value = integral_float + fractional_float;
				if (stream[0] == '-') {
					value = -value;
				}
				return value;
			}
			else {
				if (stream.size > 0) {
					int64_t integer = ConvertCharactersToInt(Stream<CharacterType>(stream.buffer + starting_index, stream.size - starting_index));
					value = static_cast<FloatingPoint>(integer);
					if (stream[0] == '-') {
						value = -value;
					}
				}
				return value;
			}
		}

		// ----------------------------------------------------------------------------------------------------------

		float ConvertCharactersToFloat(Stream<char> characters) {
			return ConvertCharactersToFloatingPoint<float>(characters);
		}

		float ConvertCharactersToFloat(Stream<wchar_t> characters) {
			return ConvertCharactersToFloatingPoint<float>(characters);
		}

		// ----------------------------------------------------------------------------------------------------------

		double ConvertCharactersToDouble(Stream<char> characters) {
			return ConvertCharactersToFloatingPoint<double>(characters);
		}

		double ConvertCharactersToDouble(Stream<wchar_t> characters) {
			return ConvertCharactersToFloatingPoint<double>(characters);
		}

		// ----------------------------------------------------------------------------------------------------------

		int64_t ConvertCharactersToInt(Stream<char> stream) {
			return ConvertCharactersToIntImpl<int64_t, char>(stream);
		}

		int64_t ConvertCharactersToInt(Stream<wchar_t> stream) {
			return ConvertCharactersToIntImpl<int64_t, wchar_t>(stream);
		}

		// ----------------------------------------------------------------------------------------------------------

		int64_t ConvertCharactersToInt(Stream<char> stream, size_t& digit_count) {
			return ConvertCharactersToIntImpl<int64_t, char>(stream, digit_count);
		}

		int64_t ConvertCharactersToInt(Stream<wchar_t> stream, size_t& digit_count) {
			return ConvertCharactersToIntImpl<int64_t, wchar_t>(stream, digit_count);
		}

		// ----------------------------------------------------------------------------------------------------------

#define CONVERT_TYPE_TO_CHARS_FROM_ASCII_TO_WIDE(convert_function, chars, ...) char characters[512]; \
		Stream<char> ascii(characters, 0); \
\
		size_t character_count = function::convert_function(ascii, __VA_ARGS__); \
		function::ConvertASCIIToWide(chars.buffer + chars.size, ascii, character_count + 1); \
		chars.size += character_count; \
\
		return character_count;
		
		template<typename StreamType>
		size_t ConvertIntToCharsFormatted(StreamType& chars, int64_t value) {
			static_assert(std::is_same_v<StreamType, Stream<char>> || std::is_same_v<StreamType, CapacityStream<char>> ||
				std::is_same_v<StreamType, Stream<wchar_t>> || std::is_same_v<StreamType, CapacityStream<wchar_t>>);

			// ASCII implementation
			if constexpr (std::is_same_v<StreamType, Stream<char>> || std::is_same_v<StreamType, CapacityStream<char>>) {
				size_t initial_size = chars.size;
				if (value < 0) {
					chars[chars.size] = '-';
					chars.size++;
					value = -value;
				}

				size_t starting_swap_index = chars.size;

				size_t count = 0;
				char temp_characters[128];
				if (value == 0) {
					chars[chars.size++] = '0';
				}
				else {
					while (value != 0) {
						temp_characters[count++] = value % 10 + '0';
						value /= 10;
					}
				}

				size_t apostrophe_count = 0;
				for (int64_t index = 0; index < count; index++) {
					unsigned int copy_index = count - index - 1;
					chars.Add(temp_characters[copy_index]);
					if (copy_index % 3 == 0 && copy_index > 0) {
						chars.Add(',');
						apostrophe_count++;
					}
				}
				chars[chars.size] = '\0';

				return chars.size - initial_size;
			}
			// Wide implementation - convert to a ASCII stack buffer then commit wides to the stream
			else {
				CONVERT_TYPE_TO_CHARS_FROM_ASCII_TO_WIDE(ConvertIntToCharsFormatted, chars, value);
			}
		}

		ECS_TEMPLATE_FUNCTION_4_BEFORE(size_t, ConvertIntToCharsFormatted, Stream<char>&, CapacityStream<char>&, Stream<wchar_t>&, CapacityStream<wchar_t>&, int64_t);
		
		// ----------------------------------------------------------------------------------------------------------

		template<typename StreamType>
		size_t ConvertIntToChars(StreamType& chars, int64_t value) {
			static_assert(std::is_same_v<StreamType, Stream<char>> || std::is_same_v<StreamType, CapacityStream<char>> ||
				std::is_same_v<StreamType, Stream<wchar_t>> || std::is_same_v<StreamType, CapacityStream<wchar_t>>);

			// ASCII implementation
			if constexpr (std::is_same_v<StreamType, Stream<char>> || std::is_same_v<StreamType, CapacityStream<char>>) {
				size_t initial_size = chars.size;

				if (value < 0) {
					chars.Add('-');
					value = -value;
				}

				size_t starting_swap_index = chars.size;

				if (value == 0) {
					chars.Add('0');
				}
				else {
					while (value != 0) {
						chars.Add(value % 10 + '0');
						value /= 10;
					}
				}
				for (size_t index = 0; index < (chars.size - starting_swap_index) >> 1; index++) {
					chars.Swap(index + starting_swap_index, chars.size - 1 - index);
				}
				chars[chars.size] = '\0';

				return chars.size - initial_size;
			}
			// Wide implementation - convert to an ASCII stack buffer then commit to the wide stream
			else {
				CONVERT_TYPE_TO_CHARS_FROM_ASCII_TO_WIDE(ConvertIntToChars, chars, value);
			}
		}

		ECS_TEMPLATE_FUNCTION_4_BEFORE(size_t, ConvertIntToChars, Stream<char>&, CapacityStream<char>&, Stream<wchar_t>&, CapacityStream<wchar_t>&, int64_t);

		// ----------------------------------------------------------------------------------------------------------

		template<bool unaligned = false>
		void avx2_copy_32multiple(void* destination, const void* source, size_t bytes) {
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

		ECS_TEMPLATE_FUNCTION_BOOL(void, avx2_copy_32multiple, void*, const void*, size_t);

		// ----------------------------------------------------------------------------------------------------------

		template<bool unaligned = false>
		void avx2_copy_128multiple(void* destination, const void* source, size_t bytes) {
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

		ECS_TEMPLATE_FUNCTION_BOOL(void, avx2_copy_128multiple, void*, const void*, size_t);

		// ----------------------------------------------------------------------------------------------------------

		template<bool exact_length>
		wchar_t* ConvertASCIIToWide(const char* pointer) {
			if constexpr (!exact_length) {
				wchar_t* w_string = new wchar_t[1024];
				MultiByteToWideChar(CP_ACP, 0, pointer, -1, w_string, 1024);
				return w_string;
			}
			else {
				size_t char_length = strlen(pointer);
				wchar_t* w_string = new wchar_t[char_length + 1];
				MultiByteToWideChar(CP_ACP, 0, pointer, char_length + 1, w_string, char_length + 1);
				return w_string;
			}
		}

		ECS_TEMPLATE_FUNCTION_BOOL(wchar_t*, ConvertASCIIToWide, const char*);

		// ----------------------------------------------------------------------------------------------------------

		size_t FindWhitespaceCharactersCount(Stream<char> string, char separator_token, CapacityStream<unsigned int>* stack_buffer)
		{
			size_t count = 0;
			
			Stream<char> token_stream = function::FindFirstCharacter(string, separator_token);
			Stream<char> new_line_stream = function::FindFirstCharacter(string, '\n');
			if (stack_buffer == nullptr) {
				while (token_stream.buffer != nullptr && new_line_stream.buffer != nullptr) {
					count += 2;

					token_stream.buffer += 1;
					token_stream.size -= 1;
					
					new_line_stream.buffer += 1;
					new_line_stream.size -= 1;

					token_stream = function::FindFirstCharacter(token_stream, separator_token);
					new_line_stream = function::FindFirstCharacter(new_line_stream, '\n');
				}

				while (token_stream.buffer != nullptr) {
					count++;
					token_stream.buffer += 1;
					token_stream.size -= 1;
					token_stream = function::FindFirstCharacter(token_stream, separator_token);
				}

				while (new_line_stream.buffer != nullptr) {
					count++;
					new_line_stream.buffer += 1;
					new_line_stream.size -= 1;
					new_line_stream = function::FindFirstCharacter(new_line_stream, '\n');
				}
			}
			else {
				while (token_stream.buffer != nullptr && new_line_stream.buffer != nullptr) {
					count++;
					if (stack_buffer->size < stack_buffer->capacity) {
						unsigned int token_difference = token_stream.buffer - string.buffer;
						unsigned int new_line_difference = new_line_stream.buffer - string.buffer;

						if (token_difference < new_line_difference) {
							stack_buffer->Add(token_difference);

							token_stream.buffer += 1;
							token_stream.size -= 1;

							token_stream = function::FindFirstCharacter(token_stream, separator_token);
						}
						else {
							stack_buffer->Add(new_line_difference);

							new_line_stream.buffer += 1;
							new_line_stream.size -= 1;

							new_line_stream = function::FindFirstCharacter(new_line_stream, '\n');
						}
					}
				}

				while (token_stream.buffer != nullptr) {
					count++;

					if (stack_buffer->size < stack_buffer->capacity) {
						stack_buffer->Add(token_stream.buffer - string.buffer);
					}

					token_stream.buffer += 1;
					token_stream.size -= 1;
					token_stream = function::FindFirstCharacter(token_stream, separator_token);
				}

				while (new_line_stream.buffer != nullptr) {
					count++;

					if (stack_buffer->size < stack_buffer->capacity) {
						stack_buffer->Add(new_line_stream.buffer - string.buffer);
					}

					new_line_stream.buffer += 1;
					new_line_stream.size -= 1;
					new_line_stream = function::FindFirstCharacter(new_line_stream, '\n');
				}
			}
			return count;
		}

		// ----------------------------------------------------------------------------------------------------------

		// it searches for spaces and next line characters
		template<typename WordStream>
		void FindWhitespaceCharacters(WordStream& spaces, Stream<char> string, char separator_token) {
			size_t position = 0;

			Stream<char> token_stream = function::FindFirstCharacter(string, separator_token);
			Stream<char> new_line_stream = function::FindFirstCharacter(string, '\n');

			while (token_stream.buffer != nullptr && new_line_stream.buffer != nullptr) {
				unsigned int token_difference = token_stream.buffer - string.buffer;
				unsigned int new_line_difference = new_line_stream.buffer - string.buffer;

				if (token_difference < new_line_difference) {
					spaces.Add(token_difference);

					token_stream.buffer += 1;
					token_stream.size -= 1;

					token_stream = function::FindFirstCharacter(token_stream, separator_token);
				}
				else {
					spaces.Add(new_line_difference);

					new_line_stream.buffer += 1;
					new_line_stream.size -= 1;

					new_line_stream = function::FindFirstCharacter(new_line_stream, '\n');
				}
			}

			while (token_stream.buffer != nullptr) {
				spaces.Add(token_stream.buffer - string.buffer);

				token_stream.buffer += 1;
				token_stream.size -= 1;
				token_stream = function::FindFirstCharacter(token_stream, separator_token);
			}

			while (new_line_stream.buffer != nullptr) {
				spaces.Add(new_line_stream.buffer - string.buffer);

				new_line_stream.buffer += 1;
				new_line_stream.size -= 1;
				new_line_stream = function::FindFirstCharacter(new_line_stream, '\n');
			}
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(void, FindWhitespaceCharacters, Stream<unsigned int>&, CapacityStream<unsigned int>&, Stream<char>, char);

		// ----------------------------------------------------------------------------------------------------------

		template<typename WordStream>
		void ParseWordsFromSentence(Stream<char> sentence, WordStream& words) {
			ParseWordsFromSentence(sentence, " \n", words);
		}

		ECS_TEMPLATE_FUNCTION_2_AFTER(void, ParseWordsFromSentence, Stream<uint2>&, CapacityStream<uint2>&, Stream<char>);

		// ----------------------------------------------------------------------------------------------------------

		template<typename WordStream>
		void ParseWordsFromSentence(Stream<char> sentence, Stream<char> tokens, WordStream& words) {
			ECS_STACK_CAPACITY_STREAM_DYNAMIC(Stream<char>, token_positions, tokens.size);
			ECS_STACK_CAPACITY_STREAM_DYNAMIC(char, token_mapping, tokens.size);

			for (size_t index = 0; index < tokens.size; index++) {
				token_positions[index] = function::FindFirstCharacter(sentence, tokens[index]);
				token_mapping[index] = tokens[index];
			}

			auto get_min_position = [&]() {
				size_t offset = LONGLONG_MAX;
				size_t index_of_min = -1;
				for (size_t index = 0; index < token_positions.size; index++) {
					size_t difference = token_positions[index].buffer - sentence.buffer;
					if (offset > difference) {
						offset = difference;
						index_of_min = index;
					}
				}

				// Look again for that character
				token_positions[index_of_min].buffer += 1;
				token_positions[index_of_min].size -= 1;
				token_positions[index_of_min] = function::FindFirstCharacter(token_positions[index_of_min], token_mapping[index_of_min]);
				if (token_positions[index_of_min].buffer == nullptr) {
					// Replace it
					token_positions.RemoveSwapBack(index_of_min);
					token_mapping.RemoveSwapBack(index_of_min);
				}

				return offset;
			};
			
			size_t starting_position = 0;
			while (token_positions.size > 0) {
				size_t minimum = get_min_position();
				if (minimum > starting_position + 1) {
					words.Add({ (unsigned int)starting_position, (unsigned int)(minimum - starting_position - 1) });
				}
				starting_position = minimum + 1;
			}
		}

		ECS_TEMPLATE_FUNCTION_2_AFTER(void, ParseWordsFromSentence, Stream<uint2>&, CapacityStream<uint2>&, Stream<char>, Stream<char>);

		// ----------------------------------------------------------------------------------------------------------

		template<typename WordStream>
		void ParseWordsFromSentence(Stream<char> sentence, char token, WordStream& words) {
			Stream<char> token_stream = function::FindFirstCharacter(sentence, token);

			size_t starting_position = 0;
			while (token_stream.buffer != nullptr) {
				size_t current_position = token_stream.buffer - sentence.buffer;
				if (current_position > starting_position + 1) {
					words.Add(uint2((unsigned int)(token_stream.buffer - sentence.buffer), (unsigned int)(current_position - starting_position - 1)));
				}
				starting_position = current_position + 1;

				token_stream.buffer += 1;
				token_stream.size -= 1;
				token_stream = function::FindFirstCharacter(token_stream, token);
			}
		}

		ECS_TEMPLATE_FUNCTION_2_AFTER(void, ParseWordsFromSentence, Stream<uint2>&, CapacityStream<uint2>&, Stream<char>, char);

		// ----------------------------------------------------------------------------------------------------------

		template<typename StreamType>
		size_t ConvertFloatingPointIntegerToChars(StreamType& chars, int64_t integer, size_t precision) {
			static_assert(std::is_same_v<StreamType, Stream<char>> || std::is_same_v<StreamType, CapacityStream<char>> ||
				std::is_same_v<StreamType, Stream<wchar_t>> || std::is_same_v<StreamType, CapacityStream<wchar_t>>);

			// ASCII implementation
			if constexpr (std::is_same_v<StreamType, Stream<char>> || std::is_same_v<StreamType, CapacityStream<char>>) {
				size_t initial_size = chars.size;
				ConvertIntToChars(chars, integer);

				size_t starting_swap_index = integer < 0 ? 1 : 0 + initial_size;

				if (precision > 0) {
					if (chars.size - starting_swap_index <= precision) {
						size_t swap_count = precision + 2 + starting_swap_index - chars.size;
						for (int64_t index = chars.size - 1; index >= (int64_t)starting_swap_index; index--) {
							chars[index + swap_count] = chars[index];
						}
						chars[starting_swap_index] = '0';
						chars[starting_swap_index + 1] = '.';
						for (size_t index = starting_swap_index + 2; index <= swap_count - 1 + starting_swap_index; index++) {
							chars[index] = '0';
						}
						chars.size += swap_count;
					}
					else {
						if (integer == 0) {
							chars[chars.size] = '.';
							for (size_t index = chars.size + 1; index < chars.size + 1 + precision; index++) {
								chars[index] = '0';
							}
							chars.size += precision + 1;
						}
						else {
							for (size_t index = chars.size - 1; index >= chars.size - precision; index--) {
								chars[index + 1] = chars[index];
							}
							chars[chars.size - precision] = '.';
							chars.size += 1;
						}
					}
				}
				chars[chars.size] = '\0';

				return chars.size - initial_size;
			}
			// Wide implementation - convert to an ASCII stack buffer then commit to the wide stream
			else {
				CONVERT_TYPE_TO_CHARS_FROM_ASCII_TO_WIDE(ConvertFloatingPointIntegerToChars, chars, integer, precision);
			}
		}

		ECS_TEMPLATE_FUNCTION_4_BEFORE(size_t, ConvertFloatingPointIntegerToChars, Stream<char>&, CapacityStream<char>&, Stream<wchar_t>&, CapacityStream<wchar_t>&, int64_t, size_t);

		// ----------------------------------------------------------------------------------------------------------

		template<typename Stream>
		size_t ConvertFloatToChars(Stream& chars, float value, size_t precision) {
			ECS_ASSERT(precision < 16);
			float power_multiply = CalculateFloatPrecisionPower(precision + 1);
			int64_t rounded_int;

			float new_value = roundf(value * power_multiply);
			new_value *= 0.1f;
			rounded_int = static_cast<int64_t>(new_value);

			return ConvertFloatingPointIntegerToChars<Stream>(chars, rounded_int, precision);
		}

		ECS_TEMPLATE_FUNCTION_4_BEFORE(size_t, ConvertFloatToChars, Stream<char>&, CapacityStream<char>&, Stream<wchar_t>&, CapacityStream<wchar_t>&, float, size_t);

		// ----------------------------------------------------------------------------------------------------------

		template<typename Stream>
		size_t ConvertDoubleToChars(Stream& chars, double value, size_t precision) {
			ECS_ASSERT(precision < 16);
			double power_multiply = CalculateDoublePrecisionPower(precision + 1);
			int64_t rounded_int;

			double new_value = round(value * power_multiply);
			new_value *= 0.1;
			rounded_int = static_cast<int64_t>(new_value);

			return ConvertFloatingPointIntegerToChars<Stream>(chars, rounded_int, precision);
		}

		ECS_TEMPLATE_FUNCTION_4_BEFORE(size_t, ConvertDoubleToChars, Stream<char>&, CapacityStream<char>&, Stream<wchar_t>&, CapacityStream<wchar_t>&, double, size_t);

		// ----------------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		char ConvertCharactersToBoolImpl(Stream<CharacterType> characters)
		{
			if constexpr (std::is_same_v<char, CharacterType>) {
				if (memcmp(characters.buffer, "true", sizeof("true") - sizeof(char)) == 0) {
					return 1;
				}
				else if (memcmp(characters.buffer, "false", sizeof("false") - sizeof(char)) == 0) {
					return 0;
				}
				return -1;
			}
			else {
				if (memcmp(characters.buffer, L"true", sizeof(L"true") - sizeof(wchar_t)) == 0) {
					return 1;
				}
				else if (memcmp(characters.buffer, L"false", sizeof(L"false") - sizeof(wchar_t)) == 0) {
					return 0;
				}
				return -1;
			}
		}

		// ----------------------------------------------------------------------------------------------------------

		char ConvertCharactersToBool(Stream<char> characters) {
			return ConvertCharactersToBoolImpl(characters);
		}

		// ----------------------------------------------------------------------------------------------------------

		char ConvertCharactersToBool(Stream<wchar_t> characters) {
			return ConvertCharactersToBoolImpl(characters);
		}

		// ----------------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		float ParseFloat(Stream<CharacterType>* characters, CharacterType delimiter)
		{
			return ParseType<CharacterType, float>(characters, delimiter, [](Stream<CharacterType> stream_characters) {
				return ConvertCharactersToFloat(stream_characters);
			});
		}

		template ECSENGINE_API float ParseFloat(Stream<char>*, char);
		template ECSENGINE_API float ParseFloat(Stream<wchar_t>*, wchar_t);

		// ----------------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		double ParseDouble(Stream<CharacterType>* characters, CharacterType delimiter)
		{
			return ParseType<CharacterType, double>(characters, delimiter, [](Stream<CharacterType> stream_characters) {
				return ConvertCharactersToDouble(stream_characters);
			});
		}

		template ECSENGINE_API double ParseDouble(Stream<char>*, char);
		template ECSENGINE_API double ParseDouble(Stream<wchar_t>*, wchar_t);

		// ----------------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		bool ParseBool(Stream<CharacterType>* characters, CharacterType delimiter)
		{
			return ParseType<CharacterType, bool>(characters, delimiter, [](Stream<CharacterType> stream_characters) {
				return (bool)ConvertCharactersToBool(stream_characters);
			});
		}

		template ECSENGINE_API bool ParseBool(Stream<char>*, char);
		template ECSENGINE_API bool ParseBool(Stream<wchar_t>*, wchar_t);

		// ----------------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		double4 ParseDouble4(
			Stream<CharacterType>* characters, 
			CharacterType external_delimiter, 
			CharacterType internal_delimiter,
			Stream<CharacterType> ignore_tag
		)
		{
			double4 result = { 0.0, 0.0, 0.0, 0.0 };

			Stream<CharacterType> ext_delimiter = function::FindFirstCharacter(*characters, Character<CharacterType>('}'));
			if (ext_delimiter.size > 0) {
				Stream<CharacterType> parse_range = { characters->buffer, function::PointerDifference(ext_delimiter.buffer, characters->buffer) / sizeof(CharacterType) };
				// Multiple values
				Stream<CharacterType> delimiter = function::FindFirstCharacter(parse_range, internal_delimiter);

				double* double_array = (double*)&result;
				size_t double_array_index = 0;
				while (delimiter.size > 0 && double_array_index < 4) {
					Stream<CharacterType> current_range = { parse_range.buffer, function::PointerDifference(delimiter.buffer, parse_range.buffer) / sizeof(CharacterType) };
					double_array[double_array_index++] = function::ConvertCharactersToDouble(current_range);

					delimiter.buffer++;
					delimiter.size--;
					Stream<CharacterType> new_delimiter = function::FindFirstCharacter(delimiter, internal_delimiter);
					if (new_delimiter.size > 0) {
						parse_range = { delimiter.buffer, function::PointerDifference(new_delimiter.buffer, delimiter.buffer) / sizeof(CharacterType) };
						delimiter = new_delimiter;
					}
					else {
						parse_range = delimiter;
						delimiter.size = 0;
					}
				}

				if (double_array_index < 4) {
					// Try to parse a last value
					double_array[double_array_index] = function::ConvertCharactersToDouble(parse_range);
				}
				*characters = ext_delimiter;
				Stream<CharacterType> new_characters = function::FindFirstCharacter(*characters, external_delimiter);
				if (new_characters.size == 0) {
					characters->size = 0;
				}
				else {
					*characters = new_characters;
					characters->Advance();
				}
			}
			else {
				// Check the ignore tag
				if (ignore_tag.size > 0) {
					const CharacterType* starting_value = function::SkipWhitespace(characters->buffer);
					const CharacterType* delimiter = function::FindFirstCharacter(*characters, external_delimiter).buffer;
					if (delimiter == nullptr) {
						result.x = ConvertCharactersToDouble(*characters);
						characters->size = 0;
						return result;
					}
					else {
						// It has a delimiter
						const CharacterType* ending_value = function::SkipCodeIdentifier(delimiter, -1);
						Stream<CharacterType> compare_characters = { starting_value, function::PointerDifference(ending_value, starting_value) / sizeof(wchar_t) };
						if (function::CompareStrings(compare_characters, ignore_tag)) {
							*characters = delimiter;
							characters->Advance();
							return double4(DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX);
						}
					}
				}
				else {
					// Single value
					result.x = ParseDouble(characters, external_delimiter);
				}
			}
			

			return result;
		}

		template ECSENGINE_API double4 ParseDouble4(Stream<char>*, char, char, Stream<char>);
		template ECSENGINE_API double4 ParseDouble4(Stream<wchar_t>*, wchar_t, wchar_t, Stream<wchar_t>);

		// ----------------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		void ParseFloats(
			Stream<CharacterType> characters,
			CharacterType delimiter,
			CapacityStream<float>& values
		) {
			while (characters.size > 0) {
				values.AddSafe(ParseFloat(&characters, delimiter));
				if (characters.size > 0) {
					characters.Advance();
				}
			}
		}

		template ECSENGINE_API void ParseFloats(Stream<char>, char, CapacityStream<float>&);
		template ECSENGINE_API void ParseFloats(Stream<wchar_t>, wchar_t, CapacityStream<float>&);

		// ----------------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		void ParseDoubles(
			Stream<CharacterType> characters,
			CharacterType delimiter,
			CapacityStream<double>& values
		) {
			while (characters.size > 0) {
				values.AddSafe(ParseDouble(&characters, delimiter));
				if (characters.size > 0) {
					characters.Advance();
				}
			}
		}

		template ECSENGINE_API void ParseDoubles(Stream<char>, char, CapacityStream<double>&);
		template ECSENGINE_API void ParseDoubles(Stream<wchar_t>, wchar_t, CapacityStream<double>&);

		// ----------------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		void ParseBools(
			Stream<CharacterType> characters,
			CharacterType delimiter,
			CapacityStream<bool>& values
		) {
			if constexpr (std::is_same_v<CharacterType, char>) {
				characters = function::SkipWhitespaceEx(characters);
			}
			else {
				const CharacterType* skipped = function::SkipWhitespace(characters.buffer);
				if (skipped < characters.buffer + characters.size) {
					characters.size -= function::PointerDifference(skipped, characters.buffer) / sizeof(CharacterType);
					characters.buffer = (CharacterType*)skipped;
				}
			}

			while (characters.size > 0) {
				values.AddSafe(ParseBool(&characters, delimiter));
				if (characters.size > 0) {
					characters.Advance();
				}

				if constexpr (std::is_same_v<CharacterType, char>) {
					characters = function::SkipWhitespaceEx(characters);
				}
				else {
					const CharacterType* skipped = function::SkipWhitespace(characters.buffer);
					if (skipped < characters.buffer + characters.size) {
						characters.size -= function::PointerDifference(skipped, characters.buffer) / sizeof(CharacterType);
						characters.buffer = (CharacterType*)skipped;
					}
				}
			}
		}

		template ECSENGINE_API void ParseBools(Stream<char>, char, CapacityStream<bool>&);
		template ECSENGINE_API void ParseBools(Stream<wchar_t>, wchar_t, CapacityStream<bool>&);

		// ----------------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		void ParseDouble4s(
			Stream<CharacterType> characters, 
			CapacityStream<double4>& values, 
			CharacterType external_delimiter, 
			CharacterType internal_delimiter, 
			Stream<CharacterType> ignore_tag
		)
		{
			while (characters.size > 0) {
				values.AddSafe(ParseDouble4(&characters, external_delimiter, internal_delimiter, ignore_tag));
				if (characters.size > 0) {
					characters.Advance();
				}
			}
		}

		template ECSENGINE_API void ParseDouble4s(Stream<char>, CapacityStream<double4>&, char, char, Stream<char>);
		template ECSENGINE_API void ParseDouble4s(Stream<wchar_t>, CapacityStream<double4>&, wchar_t, wchar_t, Stream<wchar_t>);

		// ----------------------------------------------------------------------------------------------------------

		void* Copy(AllocatorPolymorphic allocator, const void* data, size_t data_size, size_t alignment)
		{
			return Copy(allocator, { data, data_size }, alignment).buffer;
		}

		Stream<void> Copy(AllocatorPolymorphic allocator, Stream<void> data, size_t alignment)
		{
			void* allocation = Allocate(allocator, data.size, alignment);
			memcpy(allocation, data.buffer, data.size);
			return { allocation, data.size };
		}

		// ----------------------------------------------------------------------------------------------------------

		void* CopyNonZero(AllocatorPolymorphic allocator, const void* data, size_t data_size, size_t alignment)
		{
			if (data_size > 0) {
				return Copy(allocator, data, data_size, alignment);
			}
			return (void*)data;
		}

		Stream<void> CopyNonZero(AllocatorPolymorphic allocator, Stream<void> data, size_t alignment)
		{
			if (data.size > 0) {
				return Copy(allocator, data, alignment);
			}
			return data;
		}

		// ----------------------------------------------------------------------------------------------------------

		template<typename Stream>
		void MakeSequence(Stream stream, size_t offset)
		{
			for (size_t index = 0; index < stream.size; index++) {
				stream[index] = index + offset;
			}
		}

		ECS_TEMPLATE_FUNCTION_4_BEFORE(void, MakeSequence, Stream<unsigned char>, Stream<unsigned short>, Stream<unsigned int>, Stream<size_t>, size_t);
		ECS_TEMPLATE_FUNCTION_4_BEFORE(void, MakeSequence, CapacityStream<unsigned char>, CapacityStream<unsigned short>, CapacityStream<unsigned int>, CapacityStream<size_t>, size_t);
		
		// ----------------------------------------------------------------------------------------------------------

		template<typename Stream>
		void MakeDescendingSequence(Stream stream, size_t offset) {
			offset += stream.size - 1;
			for (size_t index = 0; index < stream.size; index++) {
				stream[index] = offset - index;
			}
		}

		ECS_TEMPLATE_FUNCTION_4_BEFORE(void, MakeDescendingSequence, Stream<unsigned char>, Stream<unsigned short>, Stream<unsigned int>, Stream<size_t>, size_t);
		ECS_TEMPLATE_FUNCTION_4_BEFORE(void, MakeDescendingSequence, CapacityStream<unsigned char>, CapacityStream<unsigned short>, CapacityStream<unsigned int>, CapacityStream<size_t>, size_t);

		// ----------------------------------------------------------------------------------------------------------

		template<typename IndexStream>
		void CopyStreamWithMask(void* ECS_RESTRICT buffer, Stream<void> data, IndexStream indices)
		{
			for (size_t index = 0; index < indices.size; index++) {
				memcpy(buffer, (void*)((uintptr_t)data.buffer + data.size * indices[index]), data.size);
				uintptr_t ptr = (uintptr_t)buffer;
				ptr += data.size;
				buffer = (void*)ptr;
			}
		}

		ECS_TEMPLATE_FUNCTION_4_AFTER(void, CopyStreamWithMask, Stream<unsigned char>, Stream<unsigned short>, Stream<unsigned int>, Stream<size_t>, void* ECS_RESTRICT, Stream<void>);

		// ----------------------------------------------------------------------------------------------------------

		template<typename IndexStream>
		bool CopyStreamWithMask(ECS_FILE_HANDLE file, Stream<void> data, IndexStream indices)
		{
			bool success = true;
			for (size_t index = 0; index < indices.size && success; index++) {
				success &= WriteFile(file, { function::OffsetPointer(data.buffer, data.size * indices[index]), data.size });
			}
			return success;
		}

		ECS_TEMPLATE_FUNCTION_4_AFTER(bool, CopyStreamWithMask, Stream<unsigned char>, Stream<unsigned short>, Stream<unsigned int>, Stream<size_t>, ECS_FILE_HANDLE, Stream<void>);

		// ----------------------------------------------------------------------------------------------------------

		namespace Internal {

			template ECSENGINE_API ulong2 FormatStringInternal<const char*>(char*, const char*, const char*);
			template ECSENGINE_API ulong2 FormatStringInternal<const wchar_t*>(char*, const char*, const wchar_t*);
			template ECSENGINE_API ulong2 FormatStringInternal<Stream<char>>(char*, const char*, Stream<char>);
			template ECSENGINE_API ulong2 FormatStringInternal<Stream<wchar_t>>(char*, const char*, Stream<wchar_t>);
			template ECSENGINE_API ulong2 FormatStringInternal<CapacityStream<char>>(char*, const char*, CapacityStream<char>);
			template ECSENGINE_API ulong2 FormatStringInternal<CapacityStream<wchar_t>>(char*, const char*, CapacityStream<wchar_t>);
			template ECSENGINE_API ulong2 FormatStringInternal<unsigned int>(char*, const char*, unsigned int);
			template ECSENGINE_API ulong2 FormatStringInternal<void*>(char*, const char*, void*);
			template ECSENGINE_API ulong2 FormatStringInternal<float>(char*, const char*, float);
			template ECSENGINE_API ulong2 FormatStringInternal<double>(char*, const char*, double);

		}

		// ----------------------------------------------------------------------------------------------------------

		// ----------------------------------------------------------------------------------------------------------
	}

}