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

		size_t ParseWordsFromSentence(Stream<char> sentence, char separator_token)
		{
			size_t count = 0;
			for (size_t index = 0; index < sentence.size; index++) {
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
			float2 difference = { is_negative_slope ? x_difference : -x_difference, is_negative_slope ? y_difference : -y_difference };
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

		size_t ConvertDurationToChars(size_t duration, char* characters)
		{
			auto append_lambda = [characters](size_t integer) {
				size_t character_count = strlen(characters);
				Stream<char> stream = Stream<char>(characters, character_count);
				return function::ConvertIntToCharsFormatted(stream, (int64_t)integer);
			};

			size_t write_size = 0;
			const char* string = nullptr;

			// less than 10 seconds
			if (duration < 10000) {
				string = "Just now";
				strcat(characters, string);
			}
			// less than 60 seconds - 1 minute
			else if (duration < 60'000) {
				string = "Less than a minute ago";
				strcat(characters, string);
			}
			// less than 60 minutes - 1 hour
			else if (duration < 3'600'000) {
				size_t minutes = duration / 60'000;

				if (minutes > 1) {
					string = " minutes ago";

					write_size = append_lambda(minutes);
					strcat(characters, string);
				}
				else {
					string = "A minute ago";
					strcat(characters, string);
				}
			}
			// less than a 24 hours - 1 day
			else if (duration < 24 * 3'600'000) {
				size_t hours = duration / 3'600'000;
				if (hours > 1) {
					string = " hours ago";
					write_size = append_lambda(hours);
					strcat(characters, string);
				}
				else {
					string = "An hour ago";
					strcat(characters, string);
				}
			}
			// less than a 30 days - 1 months
			else if (duration < (size_t)30 * 24 * 3'600'000) {
				size_t days = duration / (24 * 3'600'000);
				if (days > 1) {
					string = " days ago";
					write_size = append_lambda(days);
					strcat(characters, string);
				}
				else {
					string = "A day ago";
					strcat(characters, string);
				}
			}
			// less than 12 months - 1 year
			else if (duration < (size_t)12 * 30 * 24 * 3'600'000) {
				size_t months = duration / ((size_t)30 * (size_t)24 * (size_t)3'600'000);
				if (months > 1) {
					string = " months ago";
					write_size = append_lambda(months);
					strcat(characters, string);
				}
				else {
					string = "A month ago";
					strcat(characters, string);
				}
			}
			// years
			else {
				size_t years = duration / ((size_t)12 * (size_t)30 * (size_t)24 * (size_t)3'600'000);
				if (years > 1) {
					string = " years ago";
					write_size = append_lambda(years);
					strcat(characters, string);
				}
				else {
					string = "A year ago";
					strcat(characters, string);
				}
			}

			return write_size + strlen(string);
		}

		// --------------------------------------------------------------------------------------------------

		template<typename VectorType, typename CharacterType>
		void FindTokenImpl(Stream<CharacterType> string, Stream<CharacterType> token, CapacityStream<unsigned int>& tokens) {
			// Could use the FindFirstToken function but the disadvantage to that function is
			// that we cannot cache the SIMD vectors
			// So unroll that manually here

			if (string.size < token.size) {
				return;
			}

			// If the token size is greater than 256 bytes (i.e. 8 32 byte element SIMD registers)
			// The check should be done manually with memcmp
			VectorType simd_token[8];
			VectorType first_char(token[0]);

			// Do a SIMD search for the first character
			// If we get a match, then do the SIMD search from that index

			size_t simd_token_count = 0;
			size_t iteration_count = string.size - token.size + 1;
			size_t simd_count = GetSimdCount(iteration_count, first_char.size());
			size_t simd_remainder = 0;
			if (token.size <= 256 / sizeof(CharacterType)) {
				while (token.size > first_char.size()) {
					size_t offset = simd_token_count * first_char.size();
					simd_token[simd_token_count++].load(token.buffer + offset);
					token.size -= first_char.size();
				}
				if (token.size > 0) {
					size_t offset = simd_token_count * first_char.size();
					simd_token[simd_token_count].load_partial(token.size, token.buffer + offset);
					simd_remainder = token.size;
				}

				for (size_t index = 0; index < simd_count; index += first_char.size()) {
					VectorType current_chars = VectorType().load(string.buffer + index);
					auto match = current_chars == first_char;

					ForEachBit(match, [&](unsigned int bit_index) {
						size_t subindex = 0;
						const CharacterType* current_characters = string.buffer + index + bit_index;
						VectorType simd_current_characters;

						for (; subindex < simd_token_count; subindex++) {
							simd_current_characters.load(current_characters);
							if (!horizontal_and(simd_token[subindex] == simd_current_characters)) {
								// They are different quit
								break;
							}
							current_characters += simd_current_characters.size();
						}

						// If they subindex is simd_token_count, they might be equal
						if (subindex == simd_token_count) {
							if (simd_remainder > 0) {
								simd_current_characters.load_partial(simd_remainder, current_characters);
								if (horizontal_and(simd_token[simd_token_count] == simd_current_characters)) {
									// Found a match
									tokens.Add(index + bit_index);
								}
							}
							else {
								// Found a match
								tokens.Add(index + bit_index);
							}
						}

						return false;
					});
				}
			}
			else {
				for (size_t index = 0; index < simd_count; index += first_char.size()) {
					VectorType current_chars = VectorType().load(string.buffer + index);
					auto match = current_chars == first_char;

					unsigned int found_match = -1;
					ForEachBit(match, [&](unsigned int bit_index) {
						if (memcmp(string.buffer + index + bit_index, token.buffer, token.size * sizeof(CharacterType)) == 0) {
							tokens.Add(index + bit_index);
						}
						return false;
					});
				}
			}

			// For the remaining slots, just call memcmp for each index
			for (size_t index = simd_count; index < iteration_count; index++) {
				if (string[index] == token[0]) {
					// Do a precull with the first character
					if (memcmp(string.buffer + index, token.buffer, sizeof(CharacterType) * token.size) == 0) {
						tokens.Add(index);
					}
				}
			}
		}

		// --------------------------------------------------------------------------------------------------

		template<typename VectorType, typename CharacterType>
		void FindTokenImpl(Stream<CharacterType> string, CharacterType token, CapacityStream<unsigned int>& tokens)
		{
			VectorType simd_token(token);
			VectorType compare;
			unsigned int simd_size = GetSimdCount(string.size, simd_token.size());
			for (unsigned int index = 0; index < simd_size; index += simd_token.size()) {
				compare.load(string.buffer + index);
				auto match = compare == simd_token;
				ForEachBit(match, [&](unsigned int bit_index) {
					tokens.Add(index + bit_index);
					return false;
				});
			}

			if (simd_size != string.size) {
				unsigned int count = string.size - simd_size;
				compare.load_partial(count * sizeof(CharacterType), string.buffer + simd_size);
				auto match = compare == simd_token;
				ForEachBit(match, [&](unsigned int bit_index) {
					tokens.Add(simd_size + bit_index);
					return false;
				});
			}

			tokens.AssertCapacity();
		}

		// --------------------------------------------------------------------------------------------------

		void FindToken(Stream<char> string, char token, CapacityStream<unsigned int>& tokens)
		{
			FindTokenImpl<Vec32c>(string, token, tokens);
		}

		// --------------------------------------------------------------------------------------------------

		void FindToken(Stream<char> string, Stream<char> token, CapacityStream<unsigned int>& tokens)
		{
			FindTokenImpl<Vec32c>(string, token, tokens);
		}

		// --------------------------------------------------------------------------------------------------

		void FindToken(Stream<wchar_t> string, wchar_t token, CapacityStream<unsigned int>& tokens)
		{
			FindTokenImpl<Vec16s>(string, token, tokens);
		}

		// --------------------------------------------------------------------------------------------------

		void FindToken(Stream<wchar_t> string, Stream<wchar_t> token, CapacityStream<unsigned int>& tokens) {
			FindTokenImpl<Vec16s>(string, token, tokens);
		}
		
		// --------------------------------------------------------------------------------------------------

		ECS_OPTIMIZE_END;

		// Unfortunately, cannot set the template to optimization, since probably it won't be taken into account
		// correctly. Must put the optimization pragmas at the instantiation
		template<bool reverse, typename VectorType, typename CharacterType>
		Stream<CharacterType> FindFirstTokenImpl(Stream<CharacterType> characters, Stream<CharacterType> token)
		{
			if (characters.size < token.size) {
				return { nullptr, 0 };
			}

			VectorType first_character;
			size_t last_character_to_check = characters.size - token.size + 1;
			size_t simd_count = GetSimdCount(last_character_to_check, first_character.size());

			// The scalar loop must be done before the 
			if constexpr (reverse) {
				// Use a scalar loop
				for (size_t index = simd_count; index < last_character_to_check; index++) {
					const CharacterType* character = characters.buffer + last_character_to_check - index + simd_count;
					if (*character == token[0]) {
						if (memcmp(character, token.buffer, token.size * sizeof(CharacterType)) == 0) {
							return { character, characters.size - function::PointerDifference(character, characters.buffer) / sizeof(CharacterType) };
						}
					}
				}
			}

			if (simd_count > 0) {
				first_character = VectorType(token[0]);
				size_t simd_width = first_character.size();

				VectorType simd_chars;
				for (size_t index = 0; index < simd_count; index += simd_width) {
					const CharacterType* character = characters.buffer + index;
					if constexpr (reverse) {
						character = characters.buffer + simd_count - simd_width - index;
					}
					simd_chars.load(character);
					auto compare = simd_chars == first_character;
					// For each bit do the precull
					unsigned int found_match = -1;
					ForEachBit<reverse>(compare, [&](unsigned int bit_index) {
						if (memcmp(character + bit_index, token.buffer, sizeof(CharacterType) * token.size) == 0) {
							found_match = bit_index;
							return true;
						}
						return false;
					});

					if (found_match != -1) {
						return { character + found_match, characters.size - function::PointerDifference(character + found_match, characters.buffer) / sizeof(CharacterType) };
					}
				}

			}

			// Use a scalar loop
			if constexpr (!reverse) {
				for (size_t index = simd_count; index < characters.size - token.size + 1; index++) {
					const CharacterType* character = characters.buffer + index;
					if (*character == token[0]) {
						if (memcmp(character, token.buffer, token.size * sizeof(CharacterType)) == 0) {
							return { character, characters.size - function::PointerDifference(character, characters.buffer) / sizeof(CharacterType) };
						}
					}
				}
			}

			return { nullptr, 0 };
		}

		// --------------------------------------------------------------------------------------------------

		Stream<char> FindFirstToken(Stream<char> characters, Stream<char> token) {
			return FindFirstTokenImpl<false, Vec32c>(characters, token);
		}

		// --------------------------------------------------------------------------------------------------

		Stream<wchar_t> FindFirstToken(Stream<wchar_t> characters, Stream<wchar_t> token) {
			return FindFirstTokenImpl<false, Vec16s>(characters, token);
		}

		// --------------------------------------------------------------------------------------------------

		Stream<char> FindFirstCharacter(Stream<char> characters, char token) {
			size_t index = SearchBytes(characters.buffer, characters.size, token, sizeof(token));
			if (index == -1) {
				return { nullptr, 0 };
			}
			return { characters.buffer + index, characters.size - index };
		}

		// --------------------------------------------------------------------------------------------------

		Stream<wchar_t> FindFirstCharacter(Stream<wchar_t> characters, wchar_t token) {
			size_t index = SearchBytes(characters.buffer, characters.size, (size_t)token, sizeof(token));
			if (index == -1) {
				return { nullptr, 0 };
			}
			return { characters.buffer + index, characters.size - index };
		}

		// --------------------------------------------------------------------------------------------------

		Stream<char> FindTokenReverse(Stream<char> characters, Stream<char> token)
		{
			return FindFirstTokenImpl<true, Vec32c>(characters, token);
		}

		// --------------------------------------------------------------------------------------------------

		Stream<wchar_t> FindTokenReverse(Stream<wchar_t> characters, Stream<wchar_t> token)
		{
			return FindFirstTokenImpl<true, Vec16s>(characters, token);
		}

		// --------------------------------------------------------------------------------------------------

		Stream<char> FindCharacterReverse(Stream<char> characters, char character)
		{
			size_t index = SearchBytesReversed(characters.buffer, characters.size, character, sizeof(character));
			if (index == -1) {
				return { nullptr, 0 };
			}
			return { characters.buffer + index, characters.size - index };
		}

		// --------------------------------------------------------------------------------------------------

		Stream<wchar_t> FindCharacterReverse(Stream<wchar_t> characters, wchar_t character)
		{
			size_t index = SearchBytesReversed(characters.buffer, characters.size, (size_t)character, sizeof(character));
			if (index == -1) {
				return { nullptr, 0 };
			}
			return { characters.buffer + index, characters.size - index };
		}

		// --------------------------------------------------------------------------------------------------

		void* RemapPointer(const void* first_base, const void* second_base, const void* pointer)
		{
			return OffsetPointer(second_base, PointerDifference(pointer, first_base));
		}

		// --------------------------------------------------------------------------------------------------

		unsigned int FindString(const char* ECS_RESTRICT string, Stream<const char*> other)
		{
			Stream<char> stream_string = string;
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
			Stream<wchar_t> stream_string = string;
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

		template<typename CharacterType>
		unsigned int FindStringOffsetImpl(Stream<CharacterType> string, const void* strings_buffer, size_t strings_count, size_t string_byte_size, unsigned int offset, bool capacity) {
			auto loop = [=](auto is_capacity) {
				for (unsigned int index = 0; index < strings_count; index++) {
					Stream<CharacterType> current_string;
					if constexpr (is_capacity) {
						current_string = *(CapacityStream<CharacterType>*)function::OffsetPointer(strings_buffer, index * string_byte_size + offset);
					}
					else {
						current_string = *(Stream<CharacterType>*)function::OffsetPointer(strings_buffer, index * string_byte_size + offset);
					}
					if (CompareStrings(string, current_string)) {
						return index;
					}
				}
				return (unsigned int)-1;
			};

			if (capacity) {
				return loop(std::true_type{});
			}
			else {
				return loop(std::false_type{});
			}
		}

		unsigned int FindStringOffset(Stream<char> string, const void* strings_buffer, size_t strings_count, size_t string_byte_size, unsigned int offset, bool capacity)
		{
			return FindStringOffsetImpl(string, strings_buffer, strings_count, string_byte_size, offset, capacity);
		}

		// --------------------------------------------------------------------------------------------------

		unsigned int FindStringOffset(Stream<wchar_t> string, const void* strings_buffer, size_t strings_count, size_t string_byte_size, unsigned int offset, bool capacity)
		{
			return FindStringOffsetImpl(string, strings_buffer, strings_count, string_byte_size, offset, capacity);
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

		void* CoallesceStreamWithData(AllocatorPolymorphic allocator, Stream<void> data, size_t element_size)
		{
			size_t allocation_size = sizeof(Stream<void>) + data.size;
			void* allocation = Allocate(allocator, allocation_size);
			Stream<void>* stream = (Stream<void>*)allocation;
			stream->InitializeFromBuffer(function::OffsetPointer(stream, sizeof(*stream)), data.size / element_size);
			memcpy(stream->buffer, data.buffer, data.size);
			return allocation;
		}

		// --------------------------------------------------------------------------------------------------

		bool IsFloatingPointNumber(Stream<char> characters)
		{
			unsigned int dot_count = 0;
			size_t starting_index = characters[0] == '+' || characters[0] == '-';
			for (size_t index = starting_index; index < characters.size; index++) {
				if ((characters[index] < '0' || characters[index] > '9') && characters[index] != '.') {
					// For the floating point if it has a trailing f allow it
					if (index != characters.size - 1 || characters[index] != 'f') {
						return false;
					}
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
				// The comma is a separator between the 1000 parts
				// Include these as well
				if ((characters[index] < '0' || characters[index] > '9') && characters[index] != ',') {
					return false;
				}
			}
			return true;
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		template<const char* (*SkipFunction)(const char* characters, int increment)>
		Stream<char> SkipStream(Stream<char> characters, int increment) {
			if (increment > 0) {
				const char* skipped = SkipFunction(characters.buffer, increment);
				return { skipped, function::PointerDifference(characters.buffer + characters.size, skipped) };
			}
			else {
				const char* last = characters.buffer + characters.size;
				const char* skipped = SkipFunction(last, increment);
				return { characters.buffer, function::PointerDifference(skipped, characters.buffer) };
			}
		}

		Stream<char> SkipSpaceStream(Stream<char> characters, int increment)
		{
			return SkipStream<SkipSpace>(characters, increment);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		Stream<char> SkipWhitespace(Stream<char> characters, int increment)
		{
			return SkipStream<SkipWhitespace>(characters, increment);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		Stream<char> SkipWhitespaceEx(Stream<char> characters, int increment)
		{
			return SkipStream<SkipWhitespaceEx>(characters, increment);
		}

		// -----------------------------------------------------------------------------------------------------------------------------------

		Stream<char> SkipCodeIdentifier(Stream<char> characters, int increment)
		{
			return SkipStream<SkipCodeIdentifier>(characters, increment);
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
				error_message->AddStreamSafe(message);
			}
		}

		// --------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		void ReplaceCharacterImpl(Stream<CharacterType> string, CharacterType token_to_be_replaced, CharacterType replacement) {
			auto character = function::FindFirstCharacter(string, token_to_be_replaced);
			while (character.size > 0) {
				character[0] = replacement;
				character.buffer += 1;
				character.size -= 1;

				character = function::FindFirstCharacter(character, token_to_be_replaced);
			}
		}

		void ReplaceCharacter(Stream<char> string, char token_to_be_replaced, char replacement)
		{
			ReplaceCharacterImpl(string, token_to_be_replaced, replacement);
		}

		// --------------------------------------------------------------------------------------------------

		void ReplaceCharacter(Stream<wchar_t> string, wchar_t token_to_be_replaced, wchar_t replacement)
		{
			ReplaceCharacterImpl(string, token_to_be_replaced, replacement);
		}

		// --------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		void ReplaceOccurencesImpl(CapacityStream<CharacterType>& string, Stream<ReplaceOccurence<CharacterType>> occurences, CapacityStream<CharacterType>* output_string) {
			// Null terminate the string
			CharacterType previous_character = string[string.size];
			string[string.size] = Character<CharacterType>('\0');

			// Get the list of occurences for all types
			ECS_STACK_CAPACITY_STREAM(uint2, replacement_positions, 512);
			ECS_STACK_CAPACITY_STREAM(unsigned int, current_occurence_positions, 1024);

			size_t insert_index = 0;
			auto insert_occurences = [&](uint2 occurence) {
				for (; insert_index < replacement_positions.size && replacement_positions[insert_index].x < occurence.x; insert_index++) {}
				if (insert_index == replacement_positions.size) {
					replacement_positions.AddSafe(occurence);
				}
				else {
					replacement_positions.Insert(insert_index, occurence);
				}
			};

			for (size_t index = 0; index < occurences.size; index++) {
				function::FindToken(string, occurences[index].string, current_occurence_positions);
				// Insert now the positions into the global buffer
				insert_index = 0;
				for (unsigned int occurence_index = 0; occurence_index < current_occurence_positions.size; occurence_index++) {
					insert_occurences({ current_occurence_positions[occurence_index], (unsigned int)index });
				}
			}

			// The idea is copy everything until a token appears, then replace it and keep doing it
			// until all tokens are used
			unsigned int copy_start_index = 0;
			for (size_t token_index = 0; token_index < replacement_positions.size; token_index++) {
				if (output_string != nullptr) {
					// Copy everything until the start of the token
					unsigned int end_position = replacement_positions[token_index].x - 1;
					Stream<CharacterType> copy_stream = Stream<CharacterType>(string.buffer + copy_start_index, end_position - copy_start_index + 1);
					output_string->AddStream(copy_stream);

					// Put the replacement in
					output_string->AddStream(occurences[replacement_positions[token_index].y].replacement);
					copy_start_index = end_position + occurences[replacement_positions[token_index].y].string.size + 1;
				}
				else {
					// Determine if the elements after need to be displaced
					unsigned int replacement_size = occurences[replacement_positions[token_index].y].replacement.size;
					unsigned int string_size = occurences[replacement_positions[token_index].y].string.size;

					int int_displacement = (int)replacement_size - (int)string_size;
					// Put the replacement in
					memcpy(
						string.buffer + replacement_positions[token_index].x,
						occurences[replacement_positions[token_index].y].replacement.buffer,
						sizeof(CharacterType) * replacement_size
					);

					// Displace the elements if different from 0
					string.DisplaceElements(replacement_positions[token_index].x + string_size, int_displacement);

					// It is fine if the int displacement is negative, displacement will be like 0xFFFFFFFF (-1)
					// which added with 0x1 will give 0 - the same as subtracting 1 from 1.
					unsigned int displacement = int_displacement;
					// Update the token positions for the next tokens
					for (size_t next_token = token_index + 1; next_token < replacement_positions.size; next_token++) {
						replacement_positions[next_token].x += displacement;
					}
				}
			}		

			string[string.size] = previous_character;
		}

		// --------------------------------------------------------------------------------------------------

		void ReplaceOccurrences(CapacityStream<char>& string, Stream<ReplaceOccurence<char>> occurences, CapacityStream<char>* output_string)
		{
			ReplaceOccurencesImpl(string, occurences, output_string);
		}

		// --------------------------------------------------------------------------------------------------

		void ReplaceOccurences(CapacityStream<wchar_t>& string, Stream<ReplaceOccurence<wchar_t>> occurences, CapacityStream<wchar_t>* output_string)
		{
			ReplaceOccurencesImpl(string, occurences, output_string);
		}

		// --------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperatorImpl(CharacterType character) {
			if (character == Character<CharacterType>('+')) {
				return ECS_EVALUATE_EXPRESSION_ADD;
			}
			else if (character == Character<CharacterType>('-')) {
				return ECS_EVALUATE_EXPRESSION_MINUS;
			}
			else if (character == Character<CharacterType>('*')) {
				return ECS_EVALUATE_EXPRESSION_MULTIPLY;
			}
			else if (character == Character<CharacterType>('/')) {
				return ECS_EVALUATE_EXPRESSION_DIVIDE;
			}
			else if (character == Character<CharacterType>('%')) {
				return ECS_EVALUATE_EXPRESSION_MODULO;
			}
			else if (character == Character<CharacterType>('~')) {
				return ECS_EVALUATE_EXPRESSION_NOT;
			}
			else if (character == Character<CharacterType>('|')) {
				return ECS_EVALUATE_EXPRESSION_OR;
			}
			else if (character == Character<CharacterType>('&')) {
				return ECS_EVALUATE_EXPRESSION_AND;
			}
			else if (character == Character<CharacterType>('^')) {
				return ECS_EVALUATE_EXPRESSION_XOR;
			}
			else if (character == Character<CharacterType>('<')) {
				return ECS_EVALUATE_EXPRESSION_LESS;
			}
			else if (character == Character<CharacterType>('>')) {
				return ECS_EVALUATE_EXPRESSION_GREATER;
			}
			else if (character == Character<CharacterType>('=')) {
				return ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY;
			}
			else if (character == Character<CharacterType>('!')) {
				return ECS_EVALUATE_EXPRESSION_LOGICAL_NOT;
			}

			return ECS_EVALUATE_EXPRESSION_OPERATOR_COUNT;
		}

		// --------------------------------------------------------------------------------------------------

		ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperator(char character)
		{
			return GetEvaluateExpressionOperatorImpl(character);
		}

		// --------------------------------------------------------------------------------------------------

		ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperator(wchar_t character)
		{
			return GetEvaluateExpressionOperatorImpl(character);
		}

		// --------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperatorImpl(CharacterType character, CharacterType next_character, unsigned int& index) {
			ECS_EVALUATE_EXPRESSION_OPERATORS current = GetEvaluateExpressionOperator(character);
			ECS_EVALUATE_EXPRESSION_OPERATORS next = GetEvaluateExpressionOperator(next_character);

			if (current == next) {
				switch (current) {
				case ECS_EVALUATE_EXPRESSION_AND:
					index++;
					return ECS_EVALUATE_EXPRESSION_LOGICAL_AND;
				case ECS_EVALUATE_EXPRESSION_OR:
					index++;
					return ECS_EVALUATE_EXPRESSION_LOGICAL_OR;
				case ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY:
					index++;
					return ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY;
					index++;
				case ECS_EVALUATE_EXPRESSION_LESS:
					index++;
					return ECS_EVALUATE_EXPRESSION_SHIFT_LEFT;
				case ECS_EVALUATE_EXPRESSION_GREATER:
					index++;
					return ECS_EVALUATE_EXPRESSION_SHIFT_RIGHT;
				default:
					// Return a failure if both characters are the same but not of the types with
					// double characters or they are both not an operator
					return ECS_EVALUATE_EXPRESSION_OPERATOR_COUNT;
				}
			}

			// Check for logical not equality
			if (current == ECS_EVALUATE_EXPRESSION_LOGICAL_NOT && next == ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY) {
				index++;
				return ECS_EVALUATE_EXPRESSION_LOGICAL_INEQUALITY;
			}

			// Check for less equal and greater equal
			if (current == ECS_EVALUATE_EXPRESSION_LESS && next == ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY) {
				index++;
				return ECS_EVALUATE_EXPRESSION_LESS_EQUAL;
			}

			if (current == ECS_EVALUATE_EXPRESSION_GREATER && next == ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY) {
				index++;
				return ECS_EVALUATE_EXPRESSION_GREATER_EQUAL;
			}

			// Else just return the current
			return current;
		}

		// --------------------------------------------------------------------------------------------------

		ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperator(char character, char next_character, unsigned int& index)
		{
			return GetEvaluateExpressionOperatorImpl(character, next_character, index);
		}

		// --------------------------------------------------------------------------------------------------

		ECS_EVALUATE_EXPRESSION_OPERATORS GetEvaluateExpressionOperator(wchar_t character, wchar_t next_character, unsigned int& index)
		{
			return GetEvaluateExpressionOperatorImpl(character, next_character, index);
		}

		// --------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		void GetEvaluateExpressionOperatorsImpl(Stream<CharacterType> characters, CapacityStream<EvaluateExpressionOperator>& operators) {
			for (unsigned int index = 0; index < characters.size; index++) {
				CharacterType character = characters[index];
				CharacterType next_character = index < characters.size - 1 ? characters[index + 1] : Character<CharacterType>('\0');

				ECS_EVALUATE_EXPRESSION_OPERATORS operator_ = GetEvaluateExpressionOperator(character, next_character, index);
				if (operator_ != ECS_EVALUATE_EXPRESSION_OPERATOR_COUNT) {
					// We have a match
					operators.Add({ operator_, index });
				}
			}
		}

		// --------------------------------------------------------------------------------------------------

		void GetEvaluateExpressionOperators(Stream<char> characters, CapacityStream<EvaluateExpressionOperator>& operators)
		{
			GetEvaluateExpressionOperatorsImpl<char>(characters, operators);
		}

		// --------------------------------------------------------------------------------------------------

		void GetEvaluateExpressionOperators(Stream<wchar_t> characters, CapacityStream<EvaluateExpressionOperator>& operators)
		{
			GetEvaluateExpressionOperatorsImpl<wchar_t>(characters, operators);
		}

		// --------------------------------------------------------------------------------------------------

		ECS_EVALUATE_EXPRESSION_OPERATORS ORDER0[] = {
			ECS_EVALUATE_EXPRESSION_LOGICAL_NOT,
			ECS_EVALUATE_EXPRESSION_NOT
		};

		ECS_EVALUATE_EXPRESSION_OPERATORS ORDER1[] = {
			ECS_EVALUATE_EXPRESSION_MULTIPLY,
			ECS_EVALUATE_EXPRESSION_DIVIDE,
			ECS_EVALUATE_EXPRESSION_MODULO
		};

		ECS_EVALUATE_EXPRESSION_OPERATORS ORDER2[] = {
			ECS_EVALUATE_EXPRESSION_ADD,
			ECS_EVALUATE_EXPRESSION_MINUS
		};

		ECS_EVALUATE_EXPRESSION_OPERATORS ORDER3[] = {
			ECS_EVALUATE_EXPRESSION_SHIFT_LEFT,
			ECS_EVALUATE_EXPRESSION_SHIFT_RIGHT
		};

		ECS_EVALUATE_EXPRESSION_OPERATORS ORDER4[] = {
			ECS_EVALUATE_EXPRESSION_LESS,
			ECS_EVALUATE_EXPRESSION_LESS_EQUAL,
			ECS_EVALUATE_EXPRESSION_GREATER,
			ECS_EVALUATE_EXPRESSION_GREATER_EQUAL
		};

		ECS_EVALUATE_EXPRESSION_OPERATORS ORDER5[] = {
			ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY,
			ECS_EVALUATE_EXPRESSION_LOGICAL_INEQUALITY
		};

		ECS_EVALUATE_EXPRESSION_OPERATORS ORDER6[] = {
			ECS_EVALUATE_EXPRESSION_AND
		};

		ECS_EVALUATE_EXPRESSION_OPERATORS ORDER7[] = {
			ECS_EVALUATE_EXPRESSION_XOR
		};

		ECS_EVALUATE_EXPRESSION_OPERATORS ORDER8[] = {
			ECS_EVALUATE_EXPRESSION_OR
		};

		ECS_EVALUATE_EXPRESSION_OPERATORS ORDER9[] = {
			ECS_EVALUATE_EXPRESSION_LOGICAL_AND
		};

		ECS_EVALUATE_EXPRESSION_OPERATORS ORDER10[] = {
			ECS_EVALUATE_EXPRESSION_LOGICAL_OR
		};

		Stream<ECS_EVALUATE_EXPRESSION_OPERATORS> OPERATOR_ORDER[] = {
			{ ORDER0, std::size(ORDER0) },
			{ ORDER1, std::size(ORDER1) },
			{ ORDER2, std::size(ORDER2) },
			{ ORDER3, std::size(ORDER3) },
			{ ORDER4, std::size(ORDER4) },
			{ ORDER5, std::size(ORDER5) },
			{ ORDER6, std::size(ORDER6) },
			{ ORDER7, std::size(ORDER7) },
			{ ORDER8, std::size(ORDER8) },
			{ ORDER9, std::size(ORDER9) },
			{ ORDER10, std::size(ORDER10) }
		};

		void GetEvaluateExpressionOperatorOrder(Stream<EvaluateExpressionOperator> operators, CapacityStream<unsigned int>& order)
		{
			for (size_t precedence = 0; precedence < std::size(OPERATOR_ORDER); precedence++) {
				for (size_t index = 0; index < operators.size; index++) {
					for (size_t op_index = 0; op_index < OPERATOR_ORDER[precedence].size; op_index++) {
						if (operators[index].operator_index == OPERATOR_ORDER[precedence][op_index]) {
							order.AddSafe(index);
						}
					}
				}
			}
		}

		// --------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		double EvaluateExpressionValueImpl(Stream<CharacterType> characters, unsigned int& offset) {
			// Verify numbers
			unsigned int first_offset = offset;
			CharacterType first = characters[offset];
			if ((first >= Character<CharacterType>('0') && first <= Character<CharacterType>('9')) || first == Character<CharacterType>('.')) {
				// It is a number
				// keep going
				offset++;
				CharacterType last = characters[offset];
				while (offset < characters.size && (last >= Character<CharacterType>('0') && last <= Character<CharacterType>('9')) ||
					last == Character<CharacterType>('.')) {
					offset++;
					last = characters[offset];
				}

				return function::ConvertCharactersToDouble(Stream<CharacterType>(characters.buffer + first_offset, offset - first_offset));
			}

			// Verify the boolean true and false
			CharacterType true_chars[] = {
				Character<CharacterType>('t'),
				Character<CharacterType>('r'),
				Character<CharacterType>('u'),
				Character<CharacterType>('e')
			};

			CharacterType false_chars[] = {
				Character<CharacterType>('f'),
				Character<CharacterType>('a'),
				Character<CharacterType>('l'),
				Character<CharacterType>('s'),
				Character<CharacterType>('e')
			};

			if (memcmp(true_chars, characters.buffer + offset, sizeof(true_chars)) == 0) {
				offset += 4;
				return 1.0;
			}
			if (memcmp(false_chars, characters.buffer + offset, sizeof(false_chars)) == 0) {
				offset += 5;
				return 0.0;
			}

			return DBL_MAX;
		}

		double EvaluateExpressionValue(Stream<char> characters, unsigned int& offset)
		{
			return EvaluateExpressionValueImpl<char>(characters, offset);
		}

		// --------------------------------------------------------------------------------------------------

		double EvaluateExpressionValue(Stream<wchar_t> characters, unsigned int& offset)
		{
			return EvaluateExpressionValueImpl<wchar_t>(characters, offset);
		}

		// --------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		double EvaluateExpressionImpl(Stream<CharacterType> characters) {
			// Start by looking at braces

			// Get the pairs of braces
			ECS_STACK_CAPACITY_STREAM(unsigned int, opened_braces, 512);
			ECS_STACK_CAPACITY_STREAM(unsigned int, closed_braces, 512);
			// Add an invisible pair of braces, in order to make processing easier
			opened_braces.Add((unsigned int)0);
			FindToken(characters, Character<CharacterType>('('), opened_braces);
			FindToken(characters, Character<CharacterType>(')'), closed_braces);
			closed_braces.AddSafe(characters.size + 1);

			auto get_matching_brace = [=](size_t closed_index) {
				size_t subindex = 0;
				if (closed_index < closed_braces.size - 1) {
					for (; subindex < opened_braces.size - 1; subindex++) {
						if (opened_braces[subindex] < closed_braces[closed_index] && opened_braces[subindex + 1] > closed_braces[closed_index]) {
							break;
						}
					}
				}
				else {
					subindex = 0;
				}

				return subindex;
			};

			// If the closed_braces count is different from opened braces count, fail with DBL_MAX
			if (opened_braces.size != closed_braces.size) {
				return DBL_MAX;
			}

			// Increase the indices by one for the braces in order to keep order between the first invisible
			// pair of braces and the rest
			for (size_t index = 1; index < opened_braces.size; index++) {
				opened_braces[index]++;
				closed_braces[index]++;
			}
			// This one gets incremented when it shouldn't
			closed_braces[closed_braces.size - 1]--;

			ECS_STACK_CAPACITY_STREAM(EvaluateExpressionNumber, variables, 512);
			ECS_STACK_CAPACITY_STREAM(EvaluateExpressionOperator, operators, 512);
			ECS_STACK_CAPACITY_STREAM(unsigned int, operator_order, 512);

			// In case of success, insert the value of a pair evaluation into the 
			// scope variables
			auto insert_brace_value = [&](
				Stream<EvaluateExpressionNumber> scope_variables, 
				Stream<EvaluateExpressionOperator> scope_operators, 
				unsigned int index, 
				double value
			) {
				// Remove all the scope variables and the operators
				size_t scope_offset = scope_variables.buffer - variables.buffer;
				variables.Remove(scope_offset, scope_variables.size);
				size_t operator_offset = scope_operators.buffer - operators.buffer;
				operators.Remove(operator_offset, scope_operators.size);

				size_t insert_index = 0;
				for (; insert_index < variables.size; insert_index++) {
					if (variables[insert_index].index > index) {
						break;
					}
				}
				variables.Insert(insert_index, { value, index });
			};

			// Get all the variables and scope operators
			GetEvaluateExpressionOperators(characters, operators);
			GetEvaluateExpressionNumbers(characters, variables);

			for (size_t index = 0; index < closed_braces.size; index++) {
				operator_order.size = 0;

				size_t opened_brace_index = get_matching_brace(index);
				unsigned int opened_brace_offset = opened_braces[opened_brace_index];
				unsigned int end_brace_offset = closed_braces[index];

				// For all comparisons we must use greater or equal because the parenthese position
				// is offseted by 1

				// Determine the operators and the variables in this scope
				size_t start_operators = 0;
				for (; start_operators < operators.size; start_operators++) {
					if (operators[start_operators].index >= opened_brace_offset) {
						break;
					}
				}
				size_t end_operators = start_operators;
				for (; end_operators < operators.size; end_operators++) {
					if (operators[end_operators].index >= end_brace_offset) {
						break;
					}
				}
				end_operators--;

				size_t start_variables = 0;
				for (; start_variables < variables.size; start_variables++) {
					if (variables[start_variables].index >= opened_brace_offset) {
						break;
					}
				}

				size_t end_variables = start_variables;
				for (; end_variables < variables.size; end_variables++) {
					if (variables[end_variables].index >= end_brace_offset) {
						break;
					}
				}
				end_variables--;

				Stream<EvaluateExpressionOperator> scope_operators = { operators.buffer + start_operators, end_operators - start_operators + 1 };
				Stream<EvaluateExpressionNumber> scope_variables = { variables.buffer + start_variables, end_variables - start_variables + 1 };
				size_t scope_variable_initial_count = scope_variables.size;
				size_t scope_operator_initial_count = scope_operators.size;

				// Use the index of the opened brace for insertion

				// Treat the case (-value) separately
				if (scope_operators.size == 1 && scope_variables.size == 1) {
					if (scope_operators[0].operator_index == ECS_EVALUATE_EXPRESSION_LOGICAL_NOT) {
						insert_brace_value(scope_variables, scope_operators, opened_brace_offset, !(bool)(scope_variables[0].value));
					}
					else if (scope_operators[0].operator_index == ECS_EVALUATE_EXPRESSION_MINUS) {
						insert_brace_value(scope_variables, scope_operators, opened_brace_offset, -scope_variables[0].value);
					}
					else {
						// Not a correct operator for a single value
						return DBL_MAX;
					}
				}
				else {
					// Get the precedence
					GetEvaluateExpressionOperatorOrder(scope_operators, operator_order);
					// Go through the operators, first the *, / and %
					for (size_t operator_index = 0; operator_index < scope_operators.size; operator_index++) {
						// Get the left variable
						unsigned int operator_offset = scope_operators[operator_order[operator_index]].index;
						ECS_EVALUATE_EXPRESSION_OPERATORS op_type = scope_operators[operator_order[operator_index]].operator_index;

						size_t right_index = 0;
						for (; right_index < scope_variables.size; right_index++) {
							if (right_index > 0) {
								if (scope_variables[right_index].index > operator_offset && scope_variables[right_index - 1].index < operator_offset) {
									break;
								}
							}
							else {
								if (scope_variables[right_index].index > operator_offset) {
									break;
								}
							}
						}

						if (right_index == scope_variables.size) {
							// No right value, fail
							return DBL_MAX;
						}

						// If single operator, evaluate now
						if (op_type == ECS_EVALUATE_EXPRESSION_LOGICAL_NOT || op_type == ECS_EVALUATE_EXPRESSION_NOT) {
							scope_variables[right_index].value = EvaluateOperator(op_type, 0.0, scope_variables[right_index].value);
							// We don't need to do anything else, no need to squash down a value
						}
						else {
							if (right_index == 0) {
								// Fail, no left value for double operand operator
								return DBL_MAX;
							}

							size_t left_index = right_index - 1;
							// If the left value is not to the left of the operator fail
							if (scope_variables[left_index].index >= operator_offset) {
								return DBL_MAX;
							}

							// Get the result into the left index and move everything one index lower in order to remove right index
							double result = EvaluateOperator(op_type, scope_variables[left_index].value, scope_variables[right_index].value);
							scope_variables[left_index].value = result;
							scope_variables.Remove(right_index);
						}
					}

					// If after processing all the operators there is more than 1 value, error
					if (scope_variables.size > 1) {
						return DBL_MAX;
					}

					// In order to remove the scope variables appropriately
					scope_variables.size = scope_variable_initial_count;
					scope_operators.size = scope_operator_initial_count;
					insert_brace_value(scope_variables, scope_operators, opened_brace_offset, scope_variables[0].value);
				}

				// Remove the brace from the stream
				opened_braces.Remove(opened_brace_index);
			}

			ECS_ASSERT(variables.size == 1);
			return variables[0].value;
		}

		// --------------------------------------------------------------------------------------------------

		template<typename CharacterType>
		void GetEvaluateExpressionNumbersImpl(Stream<CharacterType> characters, CapacityStream<EvaluateExpressionNumber>& numbers) {
			for (unsigned int index = 0; index < characters.size; index++) {
				// Skip whitespaces
				while (characters[index] == Character<CharacterType>(' ') || characters[index] == Character<CharacterType>('\t')) {
					index++;
				}

				unsigned int start_index = index;
				double value = EvaluateExpressionValue(characters, index);
				if (value != DBL_MAX) {
					numbers.AddSafe({ value, start_index });
				}
			}
		}

		// --------------------------------------------------------------------------------------------------

		void GetEvaluateExpressionNumbers(Stream<char> characters, CapacityStream<EvaluateExpressionNumber>& numbers) {
			GetEvaluateExpressionNumbersImpl(characters, numbers);
		}

		// --------------------------------------------------------------------------------------------------

		void GetEvaluateExpressionNumbers(Stream<wchar_t> characters, CapacityStream<EvaluateExpressionNumber>& numbers) {
			GetEvaluateExpressionNumbersImpl(characters, numbers);
		}

		// --------------------------------------------------------------------------------------------------

		double EvaluateOperator(ECS_EVALUATE_EXPRESSION_OPERATORS operator_, double left, double right)
		{
			switch (operator_)
			{
			case ECS_EVALUATE_EXPRESSION_ADD:
				return left + right;
			case ECS_EVALUATE_EXPRESSION_MINUS:
				return left - right;
			case ECS_EVALUATE_EXPRESSION_MULTIPLY:
				return left * right;
			case ECS_EVALUATE_EXPRESSION_DIVIDE:
				return left / right;
			case ECS_EVALUATE_EXPRESSION_MODULO:
				return (int64_t)left % (int64_t)right;
			case ECS_EVALUATE_EXPRESSION_NOT:
				return ~(int64_t)right;
			case ECS_EVALUATE_EXPRESSION_AND:
				return (int64_t)left & (int64_t)right;
			case ECS_EVALUATE_EXPRESSION_OR:
				return (int64_t)left | (int64_t)right;
			case ECS_EVALUATE_EXPRESSION_XOR:
				return (int64_t)left ^ (int64_t)right;
			case ECS_EVALUATE_EXPRESSION_SHIFT_LEFT:
				return (int64_t)left << (int64_t)right;
			case ECS_EVALUATE_EXPRESSION_SHIFT_RIGHT:
				return (int64_t)left >> (int64_t)right;
			case ECS_EVALUATE_EXPRESSION_LOGICAL_AND:
				return (bool)left && (bool)right;
			case ECS_EVALUATE_EXPRESSION_LOGICAL_OR:
				return (bool)left || (bool)right;
			case ECS_EVALUATE_EXPRESSION_LOGICAL_EQUALITY:
				return left == right;
			case ECS_EVALUATE_EXPRESSION_LOGICAL_INEQUALITY:
				return left != right;
			case ECS_EVALUATE_EXPRESSION_LOGICAL_NOT:
				return !(bool)right;
			case ECS_EVALUATE_EXPRESSION_LESS:
				return left < right;
			case ECS_EVALUATE_EXPRESSION_LESS_EQUAL:
				return left <= right;
			case ECS_EVALUATE_EXPRESSION_GREATER:
				return left > right;
			case ECS_EVALUATE_EXPRESSION_GREATER_EQUAL:
				return left >= right;
			default:
				ECS_ASSERT(false);
			}

			return 0.0;
		}

		// --------------------------------------------------------------------------------------------------

		double EvaluateExpression(Stream<char> characters)
		{
			return EvaluateExpressionImpl<char>(characters);
		}

		// --------------------------------------------------------------------------------------------------

		double EvaluateExpression(Stream<wchar_t> characters) {
			return EvaluateExpressionImpl<wchar_t>(characters);
		}

		// ----------------------------------------------------------------------------------------------------------

		template<bool reverse>
		size_t SearchBytesImpl(const void* data, size_t element_count, size_t value_to_search, size_t byte_size) {
			auto loop = [](const void* data, size_t element_count, auto values, auto simd_value_to_search, auto constant_byte_size) {
				constexpr size_t byte_size = constant_byte_size();
				size_t simd_width = simd_value_to_search.size();
				size_t simd_count = GetSimdCount(element_count, simd_width);
				for (size_t index = 0; index < simd_count; index += simd_width) {
					const void* ptr = function::OffsetPointer(data, byte_size * index);
					if constexpr (reverse) {
						ptr = function::OffsetPointer(data, byte_size * (element_count - simd_width - index));
					}

					values.load(ptr);

					auto compare = values == simd_value_to_search;
					if (horizontal_or(compare)) {
						unsigned int bits = to_bits(compare);
						unsigned int first = 0;
						if constexpr (reverse) {
							first = FirstMSB(bits);
						}
						else {
							first = FirstLSB(bits);
						}
						
						if (first != -1) {
							if constexpr (reverse) {
								return element_count - simd_width - index + first;
							}
							else {
								// We have a match
								return index + first;
							}
						}
					}
				}

				// The last elements
				int last_element_count = element_count - simd_count;
				const void* ptr = function::OffsetPointer(data, byte_size * simd_count);
				if constexpr (reverse) {
					ptr = data;
				}

				values.load(ptr);
				auto compare = values == simd_value_to_search;
				if (horizontal_or(compare)) {
					unsigned int bits = to_bits(compare);
					unsigned int first = 0;
					if constexpr (reverse) {
						first = FirstMSB(bits);
					}
					else {
						first = FirstLSB(bits);
					}

					if (first != -1 && first < last_element_count) {
						if constexpr (reverse) {
							return (size_t)first;
						}
						else {
							return simd_count + first;
						}
					}
					else {
						if constexpr (reverse) {
							// If reversed, then try again until the bits integer is 0 or we get a value that is in bounds
							bits &= ~(1 << first);
							while (bits != 0) {
								first = FirstMSB(bits);
								if (first != -1 && first < last_element_count) {
									return (size_t)first;
								}
								bits &= ~(1 << first);
							}
						}
					}
				}
				return (size_t)-1;
			};

			if (element_count == 0) {
				return -1;
			}

			if (byte_size == 1) {
				Vec32uc values;
				Vec32uc simd_value_to_search((unsigned char)value_to_search);

				return loop(data, element_count, values, simd_value_to_search, std::integral_constant<size_t, 1>());
			}
			else if (byte_size == 2) {
				Vec16us values;
				Vec16us simd_value_to_search((unsigned short)value_to_search);

				return loop(data, element_count, values, simd_value_to_search, std::integral_constant<size_t, 2>());
			}
			else if (byte_size == 4) {
				Vec8ui values;
				Vec8ui simd_value_to_search((unsigned int)value_to_search);

				return loop(data, element_count, values, simd_value_to_search, std::integral_constant<size_t, 4>());
			}
			else if (byte_size == 8) {
				Vec4uq values;
				Vec4uq simd_value_to_search(value_to_search);

				return loop(data, element_count, values, simd_value_to_search, std::integral_constant<size_t, 8>());
			}
			else {
				ECS_ASSERT(false);
			}

			return -1;
		}

		size_t SearchBytes(const void* data, size_t element_count, size_t value_to_search, size_t byte_size)
		{
			return SearchBytesImpl<false>(data, element_count, value_to_search, byte_size);
		}

		// --------------------------------------------------------------------------------------------------

		size_t SearchBytesReversed(const void* data, size_t element_count, size_t value_to_search, size_t byte_size)
		{
			return SearchBytesImpl<true>(data, element_count, value_to_search, byte_size);
		}

		// --------------------------------------------------------------------------------------------------

		template<bool reversed>
		size_t SearchBytesExImpl(const void* data, size_t element_count, const void* value_to_search, size_t byte_size) {
			if (byte_size == 1 || byte_size == 2 || byte_size == 4 || byte_size == 8) {
				return SearchBytesImpl<reversed>(data, element_count, *(size_t*)value_to_search, byte_size);
			}

			// Not in the fast case, use memcmp
			for (size_t index = 0; index < element_count; index++) {
				size_t current_index = index;
				if constexpr (reversed) {
					current_index = element_count - 1 - index;
				}

				const void* pointer = function::OffsetPointer(data, byte_size * current_index);
				if (memcmp(pointer, value_to_search, byte_size) == 0) {
					return current_index;
				}
			}
			return -1;
		}

		size_t SearchBytesEx(const void* data, size_t element_count, const void* value_to_search, size_t byte_size)
		{
			return SearchBytesExImpl<false>(data, element_count, value_to_search, byte_size);
		}

		size_t SearchBytesExReversed(const void* data, size_t element_count, const void* value_to_search, size_t byte_size)
		{
			return SearchBytesExImpl<true>(data, element_count, value_to_search, byte_size);
		}

		// --------------------------------------------------------------------------------------------------

		// --------------------------------------------------------------------------------------------------

	}


}