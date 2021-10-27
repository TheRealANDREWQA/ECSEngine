#pragma once
#include "../Core.h"
#include "ecspch.h"
#include "BasicTypes.h"
#include "Function.h"

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

namespace ECSEngine {

	namespace function {

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

		template<bool exact_length = false, typename Allocator>
		wchar_t* ConvertASCIIToWide(const char* pointer, Allocator* allocator) {
			if constexpr (!exact_length) {
				wchar_t* w_string = (wchar_t*)allocator->Allocate(1024 * sizeof(wchar_t), alignof(wchar_t));
				MultiByteToWideChar(CP_ACP, 0, pointer, -1, w_string, 1024);
				return w_string;
			}
			else {
				size_t char_length = strlen(pointer);
				wchar_t* w_string = (wchar_t*)allocator->Allocate((char_length + 1) * sizeof(wchar_t), alignof(wchar_t));
				MultiByteToWideChar(CP_ACP, 0, pointer, char_length + 1, w_string, char_length + 1);
				return w_string;
			}
		}

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

		template<typename FloatingPoint, typename Stream>
		FloatingPoint ConvertCharactersToFloatingPoint(Stream stream) {
			FloatingPoint value = 0;
			size_t starting_index = Select(stream[0] == '-' || stream[0] == '+', 1, 0);

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
			return Select(value < min, min, value);
		}

		template<typename Value>
		Value ClampMax(Value value, Value max) {
			return Select(value > max, max, value);
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

		template<typename Type>
		Type Select(bool condition, Type first_value, Type second_value) {
			return condition ? first_value : second_value;
		}

		// the functions provided must take as parameter the pointer to the buffer element to act on
		template<typename Function1, typename Function2, typename Buffer>
		void SimdForLoop(Function1&& simd_function, Function2&& scalar_function, Buffer* buffer, size_t count, size_t vector_size) {
			size_t regular_size = count & (-vector_size);
			size_t index = 0;
			for (; index < regular_size; index += vector_size) {
				simd_function(buffer + regular_size);
			}
			scalar_function(buffer + index);
		}

		constexpr size_t convert_int_to_hex_do_not_write_0x = 1 << 0;
		constexpr size_t convert_int_to_hex_add_normal_value_after = 1 << 1;

		template<size_t flags = 0, typename Integer>
		void ConvertIntToHex(Stream<char>& characters, Integer integer) {
			constexpr char hex_characters[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
			
			if constexpr ((flags & convert_int_to_hex_do_not_write_0x) == 0) {
				characters.Add('0');
				characters.Add('x');
			}
			if constexpr (std::is_same_v<Integer, int8_t> || std::is_same_v<Integer, uint8_t>) {
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
				ConvertIntToHex<convert_int_to_hex_do_not_write_0x>(characters, last_two_bytes);
				ConvertIntToHex<convert_int_to_hex_do_not_write_0x>(characters, first_two_bytes);
			}
			else if constexpr (std::is_same_v<Integer, int64_t> || std::is_same_v<Integer, uint64_t>) {
				uint32_t first_integer = integer & 0x00000000FFFFFFFF;
				uint32_t second_integer = (integer & 0xFFFFFFFF00000000) >> 32;
				ConvertIntToHex<convert_int_to_hex_do_not_write_0x>(characters, second_integer);
				ConvertIntToHex<convert_int_to_hex_do_not_write_0x>(characters, first_integer);
			}

			if constexpr ((flags & convert_int_to_hex_add_normal_value_after) != 0) {
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
					function::ConvertIntToCharsFormatted(temp_stream, static_cast<int64_t>(parameter));
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
					function::ConvertWideCharsToASCII(parameter, CapacityStream<char>(temp_stream.buffer, temp_stream.size, temp_stream.size + parameter.size + 1));
					temp_stream.size += parameter.size;
					temp_stream[temp_stream.size] = '\0';
				}
				else if constexpr (std::is_same_v<Parameter, CapacityStream<char>> || std::is_same_v<Parameter, Stream<char>>) {
					temp_stream.AddStream(parameter);
				}
				return { temp_stream.size, copy_count };
			}

			return {0, 0};
		}

		// returns the count of the characters written;
		template<typename Parameter>
		size_t FormatString(char* destination, const char* base_characters, Parameter parameter) {
			size_t base_character_count = strlen(base_characters);
			
			ulong2 characters_written = FormatStringInternal(destination, base_characters, parameter, "{0}");
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
			
			ulong2 characters_written = FormatStringInternal(destination, base_characters, parameter1, "{0}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter2, "{1}");
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
			
			ulong2 characters_written = FormatStringInternal(destination, base_characters, parameter1, "{0}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter2, "{1}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter3, "{2}");
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

			ulong2 characters_written = FormatStringInternal(destination, base_characters, parameter1, "{0}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter2, "{1}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter3, "{2}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter4, "{3}");
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

			ulong2 characters_written = FormatStringInternal(destination, base_characters, parameter1, "{0}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter2, "{1}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter3, "{2}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter4, "{3}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter5, "{4}");
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

			ulong2 characters_written = FormatStringInternal(destination, base_characters, parameter1, "{0}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter2, "{1}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter3, "{2}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter4, "{3}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter5, "{4}");
			characters_written.y += 3;
			characters_written += FormatStringInternal(destination + characters_written.x, base_characters + characters_written.y, parameter6, "{5}");
			characters_written.y += 3;

			memcpy(destination + characters_written.x, base_characters + characters_written.y, base_character_count - characters_written.y);
			characters_written.x += base_character_count - characters_written.y;
			destination[characters_written.x] = '\0';
			return characters_written.x;
		}

		template<typename Integer>
		void IntegerRange(Integer& min, Integer& max) {
			if constexpr (std::is_same_v<Integer, int8_t>) {
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

		template<typename Stream, typename Handler>
		size_t Find(Stream stream, Handler&& handler) {
			for (size_t index = 0; index < stream.size; index++) {
				if (handler[stream[index]]) {
					return index;
				}
			}
			return -1;
		}

	}

}