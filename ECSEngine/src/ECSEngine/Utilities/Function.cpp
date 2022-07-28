#include "ecspch.h"
#include "../Core.h"
#include "Function.h"
#include "FunctionInterfaces.h"
#include "../Allocators/AllocatorPolymorphic.h"
#include "../Math/VCLExtensions.h"

namespace ECSEngine {

	namespace function {

		// --------------------------------------------------------------------------------------------------

		// pointers should be aligned preferably to 128 bytes
		void avx2_copy(void* destination, const void* source, size_t bytes) {
			// 64 B
			if (bytes < 64) {
				if (((uintptr_t)destination & 32) == 0 && ((uintptr_t)source & 32) == 0) {
					// if multiple of 32
					size_t rounded_bytes_32 = bytes & (-32);
					if (rounded_bytes_32 == bytes)
						avx2_copy_32multiple(destination, source, bytes);
					else {
						memcpy(destination, source, bytes - rounded_bytes_32);
						avx2_copy_32multiple<true>(destination, source, rounded_bytes_32);
					}
				}
				else {
					// if multiple of 32
					size_t rounded_bytes_32 = bytes & (-32);
					if (rounded_bytes_32 == bytes)
						avx2_copy_32multiple<true>(destination, source, bytes);
					else {
						memcpy(destination, source, bytes - rounded_bytes_32);
						avx2_copy_32multiple<true>(destination, source, rounded_bytes_32);
					}
				}
			}
			// 1 MB
			else if (bytes < 1'048'576) {
				if (((uintptr_t)destination & 128) == 0 && ((uintptr_t)source & 128) == 0) {
					// if multiple of 128
					size_t rounded_bytes_128 = bytes & (-128);
					if (rounded_bytes_128 == bytes)
						avx2_copy_128multiple(destination, source, bytes);
					else {
						memcpy(destination, source, bytes - rounded_bytes_128);
						avx2_copy_128multiple<true>(destination, source, bytes);
					}
				}
				else {
					// if multiple of 128
					size_t rounded_bytes_128 = bytes & (-128);
					if (rounded_bytes_128 == bytes)
						avx2_copy_128multiple<true>(destination, source, bytes);
					else {
						memcpy(destination, source, bytes - rounded_bytes_128);
						avx2_copy_128multiple<true>(destination, source, bytes);
					}
				}
			}
			// over 1 MB
			else {
				memcpy(destination, source, bytes);
			}
		}

		// --------------------------------------------------------------------------------------------------

		void CheckWindowsFunctionErrorCode(HRESULT hr, LPCWSTR box_name, const char* filename, unsigned int line, bool do_exit) {
			if (FAILED(hr)) {
				_com_error error(hr);
				ECS_TEMP_ASCII_STRING(temp_string, 512);
				temp_string.Copy(ToStream("[File] "));
				temp_string.AddStream(ToStream(filename));
				temp_string.AddStreamSafe(ToStream("\n[Line] "));
				function::ConvertIntToChars(temp_string, line);
				temp_string.Add('\n');
				temp_string.AddSafe('\0');
				wchar_t* converted_filename = function::ConvertASCIIToWide(temp_string.buffer);
#ifdef ECSENGINE_DEBUG
				__debugbreak();
#endif
				if (do_exit) {
					MessageBox(nullptr, (std::wstring(converted_filename) + error.ErrorMessage()).c_str(), box_name, MB_OK | MB_ICONERROR);
					exit(0);
				}
				else {
					MessageBox(nullptr, (std::wstring(converted_filename) + error.ErrorMessage()).c_str(), box_name, MB_OK | MB_ICONWARNING);
					delete[] converted_filename;
				}
			}
		}

		// --------------------------------------------------------------------------------------------------

		// returns the count of decoded numbers
		size_t ParseNumbersFromCharString(Stream<char> characters, unsigned int* number_buffer) {
			size_t count = 0, index = 0;
			while (index < characters.size) {
				size_t number = 0;
				// reading characters until the first digit
				while ((characters[index] < '0' || characters[index] > '9') && index < characters.size) {
					index++;
				}

				// reading the digits
				while (characters[index] >= '0' && characters[index] <= '9' && index < characters.size) {
					number = number * 10 + (characters[index] - '0');
					index++;
				}
				number_buffer[count++] = number;
			}
			return count;
		}

		// --------------------------------------------------------------------------------------------------

		size_t ParseNumbersFromCharString(Stream<char> characters, int* number_buffer) {
			size_t count = 0, index = 0;
			while (index < characters.size) {
				size_t number = 0;

				// reading characters until the first digit
				while (characters[index] < '0' || characters[index] > '9' && index < characters.size) {
					index++;
				}

				// reading the digits; if the character before the first digit is a minus, record that
				bool is_negative = characters[index - 1] == '-';
				while (characters[index] >= '0' && characters[index] <= '9' && index < characters.size) {
					number = number * 10 + characters[index] - '0';
					index++;
				}
				number_buffer[count++] = number - ((number << 1) & is_negative);
			}
			return count;
		}

		// --------------------------------------------------------------------------------------------------

		size_t ParseWordsFromSentence(const char* sentence, char separator_token)
		{
			size_t length = strlen(sentence);

			size_t count = 0;
			for (size_t index = 0; index < length; index++) {
				count += (sentence[index] == separator_token || sentence[index] == '\n');
			}
			return count;
		}

		// --------------------------------------------------------------------------------------------------

		void ObliqueRectangle(float2* positions, float2 a, float2 b, float thickness)
		{
			// calculating the slope of the perpendicular to AB segment
			// ab_slope = (b.y - a.y) / (b.x - a.x) => p_slope = -1 / ((b.y - a.y) / (b.x - a.x))
			float perpendicular_slope = (a.x - b.x) / (b.y - a.y);

			// expressing the distance from B to the lowest point
			// d is thickness
			// (d/2) ^ 2 = (x_point - b.x) ^ 2 + (y_point - b.y) ^ 2;
			// using the perpendicular slope deduce the y difference
			// y_point - b.y = perpedicular_slope * (x_point - b.x)
			// replace the expression
			// (d/2) ^ 2 = (x_point - b.x) ^ 2 * (perpendicular_slope * 2 + 1);
			// x_point - b.x = +- (d/2) / sqrt(perpendicular_slope * 2 + 1);
			// x_point = +- (d/2) / sqrt(perpendicular_slope * 2 + 1) + b.x;
			// if the ab_slope is positive, than x_point - b.x is positive
			// else negative
			// for the correlated point, the sign is simply reversed
			// the termen is constant for all 4 points, so it is the first calculated
			// from the ecuation of the perpendicular segment: y_point - b.y = perpendicular_slope * (x_point - b.x);
			// can be deduce the y difference;
			float x_difference = thickness / (2.0f * sqrt(perpendicular_slope * perpendicular_slope + 1.0f));
			float y_difference = x_difference * perpendicular_slope;

			// a points; first point is 0, second one is 2 since the right corner is deduced from b
			// if the ab_slope is positive, than the first one is to the left so it is smaller so it must be substracted
			// equivalent perpendicular slope is to be negative
			bool is_negative_slope = perpendicular_slope < 0.0f;
			float2 difference = { function::Select(is_negative_slope, x_difference, -x_difference), function::Select(is_negative_slope, y_difference, -y_difference) };
			positions[0] = { a.x - difference.x, a.y - difference.y };
			// for the correlated point, swap the signs
			positions[2] = { a.x + difference.x, a.y + difference.y };

			// b points
			// if the ab_slope is positive, than the first one is to the left so it is smaller so it must be substracted
			// equivalent perpendicular slope is to be negative
			positions[1] = { b.x - difference.x, b.y - difference.y };
			positions[3] = { b.x + difference.x, b.y + difference.y };
		}

		// --------------------------------------------------------------------------------------------------

		void ConvertDurationToChars(size_t duration, char* characters)
		{
			auto append_lambda = [characters](size_t integer) {
				size_t character_count = strlen(characters);
				Stream<char> stream = Stream<char>(characters, character_count);
				function::ConvertIntToCharsFormatted(stream, (int64_t)integer);
			};

			// less than 10 seconds
			if (duration < 10000) {
				strcat(characters, "Just now");
			}
			// less than 60 seconds - 1 minute
			else if (duration < 60'000) {
				strcat(characters, "Less than a minute ago");
			}
			// less than 60 minutes - 1 hour
			else if (duration < 3'600'000) {
				size_t minutes = duration / 60'000;
				if (minutes > 1) {
					append_lambda(minutes);
					strcat(characters, " minutes ago");
				}
				else {
					strcat(characters, "A minute ago");
				}
			}
			// less than a 24 hours - 1 day
			else if (duration < 24 * 3'600'000) {
				size_t hours = duration / 3'600'000;
				if (hours > 1) {
					append_lambda(hours);
					strcat(characters, " hours ago");
				}
				else {
					strcat(characters, "An hour ago");
				}
			}
			// less than a 30 days - 1 months
			else if (duration < (size_t)30 * 24 * 3'600'000) {
				size_t days = duration / (24 * 3'600'000);
				if (days > 1) {
					append_lambda(days);
					strcat(characters, " days ago");
				}
				else {
					strcat(characters, "A day ago");
				}
			}
			// less than 12 months - 1 year
			else if (duration < (size_t)12 * 30 * 24 * 3'600'000) {
				size_t months = duration / ((size_t)30 * (size_t)24 * (size_t)3'600'000);
				if (months > 1) {
					append_lambda(months);
					strcat(characters, " months ago");
				}
				else {
					strcat(characters, "A month ago");
				}
			}
			// years
			else {
				size_t years = duration / ((size_t)12 * (size_t)30 * (size_t)24 * (size_t)3'600'000);
				if (years > 1) {
					append_lambda(years);
					strcat(characters, " years ago");
				}
				else {
					strcat(characters, "A year ago");
				}
			}
		}

		// --------------------------------------------------------------------------------------------------

		Stream<char> FindFirstToken(Stream<char> characters, const char* token)
		{
			size_t token_size = strlen(token);

			// If the token size is greater than 256 (i.e. 8 32 byte element SIMD registers)
			// The check should be done manually with memcmp

			Vec32c simd_token[8];
			Vec32c first_char(token[0]);

			if (characters.size < token_size) {
				return { nullptr, 0 };
			}

			size_t simd_token_count = 0;
			size_t iteration_count = characters.size - token_size + 1;
			size_t simd_count = GetSimdCount(iteration_count, first_char.size());
			size_t simd_remainder = 0;
			if (token_size <= 256) {
				while (token_size > first_char.size()) {
					size_t offset = simd_token_count * first_char.size();
					simd_token[simd_token_count++].load(token + offset);
					token_size -= first_char.size();
				}
				if (token_size > 0) {
					size_t offset = simd_token_count * first_char.size();
					simd_token[simd_token_count].load_partial(token_size, token + offset);
					simd_remainder = token_size;
				}

				for (size_t index = 0; index < simd_count; index += first_char.size()) {
					Vec32c current_chars = Vec32c().load(characters.buffer + index);
					auto match = current_chars == first_char;

					unsigned int found_match = -1;
					ForEachBit(match, [&](unsigned int bit_index) {
						size_t subindex = 0;
						const char* current_characters = characters.buffer + index + bit_index;
						Vec32c simd_current_characters;

						for (; subindex < simd_token_count; subindex++) {
							simd_current_characters.load(current_characters + subindex * simd_current_characters.size());
							if (horizontal_and(simd_token[subindex] == simd_current_characters)) {
								// They are different quit
								break;
							}
						}

						// If they subindex is simd_token_count, they might be equal
						if (subindex == simd_token_count) {
							if (simd_remainder > 0) {
								simd_current_characters.load_partial(simd_remainder, current_characters + simd_token_count * simd_current_characters.size());
								if (horizontal_and(simd_token[simd_token_count] == simd_current_characters)) {
									// Found a match
									found_match = bit_index;
									return true;
								}
							}
							else {
								// Found a match
								found_match = bit_index;
								return true;
							}
						}
						
						return false;
					});

					if (found_match != -1) {
						const char* string = characters.buffer + index + found_match;
						return { string, PointerDifference(characters.buffer + characters.size, string) };
					}
				}
			}
			else {
				for (size_t index = 0; index < simd_count; index += first_char.size()) {
					Vec32c current_chars = Vec32c().load(characters.buffer + index);
					auto match = current_chars == first_char;

					unsigned int found_match = -1;
					ForEachBit(match, [&](unsigned int bit_index) {
						if (memcmp(characters.buffer + index + bit_index, token, token_size * sizeof(char)) == 0) {
							found_match = bit_index;
							return true;
						}
						return false;
					});

					if (found_match != -1) {
						const char* string = characters.buffer + index + found_match;
						return { string, PointerDifference(characters.buffer + characters.size, string) };
					}
				}
			}

			// For the remaining slots, just call memcmp for each index
			for (size_t index = simd_count; index < iteration_count; index++) {
				if (memcmp(characters.buffer + index, token, sizeof(char) * token_size) == 0) {
					const char* string = characters.buffer + index;
					return { string, PointerDifference(characters.buffer + characters.size, string) };
				}
			}

			return { nullptr, 0 };
		}

		// --------------------------------------------------------------------------------------------------

		Stream<char> FindFirstCharacter(Stream<char> characters, char token)
		{
			Vec32c simd_token(token);

			size_t simd_count = GetSimdCount(characters.size, simd_token.size());
			for (size_t index = 0; index < simd_count; index += simd_token.size()) {
				Vec32c current_chars = Vec32c().load(characters.buffer + index);
				auto match = current_chars == simd_token;
				if (horizontal_or(match)) {
					unsigned int mask = to_bits(match);
					unsigned long bit_index = 0;
					_BitScanForward(&bit_index, mask);

					index += bit_index;
					return { characters.buffer + index, characters.size - index };
				}
			}

			for (size_t index = simd_count; index < characters.size; index++) {
				if (characters[index] == token) {
					return { characters.buffer + index, characters.size - index };
				}
			}

			return { nullptr, 0 };
		}

		// --------------------------------------------------------------------------------------------------

		const char* FindTokenReverse(const char* characters, const char* token)
		{
			size_t character_size = strlen(characters);
			size_t token_size = strlen(token);

			constexpr size_t all_true = 0xFFFFFFFFFFFFFFFF;
			size_t token_size_iteration = token_size / 8;

			size_t remainder = token_size % 8;
			if (remainder != 0) {
				const size_t* token_size_t = (const size_t*)token;
				if (token_size_iteration > 0) {
					for (size_t index = 0; index < character_size - token_size + 1; index++) {
						size_t iteration = 0;
						const size_t* reinterpretation = (const size_t*)(characters + character_size - token_size - index);
						while (iteration < token_size_iteration && *(reinterpretation + iteration) == *(token_size_t + iteration)) {
							iteration++;
						}
						if (iteration == token_size_iteration) {
							size_t last_index = 0;
							while (last_index < 8 && characters[character_size - index - token_size + last_index] == token[token_size - token_size + last_index]) {
								last_index++;
							}
							if (last_index == remainder) {
								return characters + character_size - token_size - index;
							}
						}
					}
				}
				else {
					for (size_t index = 0; index < character_size - token_size + 1; index++) {
						size_t last_index = 0;
						while (last_index < 8 && characters[character_size - index - token_size + last_index] == token[token_size - token_size + last_index]) {
							last_index++;
						}
						if (last_index == remainder) {
							return characters + character_size - token_size - index;
						}
					}
				}

				return nullptr;
			}
			else {
				const size_t* token_size_t = (const size_t*)token;
				for (size_t index = 0; index < character_size - token_size + 1; index++) {
					size_t iteration = 0;
					const size_t* reinterpretation = (const size_t*)(characters + character_size - token_size - index);
					while (iteration < token_size_iteration && *(reinterpretation + iteration) == *(token_size_t + iteration)) {
						iteration++;
					}
					if (iteration == token_size_iteration) {
						return characters + character_size - token_size - index;
					}
				}

				return nullptr;
			}
		}

		// --------------------------------------------------------------------------------------------------

		const char* FindCharacterReverse(const char* ending_character, const char* lower_bound, char character)
		{
			size_t difference = PointerDifference(ending_character, lower_bound);
			size_t index = 0;
			size_t simd_bound = difference >= 32 ? difference - 31 : 0;

			if (simd_bound > 0) {
				Vec32c simd_vector(character);

				for (; index < simd_bound; index++) {
					const char* data_to_load = ending_character - simd_vector.size() - index + 1;
					Vec32c string_chars = Vec32c().load(data_to_load);
					auto is_match = simd_vector == string_chars;

					// At least one character was found
					if (horizontal_or(is_match)) {
						unsigned int bit_mask = to_bits(is_match);
						unsigned long highest_bit = -1;
						if (_BitScanReverse(&highest_bit, bit_mask)) {
							return data_to_load + highest_bit;
						}
					}
				}
			}
			
			// Do a bytewise check
			for (; index < difference; index++) {
				if (ending_character[-index] == character) {
					return ending_character - index;
				}
			}

			return nullptr;
		}

		// --------------------------------------------------------------------------------------------------

		void* RemapPointer(const void* first_base, const void* second_base, const void* pointer)
		{
			return OffsetPointer(second_base, PointerDifference(pointer, first_base));
		}

		// --------------------------------------------------------------------------------------------------

		unsigned int FindString(const char* ECS_RESTRICT string, Stream<const char*> other)
		{
			Stream<char> stream_string = ToStream(string);
			for (size_t index = 0; index < other.size; index++) {
				if (CompareStrings(stream_string, Stream<char>(other[index], strlen(other[index])))) {
					return index;
				}
			}
			return -1;
		}

		// --------------------------------------------------------------------------------------------------

		unsigned int FindString(Stream<char> string, Stream<Stream<char>> other)
		{
			for (size_t index = 0; index < other.size; index++) {
				if (CompareStrings(string, other[index])) {
					return index;
				}
			}
			return -1;
		}

		// --------------------------------------------------------------------------------------------------

		unsigned int FindString(const wchar_t* ECS_RESTRICT string, Stream<const wchar_t*> other) {
			Stream<wchar_t> stream_string = ToStream(string);
			for (size_t index = 0; index < other.size; index++) {
				if (CompareStrings(string, other[index])) {
					return index;
				}
			}
			return -1;
		}

		// --------------------------------------------------------------------------------------------------

		unsigned int FindString(Stream<wchar_t> string, Stream<Stream<wchar_t>> other) {
			for (size_t index = 0; index < other.size; index++) {
				if (CompareStrings(string, other[index])) {
					return index;
				}
			}
			return -1;
		}

		// --------------------------------------------------------------------------------------------------

		void* CoallesceStreamWithData(void* allocation, size_t size)
		{
			uintptr_t buffer = (uintptr_t)allocation;
			buffer += sizeof(Stream<void>);
			Stream<void>* stream = (Stream<void>*)allocation;
			stream->InitializeFromBuffer(buffer, size);
			return allocation;
		}

		// --------------------------------------------------------------------------------------------------

		void* CoallesceCapacityStreamWithData(void* allocation, size_t size, size_t capacity) {
			uintptr_t buffer = (uintptr_t)allocation;
			buffer += sizeof(CapacityStream<void>);
			CapacityStream<void>* stream = (CapacityStream<void>*)allocation;
			stream->InitializeFromBuffer(buffer, size, capacity);
			return allocation;
		}

		// --------------------------------------------------------------------------------------------------

		bool IsFloatingPointNumber(Stream<char> characters)
		{
			unsigned int dot_count = 0;
			size_t starting_index = characters[0] == '+' || characters[0] == '-';
			for (size_t index = starting_index; index < characters.size; index++) {
				if ((characters[index] < '0' || characters[index] > '9') && characters[index] != '.') {
					return false;
				}

				dot_count += characters[index] == '.';
			}
			return dot_count <= 1;
		}

		// --------------------------------------------------------------------------------------------------

		bool IsIntegerNumber(Stream<char> characters)
		{
			size_t starting_index = characters[0] == '+' || characters[0] == '-';
			for (size_t index = starting_index; index < characters.size; index++) {
				if (characters[index] < '0' || characters[index] > '9') {
					return false;
				}
			}
			return true;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int GetAlphabetIndex(char character) {
			if (character >= '!' && character <= '~')
				return character - '!';
			if (character == ' ')
				return (unsigned int)AlphabetIndex::Space;
			if (character == '\t')
				return (unsigned int)AlphabetIndex::Tab;
			return (unsigned int)AlphabetIndex::Unknown;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		unsigned int GetAlphabetIndex(char character, CharacterType& character_type) {
			unsigned int uv_index = character - '!';
			if (character == ' ') {
				character_type = CharacterType::Space;
				return uv_index;
			}
			if (character < '!') {
				character_type = CharacterType::Unknown;
				return uv_index;
			}
			if (character < '0') {
				character_type = CharacterType::Symbol;
				return uv_index;
			}
			if (character <= '9') {
				character_type = CharacterType::Digit;
				return uv_index;
			}
			if (character < 'A') {
				character_type = CharacterType::Symbol;
				return uv_index;
			}
			if (character <= 'Z') {
				character_type = CharacterType::CapitalLetter;
				return uv_index;
			}
			if (character < 'a') {
				character_type = CharacterType::Symbol;
				return uv_index;
			}
			if (character <= 'z') {
				character_type = CharacterType::LowercaseLetter;
				return uv_index;
			}
			if (character <= '~') {
				character_type = CharacterType::Symbol;
				return uv_index;
			}
			if (character == '\t') {
				character_type = CharacterType::Tab;
				return (unsigned int)AlphabetIndex::Tab;
			}
			character_type = CharacterType::Unknown;
			return (unsigned int)AlphabetIndex::Unknown;
		}

		// --------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		void ConvertDateToStringImplementation(Date date, Stream<CharacterType>& characters, size_t format_flags) {
			auto flag = [&](size_t integer) {
				CharacterType temp[256];
				Stream<CharacterType> temp_stream = Stream<CharacterType>(temp, 0);
				if (integer < 10) {
					temp_stream.Add(Character<CharacterType>('0'));
				}
				function::ConvertIntToChars(temp_stream, integer);

				characters.AddStream(temp_stream);
			};

			CharacterType colon_or_dash = (format_flags & ECS_LOCAL_TIME_FORMAT_DASH_INSTEAD_OF_COLON) != 0 ? Character<CharacterType>('-') : Character<CharacterType>(':');

			bool has_hour = false;
			if (function::HasFlag(format_flags, ECS_LOCAL_TIME_FORMAT_HOUR)) {
				flag(date.hour);
				has_hour = true;
			}

			bool has_minutes = false;
			if (function::HasFlag(format_flags, ECS_LOCAL_TIME_FORMAT_MINUTES)) {
				if (has_hour) {
					characters.Add(colon_or_dash);
				}
				has_minutes = true;
				flag(date.minute);
			}

			bool has_seconds = false;
			if (function::HasFlag(format_flags, ECS_LOCAL_TIME_FORMAT_SECONDS)) {
				if (has_minutes || has_hour) {
					characters.Add(colon_or_dash);
				}
				has_seconds = true;
				flag(date.seconds);
			}

			bool has_milliseconds = false;
			if (function::HasFlag(format_flags, ECS_LOCAL_TIME_FORMAT_MILLISECONDS)) {
				if (has_hour || has_minutes || has_seconds) {
					characters.Add(colon_or_dash);
				}
				has_milliseconds = true;
				flag(date.milliseconds);
			}

			bool has_hour_minutes_seconds_milliseconds = has_hour || has_minutes || has_seconds || has_milliseconds;
			bool has_space_been_written = false;

			bool has_day = false;
			if (function::HasFlag(format_flags, ECS_LOCAL_TIME_FORMAT_DAY)) {
				if (!has_space_been_written && has_hour_minutes_seconds_milliseconds) {
					characters.Add(Character<CharacterType>(' '));
					has_space_been_written = true;
				}
				has_day = true;
				flag(date.day);
			}

			bool has_month = false;
			if (function::HasFlag(format_flags, ECS_LOCAL_TIME_FORMAT_MONTH)) {
				if (!has_space_been_written && has_hour_minutes_seconds_milliseconds) {
					characters.Add(Character<CharacterType>(' '));
					has_space_been_written = true;
				}
				if (has_day) {
					characters.Add(Character<CharacterType>('-'));
				}
				has_month = true;
				flag(date.month);
			}

			if (function::HasFlag(format_flags, ECS_LOCAL_TIME_FORMAT_YEAR)) {
				if (!has_space_been_written && has_hour_minutes_seconds_milliseconds) {
					characters.Add(Character<CharacterType>(' '));
					has_space_been_written = true;
				}
				if (has_day || has_month) {
					characters.Add(Character<CharacterType>('-'));
				}
				flag(date.year);
			}

			characters[characters.size] = Character<CharacterType>('\0');
		}

		void ConvertDateToString(Date date, Stream<char>& characters, size_t format_flags)
		{
			ConvertDateToStringImplementation(date, characters, format_flags);
		}

		// --------------------------------------------------------------------------------------------------

		void ConvertDateToString(Date date, CapacityStream<char>& characters, size_t format_flags)
		{
			Stream<char> stream(characters);
			ConvertDateToString(date, stream, format_flags);
			characters.size = stream.size;
			characters.AssertCapacity();
		}

		// --------------------------------------------------------------------------------------------------

		void ConvertDateToString(Date date, Stream<wchar_t>& characters, size_t format_flags) {
			ConvertDateToStringImplementation(date, characters, format_flags);
		}

		// --------------------------------------------------------------------------------------------------

		void ConvertDateToString(Date date, CapacityStream<wchar_t>& characters, size_t format_flags) {
			Stream<wchar_t> stream(characters);
			ConvertDateToString(date, stream, format_flags);
			characters.size = stream.size;
			characters.AssertCapacity();
		}

		// --------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		Date ConvertStringToDateImplementation(Stream<CharacterType> characters, size_t format_flags) {
			Date date;

			if (characters[0] == Character<CharacterType>('[')) {
				characters.buffer++;
				characters.size--;
			}

			const CharacterType* boundary = characters.buffer + characters.size;
			auto strchr_byte_by_byte = [](const CharacterType* characters, const CharacterType* boundary, CharacterType character) {
				while (characters < boundary && *characters != Character<CharacterType>(character)) {
					characters++;
				}
				return characters;
			};

			auto skip = [](const CharacterType* characters, const CharacterType* boundary) {
				while (characters < boundary && (*characters < Character<CharacterType>('0') || *characters > Character<CharacterType>('9'))) {
					characters++;
				}
				return characters;
			};

			auto get_integer = [](const CharacterType** characters, const CharacterType* boundary) {
				int64_t number = 0;
				while (*characters < boundary && (**characters >= Character<CharacterType>('0') && **characters <= Character<CharacterType>('9'))) {
					number = number * 10 + ((int64_t)(**characters) - (int64_t)Character<CharacterType>('0'));
					(*characters)++;
				}
				return number;
			};

			/*unsigned char semicolon_count = 0;
			unsigned char dash_count = 0;

			const CharacterType* semicolon = strchr_byte_by_byte(characters.buffer, boundary, Character<CharacterType>(':'));
			if (semicolon < boundary) {
				semicolon_count++;
				semicolon = strchr_byte_by_byte(semicolon + 1, boundary, Character<CharacterType>(':'));
				if (semicolon < boundary) {
					semicolon_count++;
					semicolon = strchr_byte_by_byte(semicolon + 1, boundary, Character<CharacterType>(':'));
					if (semicolon < boundary) {
						semicolon_count++;
					}
				}
			}

			const CharacterType* dash = strchr_byte_by_byte(characters.buffer, boundary, Character<CharacterType>('-'));
			if (dash < boundary) {
				dash_count++;
				dash = strchr_byte_by_byte(dash + 1, boundary, Character<CharacterType>('-'));
				if (dash < boundary) {
					dash_count++;
				}
			}*/

			int64_t numbers[10];
			unsigned char number_count = 0;

			const CharacterType* current_character = characters.buffer;
			while (current_character < boundary) {
				current_character = skip(current_character, boundary);
				numbers[number_count++] = get_integer(&current_character, boundary);
			}

			ECS_ASSERT(number_count > 0);
			unsigned char numbers_parsed = 0;
			if (format_flags & ECS_LOCAL_TIME_FORMAT_HOUR) {
				date.hour = numbers[0];
				numbers_parsed++;
			}
			if (format_flags & ECS_LOCAL_TIME_FORMAT_MINUTES) {
				ECS_ASSERT(number_count > numbers_parsed);
				date.minute = numbers[numbers_parsed];
				numbers_parsed++;
			}
			if (format_flags & ECS_LOCAL_TIME_FORMAT_SECONDS) {
				ECS_ASSERT(number_count > numbers_parsed);
				date.seconds = numbers[numbers_parsed];
				numbers_parsed++;
			}
			if (format_flags & ECS_LOCAL_TIME_FORMAT_MILLISECONDS) {
				ECS_ASSERT(number_count > numbers_parsed);
				date.milliseconds = numbers[numbers_parsed];
				numbers_parsed++;
			}

			if (format_flags & ECS_LOCAL_TIME_FORMAT_DAY) {
				ECS_ASSERT(number_count > numbers_parsed);
				date.day = numbers[numbers_parsed];
				numbers_parsed++;
			}
			if (format_flags & ECS_LOCAL_TIME_FORMAT_MONTH) {
				ECS_ASSERT(number_count > numbers_parsed);
				date.month = numbers[numbers_parsed];
				numbers_parsed++;
			}
			if (format_flags & ECS_LOCAL_TIME_FORMAT_YEAR) {
				ECS_ASSERT(number_count > numbers_parsed);
				date.year = numbers[numbers_parsed];
				numbers_parsed++;
			}

			return date;
		}

		Date ConvertStringToDate(Stream<char> characters, size_t format_flags)
		{
			return ConvertStringToDateImplementation(characters, format_flags);
		}

		// --------------------------------------------------------------------------------------------------

		Date ConvertStringToDate(Stream<wchar_t> characters, size_t format_flags)
		{
			return ConvertStringToDateImplementation(characters, format_flags);
		}

		// --------------------------------------------------------------------------------------------------

		template<typename CharacterType, typename StreamType>
		void ConvertByteSizeToStringImplementation(size_t byte_size, StreamType& characters) {
			if (byte_size < ECS_KB) {
				function::ConvertIntToChars(characters, byte_size);
				characters.Add(Character<CharacterType>(' '));
				characters.Add(Character<CharacterType>('b'));
				characters.Add(Character<CharacterType>('y'));
				characters.Add(Character<CharacterType>('t'));
				characters.Add(Character<CharacterType>('e'));
				characters.Add(Character<CharacterType>('s'));
			}
			else if (byte_size < ECS_MB) {
				float kilobytes = (float)byte_size / (float)ECS_KB;
				function::ConvertFloatToChars(characters, kilobytes, 2);
				characters.Add(Character<CharacterType>(' '));
				characters.Add(Character<CharacterType>('K'));
				characters.Add(Character<CharacterType>('B'));
			}
			else if (byte_size < ECS_GB) {
				float mb = (float)byte_size / (float)ECS_MB;
				function::ConvertFloatToChars(characters, mb, 2);
				characters.Add(Character<CharacterType>(' '));
				characters.Add(Character<CharacterType>('M'));
				characters.Add(Character<CharacterType>('B'));
			}
			else if (byte_size < ECS_TB) {
				float gb = (float)byte_size / (float)ECS_GB;
				function::ConvertFloatToChars(characters, gb, 2);
				characters.Add(Character<CharacterType>(' '));
				characters.Add(Character<CharacterType>('G'));
				characters.Add(Character<CharacterType>('B'));
			}
			else if (byte_size < ECS_TB * ECS_KB) {
				float tb = (float)byte_size / (float)ECS_TB;
				function::ConvertFloatToChars(characters, tb, 2);
				characters.Add(Character<CharacterType>(' '));
				characters.Add(Character<CharacterType>('T'));
				characters.Add(Character<CharacterType>('B'));
			}
			else {
				ECS_ASSERT(false);
			}

			characters[characters.size] = Character<CharacterType>('\0');
		}

		void ConvertByteSizeToString(size_t byte_size, Stream<char>& characters)
		{
			ConvertByteSizeToStringImplementation<char>(byte_size, characters);
		}

		void ConvertByteSizeToString(size_t byte_size, CapacityStream<char>& characters)
		{
			ConvertByteSizeToStringImplementation<char>(byte_size, characters);
			characters.AssertCapacity();
		}
		
		void ConvertByteSizeToString(size_t byte_size, Stream<wchar_t>& characters) {
			ConvertByteSizeToStringImplementation<wchar_t>(byte_size, characters);
		}

		void ConvertByteSizeToString(size_t byte_size, CapacityStream<wchar_t>& characters)
		{
			ConvertByteSizeToStringImplementation<wchar_t>(byte_size, characters);
			characters.AssertCapacity();
		}

		// --------------------------------------------------------------------------------------------------

		bool IsDateLater(Date first, Date second)
		{
			if (second.year > first.year) {
				return true;
			}
			if (second.year == first.year) {
				if (second.month > first.month) {
					return true;
				}
				if (second.month == first.month) {
					if (second.day > first.day) {
						return true;
					}
					if (second.day == first.day) {
						if (second.hour > first.hour) {
							return true;
						}
						if (second.hour == first.hour) {
							if (second.minute > first.minute) {
								return true;
							}
							if (second.minute == first.minute) {
								if (second.seconds > first.seconds) {
									return true;
								}
								if (second.seconds == first.seconds) {
									return second.milliseconds > first.milliseconds;
								}
								return false;
							}
							return false;
						}
						return false;
					}
					return false;
				}
				return false;
			}
			return false;
		}

		// --------------------------------------------------------------------------------------------------

		void SetErrorMessage(CapacityStream<char>* error_message, Stream<char> message)
		{
			if (error_message != nullptr) {
				error_message->AddStreamSafe(message);
			}
		}

		// --------------------------------------------------------------------------------------------------

		void SetErrorMessage(CapacityStream<char>* error_message, const char* message)
		{
			if (error_message != nullptr) {
				error_message->AddStreamSafe(ToStream(message));
			}
		}

		// --------------------------------------------------------------------------------------------------

		// --------------------------------------------------------------------------------------------------

		// --------------------------------------------------------------------------------------------------

	}


}