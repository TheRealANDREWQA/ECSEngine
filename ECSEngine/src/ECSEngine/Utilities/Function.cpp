#include "ecspch.h"
#include "../Core.h"
#include "Function.h"
#include "FunctionInterfaces.h"
#include "../Allocators/AllocatorPolymorphic.h"

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

		size_t ParseWordsFromSentence(const char* sentence)
		{
			size_t length = strlen(sentence);

			size_t count = 0;
			for (size_t index = 0; index < length; index++) {
				count += (sentence[index] == ' ' || sentence[index] == '\n');
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

		const char* FindTokenReverse(const char* ECS_RESTRICT characters, const char* ECS_RESTRICT token)
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
							while (last_index < 8 && characters[character_size - index - 8 + last_index] == token[token_size - 8 + last_index]) {
								last_index++;
							}
							if (last_index == remainder) {
								return characters - token_size - index;
							}
						}
					}
				}
				else {
					for (size_t index = 0; index < character_size - token_size + 1; index++) {
						size_t last_index = 0;
						while (last_index < 8 && characters[character_size - index - 8 + last_index] == token[token_size - 8 + last_index]) {
							last_index++;
						}
						if (last_index == remainder) {
							return characters - token_size - index;
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
						return characters - token_size - index;
					}
				}

				return nullptr;
			}
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
				if (characters[index] < '0' || characters[index] > '9' && characters[index] != '.') {
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

		void ConvertDateToString(Date date, Stream<char>& characters, size_t format_flags)
		{
			characters.Add('[');

			auto flag = [&](size_t integer) {
				char temp[256];
				Stream<char> temp_stream = Stream<char>(temp, 0);
				function::ConvertIntToChars(temp_stream, integer);
				for (size_t index = 0; index < temp_stream.size; index++) {
					characters.Add(temp_stream[index]);
				}
			};

			bool has_hour = false;
			if (function::HasFlag(format_flags, ECS_LOCAL_TIME_FORMAT_HOUR)) {
				flag(date.hour);
				has_hour = true;
			}

			bool has_minutes = false;
			if (function::HasFlag(format_flags, ECS_LOCAL_TIME_FORMAT_MINUTES)) {
				if (has_hour) {
					characters.Add(':');
				}
				has_minutes = true;
				flag(date.minute);
			}

			bool has_seconds = false;
			if (function::HasFlag(format_flags, ECS_LOCAL_TIME_FORMAT_SECONDS)) {
				if (has_minutes || has_hour) {
					characters.Add(':');
				}
				has_seconds = true;
				flag(date.seconds);
			}

			bool has_milliseconds = false;
			if (function::HasFlag(format_flags, ECS_LOCAL_TIME_FORMAT_MILLISECONDS)) {
				if (has_hour || has_minutes || has_seconds) {
					characters.Add(':');
				}
				has_milliseconds = true;
				flag(date.milliseconds);
			}

			bool has_hour_minutes_seconds_milliseconds = has_hour || has_minutes || has_seconds || has_milliseconds;
			bool has_space_been_written = false;

			bool has_day = false;
			if (function::HasFlag(format_flags, ECS_LOCAL_TIME_FORMAT_DAY)) {
				if (!has_space_been_written && has_hour_minutes_seconds_milliseconds) {
					characters.Add(' ');
					has_space_been_written = true;
				}
				has_day = true;
				flag(date.day);
			}

			bool has_month = false;
			if (function::HasFlag(format_flags, ECS_LOCAL_TIME_FORMAT_MONTH)) {
				if (!has_space_been_written && has_hour_minutes_seconds_milliseconds) {
					characters.Add(' ');
					has_space_been_written = true;
				}
				if (has_day) {
					characters.Add('-');
				}
				has_month = true;
				flag(date.month);
			}

			if (function::HasFlag(format_flags, ECS_LOCAL_TIME_FORMAT_YEAR)) {
				if (!has_space_been_written && has_hour_minutes_seconds_milliseconds) {
					characters.Add(' ');
					has_space_been_written = true;
				}
				if (has_day || has_month) {
					characters.Add('-');
				}
				flag(date.year);
			}

			characters.Add(']');
			characters.Add(' ');
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