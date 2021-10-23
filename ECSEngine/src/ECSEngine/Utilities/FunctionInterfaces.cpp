#include "ecspch.h"
//#include "FunctionTemplates.h"
#include "FunctionInterfaces.h"
#include "../../Includes/ECSEngineAllocators.h"
#include "Function.h"
#include "Assert.h"

namespace ECSEngine {

	ECS_CONTAINERS;

	namespace function {

		template<typename Stream>
		float ConvertCharactersToFloat(Stream characters) {
			return ConvertCharactersToFloatingPoint<float>(characters);
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(float, ConvertCharactersToFloat, Stream<char>, CapacityStream<char>);

		template<typename Stream>
		double ConvertCharactersToDouble(Stream characters) {
			return ConvertCharactersToFloatingPoint<double>(characters);
		}

		ECS_TEMPLATE_FUNCTION_2_BEFORE(double, ConvertCharactersToDouble, Stream<char>, CapacityStream<char>);

#define CONVERT_TYPE_TO_CHARS_FROM_ASCII_TO_WIDE(convert_function, chars, ...) char characters[512]; \
		containers::Stream<char> ascii(characters, 0); \
\
		size_t character_count = function::convert_function(ascii, __VA_ARGS__); \
		function::ConvertASCIIToWide(chars.buffer + chars.size, ascii, character_count + 1); \
		chars.size += character_count; \
\
		return character_count;
		
		template<typename Stream>
		size_t ConvertIntToCharsFormatted(Stream& chars, int64_t value) {
			static_assert(std::is_same_v<Stream, containers::Stream<char>> || std::is_same_v<Stream, containers::CapacityStream<char>> ||
				std::is_same_v<Stream, containers::Stream<wchar_t>> || std::is_same_v<Stream, containers::CapacityStream<wchar_t>>);

			// ASCII implementation
			if constexpr (std::is_same_v<Stream, containers::Stream<char>> || std::is_same_v<Stream, containers::CapacityStream<char>>) {
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
						chars.Add('\'');
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
		
		template<typename Stream>
		size_t ConvertIntToChars(Stream& chars, int64_t value) {
			static_assert(std::is_same_v<Stream, containers::Stream<char>> || std::is_same_v<Stream, containers::CapacityStream<char>> ||
				std::is_same_v<Stream, containers::Stream<wchar_t>> || std::is_same_v<Stream, containers::CapacityStream<wchar_t>>);

			// ASCII implementation
			if constexpr (std::is_same_v<Stream, containers::Stream<char>> || std::is_same_v<Stream, containers::CapacityStream<char>>) {
				size_t initial_size = chars.size;

				if (value < 0) {
					chars[chars.size] = '-';
					chars.size++;
					value = -value;
				}

				size_t starting_swap_index = chars.size;

				if (value == 0) {
					chars[chars.size++] = '0';
				}
				else {
					while (value != 0) {
						chars[chars.size] = value % 10 + '0';
						chars.size++;
						value /= 10;
					}
				}
				for (size_t index = 0; index < (chars.size - starting_swap_index) >> 1; index++) {
					char temp = chars[index + starting_swap_index];
					chars[index + starting_swap_index] = chars[chars.size - 1 - index];
					chars[chars.size - 1 - index] = temp;
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

		size_t FindWhitespaceCharactersCount(const char* string, CapacityStream<unsigned int>* stack_buffer)
		{
			size_t size = strlen(string);
			const char* character = strpbrk(string, " \n");

			size_t count = 0;
			if (stack_buffer == nullptr) {
				while (character != nullptr) {
					size_t position = (uintptr_t)character - (uintptr_t)string;
					character = strpbrk(string + position + 1, " \n");
					count++;
				}
			}
			else {
				while (character != nullptr) {
					size_t position = (uintptr_t)character - (uintptr_t)string;
					if (stack_buffer->size < stack_buffer->capacity) {
						stack_buffer->Add(position);
					}
					character = strpbrk(string + position + 1, " \n");
					count++;
				}
			}
			return count;
		}

		// it searches for spaces and next line characters
		template<typename Stream>
		void FindWhitespaceCharacters(const char* string, Stream& spaces) {
			size_t length = strlen(string);
			size_t position = 0;

			const char* character = strpbrk(string, " \n");
			while (character != nullptr) {
				position = (uintptr_t)character - (uintptr_t)string;
				spaces.Add(position);
				character = strpbrk(string + position + 1, " \n");
			}
		}

		ECS_TEMPLATE_FUNCTION_2_AFTER(void, FindWhitespaceCharacters, Stream<unsigned int>&, CapacityStream<unsigned int>&, const char*);

		// finds the tokens that appear in the current string
		template<typename Stream>
		void FindToken(const char* string, char token, Stream& tokens) {
			size_t length = strlen(string);
			size_t position = 0;

			const char* character = strchr(string, token);
			while (character != nullptr) {
				position = (uintptr_t)character - (uintptr_t)string;
				tokens.Add(position);
				character = strchr(string + position + 1, token);
			}
		}

		ECS_TEMPLATE_FUNCTION_2_AFTER(void, FindToken, Stream<unsigned int>&, CapacityStream<unsigned int>&, const char*, char);

		// finds the tokens that appear in the current string
		template<typename Stream>
		void FindToken(const char* ECS_RESTRICT string, const char* ECS_RESTRICT tokens, Stream& positions) {
			size_t length = strlen(string);
			size_t token_size = strlen(tokens);
			size_t position = 0;

			const char* _token = strstr(string, tokens);
			while (_token != nullptr) {
				position = (uintptr_t)_token - (uintptr_t)string;
				positions.Add(position);
				_token = strstr(string + position + token_size, tokens);
			}
		}

		ECS_TEMPLATE_FUNCTION_2_AFTER(void, FindToken, Stream<unsigned int>&, CapacityStream<unsigned int>&, const char* ECS_RESTRICT, const char* ECS_RESTRICT);

		template<typename Stream>
		void ParseWordsFromSentence(const char* sentence, Stream& words) {
			ParseWordsFromSentence(sentence, " \n", words);
		}

		ECS_TEMPLATE_FUNCTION_2_AFTER(void, ParseWordsFromSentence, Stream<uint2>&, CapacityStream<uint2>&, const char*);

		template<typename Stream>
		void ParseWordsFromSentence(const char* ECS_RESTRICT sentence, const char* ECS_RESTRICT tokens, Stream& words) {
			size_t length = strlen(sentence);

			size_t starting_position = 0;
			size_t position = 0; 
			const char* character = strpbrk(sentence, tokens);
			while (character != nullptr) {
				position = (uintptr_t)character - (uintptr_t)sentence;
				if (position > starting_position + 1) {
					words.Add({ static_cast<unsigned int>(starting_position), static_cast<unsigned int>(position - starting_position - 1) });
					starting_position = position + 1;
				}
				character = strpbrk(sentence + position + 1, tokens);
			}
		}

		ECS_TEMPLATE_FUNCTION_2_AFTER(void, ParseWordsFromSentence, Stream<uint2>&, CapacityStream<uint2>&, const char* ECS_RESTRICT, const char* ECS_RESTRICT);

		template<typename Stream>
		void ParseWordsFromSentence(const char* sentence, char token, Stream& words) {
			size_t length = strlen(sentence);

			size_t starting_position = 0;
			size_t position = 0;
			const char* character = strchr(sentence, token);
			while (character != nullptr) {
				position = (uintptr_t)character - (uintptr_t)sentence;
				if (position > starting_position + 1) {
					words.Add({ static_cast<unsigned int>(starting_position), static_cast<unsigned int>(position - starting_position - 1) });
					starting_position = position + 1;
				}
				character = strchr(sentence + position + 1, token);
			}
		}

		ECS_TEMPLATE_FUNCTION_2_AFTER(void, ParseWordsFromSentence, Stream<uint2>&, CapacityStream<uint2>&, const char*, char);

		template<typename Type>
		Type PredicateValue(bool condition, Type first_value, Type second_value) {
			return first_value * condition + second_value * (1 - condition);
		}

		ECS_TEMPLATE_FUNCTION(float, PredicateValue, bool, float, float);
		ECS_TEMPLATE_FUNCTION(double, PredicateValue, bool, double, double);
		ECS_TEMPLATE_FUNCTION(unsigned int, PredicateValue, bool, unsigned int, unsigned int);
		ECS_TEMPLATE_FUNCTION(unsigned short, PredicateValue, bool, unsigned short, unsigned short);
		ECS_TEMPLATE_FUNCTION(int64_t, PredicateValue, bool, int64_t, int64_t);
		ECS_TEMPLATE_FUNCTION(size_t, PredicateValue, bool, size_t, size_t);

		template<typename Stream>
		size_t ConvertFloatingPointIntegerToChars(Stream& chars, int64_t integer, size_t precision) {
			static_assert(std::is_same_v<Stream, containers::Stream<char>> || std::is_same_v<Stream, containers::CapacityStream<char>> ||
				std::is_same_v<Stream, containers::Stream<wchar_t>> || std::is_same_v<Stream, containers::CapacityStream<wchar_t>>);

			// ASCII implementation
			if constexpr (std::is_same_v<Stream, containers::Stream<char>> || std::is_same_v<Stream, containers::CapacityStream<char>>) {
				size_t initial_size = chars.size;
				ConvertIntToChars(chars, integer);

				size_t starting_swap_index = function::PredicateValue(integer < 0, 1, 0) + initial_size;

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

		template<typename Stream>
		size_t ConvertFloatToChars(Stream& chars, float value, size_t precision) {
			ECS_ASSERT(precision < 16);
			float power_multiply = CalculateFloatPrecisionPower(precision + 1);
			int64_t rounded_int;

			float new_value = roundf(value * power_multiply);
			new_value *= 0.1f;
			rounded_int = static_cast<int64_t>(new_value);

			return ConvertFloatingPointIntegerToChars<Stream>(chars, rounded_int, precision);
			//chars.size = sprintf(chars.buffer, "%f", value);
		}

		ECS_TEMPLATE_FUNCTION_4_BEFORE(size_t, ConvertFloatToChars, Stream<char>&, CapacityStream<char>&, Stream<wchar_t>&, CapacityStream<wchar_t>&, float, size_t);

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

		// non digit characters are discarded
		template<typename Integer, typename Stream>
		Integer ConvertCharactersToInt(Stream stream) {
			Integer integer = Integer(0);
			size_t starting_index = PredicateValue(stream[0] == '-', 1, 0);

			for (size_t index = starting_index; index < stream.size; index++) {
				if (stream[index] >= '0' && stream[index] <= '9') {
					integer = integer * 10 + stream[index] - '0';
				}
			}
			integer = PredicateValue<Integer>(starting_index == 1, -integer, integer);

			return integer;
		}

		template ECSENGINE_API int64_t ConvertCharactersToInt<int64_t>(Stream<char>);
		template ECSENGINE_API int64_t ConvertCharactersToInt<int64_t>(CapacityStream<char>);
		template ECSENGINE_API size_t ConvertCharactersToInt<size_t>(Stream<char>);
		template ECSENGINE_API size_t ConvertCharactersToInt<size_t>(CapacityStream<char>);

		// non digit characters are discarded
		template<typename Integer, typename Stream>
		Integer ConvertCharactersToInt(Stream stream, size_t& digit_count) {
			Integer integer = Integer(0);
			size_t starting_index = PredicateValue(stream[0] == '-', 1, 0);
			digit_count = 0;

			for (size_t index = starting_index; index < stream.size; index++) {
				if (stream[index] >= '0' && stream[index] <= '9') {
					integer = integer * 10 + stream[index] - '0';
					digit_count++;
				}
			}
			integer = PredicateValue(starting_index == 1, -integer, integer);

			return integer;
		}

		template ECSENGINE_API int64_t ConvertCharactersToInt<int64_t>(Stream<char>, size_t&);
		template ECSENGINE_API int64_t ConvertCharactersToInt<int64_t>(CapacityStream<char>, size_t&);
		template ECSENGINE_API size_t ConvertCharactersToInt<size_t>(Stream<char>, size_t&);
		template ECSENGINE_API size_t ConvertCharactersToInt<size_t>(CapacityStream<char>, size_t&);

		template<typename FloatingPoint, typename Stream>
		FloatingPoint ConvertCharactersToFloatingPoint(Stream stream) {
			FloatingPoint value = 0;
			size_t starting_index = PredicateValue(stream[0] == '-' || stream[0] == '+', 1, 0);

			size_t dot_index = stream.size;
			for (size_t index = 0; index < stream.size; index++) {
				if (stream[index] == '.') {
					dot_index = index;
					break;
				}
			}
			if (dot_index < stream.size) {
				size_t integral_part = ConvertCharactersToInt<size_t>(Stream(stream.buffer + starting_index, dot_index - starting_index));

				size_t fractional_digit_count = 0;
				size_t fractional_part = ConvertCharactersToInt<size_t>(Stream(stream.buffer + dot_index + 1, stream.size - dot_index - 1), fractional_digit_count);

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
				size_t integer = ConvertCharactersToInt<size_t>(Stream(stream.buffer + starting_index, stream.size - starting_index));
				FloatingPoint value = static_cast<FloatingPoint>(integer);
				if (stream[0] == '-') {
					value = -value;
				}
				return value;
			}
		}

		template ECSENGINE_API float ConvertCharactersToFloatingPoint<float>(Stream<char>);
		template ECSENGINE_API float ConvertCharactersToFloatingPoint<float>(CapacityStream<char>);
		template ECSENGINE_API double ConvertCharactersToFloatingPoint<double>(Stream<char>);
		template ECSENGINE_API double ConvertCharactersToFloatingPoint<double>(CapacityStream<char>);

		template<typename Allocator>
		void GetRecursiveDirectories(Allocator* allocator, const wchar_t* root, CapacityStream<const wchar_t*>& directories_paths)
		{
			for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(root))) {
				if (entry.is_directory()) {
					const wchar_t* path = entry.path().c_str();
					size_t character_count = wcslen(path) + 1;
					wchar_t* allocation = (wchar_t*)allocator->Allocate(sizeof(wchar_t) * character_count, alignof(wchar_t));
					memcpy(allocation, path, sizeof(wchar_t) * character_count);
					directories_paths.Add(allocation);
				}
			}
		}

		ECS_TEMPLATE_FUNCTION_ALLOCATOR_API(void, GetRecursiveDirectories, const wchar_t*, CapacityStream<const wchar_t*>&);
		
		template<typename Allocator>
		void GetRecursiveDirectoriesBatchedAllocation(Allocator* allocator, const wchar_t* root, CapacityStream<const wchar_t*>& directories_paths)
		{
			size_t total_memory_count = 0;
			size_t path_sizes[4096];

			size_t index = 0;
			for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(root))) {
				if (entry.is_directory()) {
					const wchar_t* path = entry.path().c_str();
					path_sizes[index] = sizeof(wchar_t) * (wcslen(path) + 1);
					total_memory_count += path_sizes[index++];
				}
			}
			ECS_ASSERT(index < 4096);

			void* allocation = allocator->Allocate(total_memory_count, alignof(wchar_t));
			uintptr_t ptr = (uintptr_t)ptr;

			index = 0;
			for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(root))) {
				if (entry.is_directory()) {
					memcpy((wchar_t*)ptr, entry.path().c_str(), path_sizes[index]);
					directories_paths.Add((wchar_t*)ptr);
					ptr += path_sizes[index++];
				}
			}
		}

		ECS_TEMPLATE_FUNCTION_ALLOCATOR_API(void, GetRecursiveDirectoriesBatchedAllocation, const wchar_t*, CapacityStream<const wchar_t*>&);

		template<typename Allocator>
		void GetRecursiveDirectories(Allocator* allocator, const wchar_t* root, ResizableStream<const wchar_t*, Allocator>& directories_paths)
		{
			for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(root))) {
				if (entry.is_directory()) {
					const wchar_t* path = entry.path().c_str();
					size_t character_count = wcslen(path) + 1;
					wchar_t* allocation = (wchar_t*)allocator->Allocate(sizeof(wchar_t) * character_count, alignof(wchar_t*));
					memcpy(allocation, path, sizeof(wchar_t) * character_count);
					directories_paths.Add((wchar_t*)allocation);
				}
			}
		}

#define INSTANTIATE_RECURSIVE_DIRECTORY(allocator) template ECSENGINE_API void GetRecursiveDirectories<allocator>(allocator*, const wchar_t*, ResizableStream<const wchar_t*, allocator>&)
		
		INSTANTIATE_RECURSIVE_DIRECTORY(LinearAllocator);
		INSTANTIATE_RECURSIVE_DIRECTORY(StackAllocator);
		INSTANTIATE_RECURSIVE_DIRECTORY(PoolAllocator);
		INSTANTIATE_RECURSIVE_DIRECTORY(MultipoolAllocator);
		INSTANTIATE_RECURSIVE_DIRECTORY(MemoryManager);
		INSTANTIATE_RECURSIVE_DIRECTORY(GlobalMemoryManager);
		INSTANTIATE_RECURSIVE_DIRECTORY(MemoryArena);
		INSTANTIATE_RECURSIVE_DIRECTORY(ResizableMemoryArena);

#undef INSTANTIATE_RECUSIVE_DIRECTORY

		template<typename Allocator>
		void GetRecursiveDirectoriesBatchedAllocation(Allocator* allocator, const wchar_t* root, ResizableStream<const wchar_t*, Allocator>& directories_paths)
		{
			size_t total_memory_count = 0;
			size_t path_sizes[4096];

			size_t index = 0;
			for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(root))) {
				if (entry.is_directory()) {
					const wchar_t* path = entry.path().c_str();
					path_sizes[index] = sizeof(wchar_t) * (wcslen(path) + 1);
					total_memory_count += path_sizes[index++];
				}
			}
			ECS_ASSERT(index < 4096);

			void* allocation = allocator->Allocate(total_memory_count, alignof(wchar_t));
			uintptr_t ptr = (uintptr_t)ptr;

			index = 0;
			for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(root))) {
				if (entry.is_directory()) {
					memcpy((wchar_t*)ptr, entry.path().c_str(), path_sizes[index]);
					directories_paths.Add((wchar_t*)ptr);
					ptr += path_sizes[index++];
				}
			}
		}

#define INSTANTIATE_RECURSIVE_DIRECTORY(allocator) template ECSENGINE_API void GetRecursiveDirectoriesBatchedAllocation<allocator>(allocator*, const wchar_t*, ResizableStream<const wchar_t*, allocator>&)

		INSTANTIATE_RECURSIVE_DIRECTORY(LinearAllocator);
		INSTANTIATE_RECURSIVE_DIRECTORY(StackAllocator);
		INSTANTIATE_RECURSIVE_DIRECTORY(PoolAllocator);
		INSTANTIATE_RECURSIVE_DIRECTORY(MultipoolAllocator);
		INSTANTIATE_RECURSIVE_DIRECTORY(MemoryManager);
		INSTANTIATE_RECURSIVE_DIRECTORY(GlobalMemoryManager);
		INSTANTIATE_RECURSIVE_DIRECTORY(MemoryArena);
		INSTANTIATE_RECURSIVE_DIRECTORY(ResizableMemoryArena);

#undef INSTANTIATE_RECUSIVE_DIRECTORY

		template<typename Allocator>
		void GetDirectoryFiles(Allocator* allocator, const wchar_t* directory, CapacityStream<const wchar_t*>& files) {
			for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(directory))) {
				const wchar_t* path = entry.path().c_str();
				size_t character_count = wcslen(path) + 1;
				wchar_t* allocation = (wchar_t*)allocator->Allocate(sizeof(wchar_t) * character_count, alignof(wchar_t));
				memcpy(allocation, path, sizeof(wchar_t) * character_count);
				files.Add(allocation);
			}
		}

		ECS_TEMPLATE_FUNCTION_ALLOCATOR_API(void, GetDirectoryFiles, const wchar_t*, CapacityStream<const wchar_t*>&);

		template<typename Allocator>
		void GetDirectoryFilesBatchedAllocation(Allocator* allocator, const wchar_t* directory, CapacityStream<const wchar_t*>& files) {
			size_t total_memory = 0;
			size_t path_sizes[4096];
			size_t index = 0;

			for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(directory))) {
				const wchar_t* path = entry.path().c_str();
				path_sizes[index++] = sizeof(wchar_t) * (wcslen(path) + 1);
			}
			ECS_ASSERT(index < 4096);

			void* allocation = allocator->Allocate(total_memory, alignof(wchar_t));
			uintptr_t ptr = (uintptr_t)allocation;

			index = 0;
			for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(directory))) {
				memcpy((wchar_t*)ptr, entry.path().c_str(), path_sizes[index]);
				files.Add((wchar_t*)ptr);
				ptr += path_sizes[index++];
			}
		}

		ECS_TEMPLATE_FUNCTION_ALLOCATOR_API(void, GetDirectoryFilesBatchedAllocation, const wchar_t*, CapacityStream<const wchar_t*>&);

		template<typename Allocator>
		void GetDirectoryFiles(Allocator* allocator, const wchar_t* directory, ResizableStream<const wchar_t*, Allocator>& files) {
			for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(directory))) {
				const wchar_t* path = entry.path().c_str();
				size_t character_count = wcslen(path) + 1;
				wchar_t* allocation = (wchar_t*)allocator->Allocate(sizeof(wchar_t) * character_count, alignof(wchar_t));
				memcpy(allocation, path, sizeof(wchar_t) * character_count);
				files.Add(allocation);
			}
		}

#define INSTANTIATE_RECURSIVE_DIRECTORY(allocator) template ECSENGINE_API void GetDirectoryFiles<allocator>(allocator*, const wchar_t*, ResizableStream<const wchar_t*, allocator>&)

		INSTANTIATE_RECURSIVE_DIRECTORY(LinearAllocator);
		INSTANTIATE_RECURSIVE_DIRECTORY(StackAllocator);
		INSTANTIATE_RECURSIVE_DIRECTORY(PoolAllocator);
		INSTANTIATE_RECURSIVE_DIRECTORY(MultipoolAllocator);
		INSTANTIATE_RECURSIVE_DIRECTORY(MemoryManager);
		INSTANTIATE_RECURSIVE_DIRECTORY(GlobalMemoryManager);
		INSTANTIATE_RECURSIVE_DIRECTORY(MemoryArena);
		INSTANTIATE_RECURSIVE_DIRECTORY(ResizableMemoryArena);

#undef INSTANTIATE_RECUSIVE_DIRECTORY

		template<typename Allocator>
		void GetDirectoryFilesBatchedAllocation(Allocator* allocator, const wchar_t* directory, ResizableStream<const wchar_t*, Allocator>& files) {
			size_t total_memory = 0;
			size_t path_sizes[4096];
			size_t index = 0;

			for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(directory))) {
				const wchar_t* path = entry.path().c_str();
				path_sizes[index++] = sizeof(wchar_t) * (wcslen(path) + 1);
			}
			ECS_ASSERT(index < 4096);

			void* allocation = allocator->Allocate(total_memory, alignof(wchar_t));
			uintptr_t ptr = (uintptr_t)allocation;

			index = 0;
			for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(directory))) {
				memcpy((wchar_t*)ptr, entry.path().c_str(), path_sizes[index]);
				files.Add((wchar_t*)ptr);
				ptr += path_sizes[index++];
			}
		}

#define INSTANTIATE_RECURSIVE_DIRECTORY(allocator) template ECSENGINE_API void GetDirectoryFilesBatchedAllocation<allocator>(allocator*, const wchar_t*, ResizableStream<const wchar_t*, allocator>&)

		INSTANTIATE_RECURSIVE_DIRECTORY(LinearAllocator);
		INSTANTIATE_RECURSIVE_DIRECTORY(StackAllocator);
		INSTANTIATE_RECURSIVE_DIRECTORY(PoolAllocator);
		INSTANTIATE_RECURSIVE_DIRECTORY(MultipoolAllocator);
		INSTANTIATE_RECURSIVE_DIRECTORY(MemoryManager);
		INSTANTIATE_RECURSIVE_DIRECTORY(GlobalMemoryManager);
		INSTANTIATE_RECURSIVE_DIRECTORY(MemoryArena);
		INSTANTIATE_RECURSIVE_DIRECTORY(ResizableMemoryArena);

#undef INSTANTIATE_RECUSIVE_DIRECTORY

		template<typename Allocator>
		void* Copy(Allocator* allocator, const void* data, size_t data_size, size_t alignment) {
			void* allocation = allocator->Allocate(data_size, alignment);
			memcpy(allocation, data, data_size);
			return allocation;
		}

		ECS_TEMPLATE_FUNCTION_ALLOCATOR_API(void*, Copy, const void*, size_t, size_t);

		template<typename Allocator>
		void* CopyTs(Allocator* allocator, const void* data, size_t data_size, size_t alignment) {
			void* allocation = allocator->Allocate_ts(data_size, alignment);
			memcpy(allocation, data, data_size);
			return allocation;
		}

		ECS_TEMPLATE_FUNCTION_ALLOCATOR_API(void*, CopyTs, const void*, size_t, size_t);

		template<typename Allocator>
		Stream<char> StringCopy(Allocator* allocator, const char* string) {
			size_t string_size = strlen(string);
			return Stream<char>(Copy(allocator, string, (string_size + 1) * sizeof(char), alignof(char)), string_size);
		}

		ECS_TEMPLATE_FUNCTION_ALLOCATOR_API(Stream<char>, StringCopy, const char*);

		template<typename Allocator>
		Stream<char> StringCopyTs(Allocator* allocator, const char* string) {
			size_t string_size = strlen(string);
			return Stream<char>(CopyTs(allocator, string, (string_size + 1) * sizeof(char), alignof(char)), string_size);
		}

		ECS_TEMPLATE_FUNCTION_ALLOCATOR_API(Stream<char>, StringCopyTs, const char*);

		template<typename Allocator>
		Stream<wchar_t> StringCopy(Allocator* allocator, const wchar_t* string) {
			size_t string_size = wcslen(string);
			return Stream<wchar_t>(Copy(allocator, string, (string_size + 1) * sizeof(wchar_t), alignof(wchar_t)), string_size);
		}

		ECS_TEMPLATE_FUNCTION_ALLOCATOR_API(Stream<wchar_t>, StringCopy, const wchar_t*);

		template<typename Allocator>
		Stream<wchar_t> StringCopyTs(Allocator* allocator, const wchar_t* string) {
			size_t string_size = wcslen(string);
			return Stream<wchar_t>(CopyTs(allocator, string, (string_size + 1) * sizeof(wchar_t), alignof(wchar_t)), string_size);
		}

		ECS_TEMPLATE_FUNCTION_ALLOCATOR_API(Stream<wchar_t>, StringCopyTs, const wchar_t*);

		template<typename Stream>
		unsigned int IsStringInStream(const char* string, Stream stream)
		{
			return IsStringInStream(ToStream(string), stream);
		}

		ECS_TEMPLATE_FUNCTION_4_AFTER(unsigned int, IsStringInStream, Stream<Stream<char>>, CapacityStream<Stream<char>>, Stream<CapacityStream<char>>, CapacityStream<CapacityStream<char>>, const char*);

		template<typename StreamType>
		unsigned int IsStringInStream(Stream<char> string, StreamType stream) {
			for (size_t index = 0; index < stream.size; index++) {
				if (string.size == stream[index].size) {
					if (memcmp(string.buffer, stream[index].buffer, string.size) == 0) {
						return index;
					}
				}
			}
			return -1;
		}

		ECS_TEMPLATE_FUNCTION_4_AFTER(unsigned int, IsStringInStream, Stream<Stream<char>>, CapacityStream<Stream<char>>, Stream<CapacityStream<char>>, CapacityStream<CapacityStream<char>>, Stream<char>);

		template<typename Stream>
		unsigned int IsStringInStream(const wchar_t* string, Stream stream) {
			return IsStringInStream(ToStream(string), stream);
		}

		ECS_TEMPLATE_FUNCTION_4_AFTER(unsigned int, IsStringInStream, Stream<Stream<wchar_t>>, CapacityStream<Stream<wchar_t>>, Stream<CapacityStream<wchar_t>>, CapacityStream<CapacityStream<wchar_t>>, const wchar_t*);

		template<typename StreamType>
		unsigned int IsStringInStream(Stream<wchar_t> string, StreamType stream) {
			size_t byte_size = string.size * sizeof(wchar_t);
			for (size_t index = 0; index < stream.size; index++) {
				if (string.size == stream[index].size) {
					if (memcmp(string.buffer, stream[index].buffer, byte_size) == 0) {
						return index;
					}
				}
			}
			return -1;
		}

		ECS_TEMPLATE_FUNCTION_4_AFTER(unsigned int, IsStringInStream, Stream<Stream<wchar_t>>, CapacityStream<Stream<wchar_t>>, Stream<CapacityStream<wchar_t>>, CapacityStream<CapacityStream<wchar_t>>, Stream<wchar_t>);

		template<typename Stream>
		void MakeSequence(Stream stream)
		{
			for (size_t index = 0; index < stream.size; index++) {
				stream[index] = index;
			}
		}

		ECS_TEMPLATE_FUNCTION_4_BEFORE(void, MakeSequence, Stream<unsigned char>, Stream<unsigned short>, Stream<unsigned int>, Stream<size_t>);
		
		template<typename Stream>
		void MakeDescendingSequence(Stream stream) {
			for (size_t index = 0; index < stream.size; index++) {
				stream[index] = stream.size - 1 - index;
			}
		}

		ECS_TEMPLATE_FUNCTION_4_BEFORE(void, MakeDescendingSequence, Stream<unsigned char>, Stream<unsigned short>, Stream<unsigned int>, Stream<size_t>);


		template<typename IndexStream>
		void CopyStreamWithMask(void* ECS_RESTRICT buffer, const void* ECS_RESTRICT data, size_t data_element_size, IndexStream indices)
		{
			for (size_t index = 0; index < indices.size; index++) {
				memcpy(buffer, (void*)((uintptr_t)data + data_element_size * indices[index]), data_element_size);
				uintptr_t ptr = (uintptr_t)buffer;
				ptr += data_element_size;
				buffer = (void*)ptr;
			}
		}

		ECS_TEMPLATE_FUNCTION_4_AFTER(void, CopyStreamWithMask, Stream<unsigned char>, Stream<unsigned short>, Stream<unsigned int>, Stream<size_t>, void* ECS_RESTRICT, const void* ECS_RESTRICT, size_t);

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

		template<typename IndexStream>
		void CopyStreamWithMask(std::ofstream& stream, const void* ECS_RESTRICT data, size_t data_element_size, IndexStream indices)
		{
			for (size_t index = 0; index < indices.size; index++) {
				stream.write((const char*)((uintptr_t)data + data_element_size * indices[index]), data_element_size);
			}
		}

		ECS_TEMPLATE_FUNCTION_4_AFTER(void, CopyStreamWithMask, Stream<unsigned char>, Stream<unsigned short>, Stream<unsigned int>, Stream<size_t>, std::ofstream&, const void* ECS_RESTRICT, size_t);

		template<typename IndexStream>
		void CopyStreamWithMask(std::ofstream& stream, Stream<void> data, IndexStream indices)
		{
			for (size_t index = 0; index < indices.size; index++) {
				stream.write((const char*)((uintptr_t)data.buffer + data.size * indices[index]), data.size);
			}
		}

		ECS_TEMPLATE_FUNCTION_4_AFTER(void, CopyStreamWithMask, Stream<unsigned char>, Stream<unsigned short>, Stream<unsigned int>, Stream<size_t>, std::ofstream&, Stream<void>);

	}

}