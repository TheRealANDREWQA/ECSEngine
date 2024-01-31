#include "ecspch.h"
#include "StringUtilities.h"
#include "Utilities.h"
#include "PointerUtilities.h"
#include "../Allocators/ResizableLinearAllocator.h"
#include "../Math/VCLExtensions.h"

namespace ECSEngine {

	// --------------------------------------------------------------------------------------------------

	template<typename CharacterType>
	static bool IsHexCharImpl(CharacterType value) {
		return IsNumberCharacter(value) || (value >= Character<CharacterType>('a') && value <= Character<CharacterType>('f')) ||
			(value >= Character<CharacterType>('A') && value <= Character<CharacterType>('F'));
	}

	bool IsHexChar(char value)
	{
		return IsHexCharImpl(value);
	}

	bool IsHexChar(wchar_t value)
	{
		return IsHexCharImpl(value);
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
			number_buffer[count++] = is_negative ? -(int)number : (int)number;
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

	size_t ConvertDurationToChars(size_t duration, char* characters)
	{
		auto append_lambda = [characters](size_t integer) {
			size_t character_count = strlen(characters);
			Stream<char> stream = Stream<char>(characters, character_count);
			return ConvertIntToCharsFormatted(stream, (int64_t)integer);
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

	// The functor receives a (unsigned int index) as parameters when a new entry should be added
	template<typename VectorType, typename CharacterType, typename Functor>
	void FindTokenImpl(Stream<CharacterType> string, Stream<CharacterType> token, Functor&& functor) {
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
								functor(index + bit_index);
							}
						}
						else {
							// Found a match
							functor(index + bit_index);
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
						functor(index + bit_index);
					}
					return false;
					});
			}
		}

		// For the remaining slots, just call memcmp for each index
		for (size_t index = simd_count; index < iteration_count; index++) {
			// Do a precull with the first character
			if (string[index] == token[0]) {
				if (memcmp(string.buffer + index, token.buffer, sizeof(CharacterType) * token.size) == 0) {
					functor(index);
				}
			}
		}
	}

	// --------------------------------------------------------------------------------------------------

	template<typename VectorType, typename CharacterType>
	void FindTokenImpl(Stream<CharacterType> string, CharacterType token, AdditionStream<unsigned int> tokens)
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
	}

	// --------------------------------------------------------------------------------------------------

	void FindToken(Stream<char> string, char token, AdditionStream<unsigned int> tokens)
	{
		FindTokenImpl<Vec32c>(string, token, tokens);
	}

	// --------------------------------------------------------------------------------------------------

	void FindToken(Stream<char> string, Stream<char> token, AdditionStream<unsigned int> tokens)
	{
		FindTokenImpl<Vec32c>(string, token, [&](unsigned int index) {
			tokens.Add(index);
			});
	}

	// --------------------------------------------------------------------------------------------------

	void FindToken(Stream<wchar_t> string, wchar_t token, AdditionStream<unsigned int> tokens)
	{
		FindTokenImpl<Vec16s>(string, token, tokens);
	}

	// --------------------------------------------------------------------------------------------------

	void FindToken(Stream<wchar_t> string, Stream<wchar_t> token, AdditionStream<unsigned int> tokens) {
		FindTokenImpl<Vec16s>(string, token, [&](unsigned int index) {
			tokens.Add(index);
			});
	}

	// --------------------------------------------------------------------------------------------------

	void FindToken(Stream<char> string, Stream<char> token, AdditionStream<Stream<char>> tokens) {
		FindTokenImpl<Vec32c>(string, token, [&](unsigned int index) {
			tokens.Add({ string.buffer + index, string.size - index });
			});
	}

	// --------------------------------------------------------------------------------------------------

	void FindToken(Stream<wchar_t> string, Stream<wchar_t> token, AdditionStream<Stream<wchar_t>> tokens) {
		FindTokenImpl<Vec16s>(string, token, [&](unsigned int index) {
			tokens.Add({ string.buffer + index, string.size - index });
			});
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
		size_t last_character_to_check = characters.size - token.size;
		size_t simd_count = GetSimdCount(last_character_to_check, first_character.size());

		// The scalar loop must be done before the 
		if constexpr (reverse) {
			// Use a scalar loop
			for (size_t index = simd_count; index <= last_character_to_check; index++) {
				const CharacterType* character = characters.buffer + last_character_to_check - index + simd_count;
				if (*character == token[0]) {
					if (memcmp(character, token.buffer, token.size * sizeof(CharacterType)) == 0) {
						return { character, characters.size - PointerDifference(character, characters.buffer) / sizeof(CharacterType) };
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
					return { character + found_match, characters.size - PointerDifference(character + found_match, characters.buffer) / sizeof(CharacterType) };
				}
			}

		}

		// Use a scalar loop
		if constexpr (!reverse) {
			for (size_t index = simd_count; index < characters.size - token.size + 1; index++) {
				const CharacterType* character = characters.buffer + index;
				if (*character == token[0]) {
					if (memcmp(character, token.buffer, token.size * sizeof(CharacterType)) == 0) {
						return { character, characters.size - PointerDifference(character, characters.buffer) / sizeof(CharacterType) };
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
		size_t index = SearchBytes(characters, token);
		if (index == -1) {
			return { nullptr, 0 };
		}
		return { characters.buffer + index, characters.size - index };
	}

	// --------------------------------------------------------------------------------------------------

	Stream<wchar_t> FindFirstCharacter(Stream<wchar_t> characters, wchar_t token) {
		size_t index = SearchBytes(characters, token);
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

	template<typename CharacterType>
	const CharacterType* FindMatchingParanthesisImpl(
		const CharacterType* start_character,
		const CharacterType* end_character,
		CharacterType opened_char,
		CharacterType closed_char,
		unsigned int opened_count
	) {
		const CharacterType* current_closed = start_character - 1;
		const CharacterType* current_opened = start_character - 1;
		bool initial_zero = opened_count == 0;

		auto iteration = [&]() {
			current_opened = current_closed;
			current_closed = FindFirstCharacter(Stream<CharacterType>(current_closed + 1, PointerDifference(end_character, current_closed + 1) / sizeof(CharacterType)), closed_char).buffer;
			if (current_closed == nullptr) {
				return;
			}

			while (current_opened < end_character) {
				current_opened = FindFirstCharacter(
					Stream<CharacterType>(current_opened + 1, PointerDifference(current_closed, current_opened + 1) / sizeof(CharacterType)),
					opened_char
				).buffer;
				if (current_opened != nullptr) {
					opened_count++;
				}
				else {
					break;
				}
			}
			opened_count--;
		};

		if (opened_count == 0) {
			iteration();
		}
		while (current_closed < end_character && opened_count > 0) {
			iteration();
		}
		return opened_count == 0 ? current_closed : nullptr;
	}

	const char* FindMatchingParenthesis(const char* start_character, const char* end_character, char opened_char, char closed_char, unsigned int opened_count)
	{
		return FindMatchingParanthesisImpl(start_character, end_character, opened_char, closed_char, opened_count);
	}

	// --------------------------------------------------------------------------------------------------

	const wchar_t* FindMatchingParenthesis(const wchar_t* start_character, const wchar_t* end_character, wchar_t opened_char, wchar_t closed_char, unsigned int opened_count)
	{
		return FindMatchingParanthesisImpl(start_character, end_character, opened_char, closed_char, opened_count);
	}

	// --------------------------------------------------------------------------------------------------

	Stream<char> FindMatchingParenthesis(Stream<char> range, char opened_char, char closed_char, unsigned int opened_count)
	{
		const char* opened_paren = FindMatchingParenthesis(range.buffer, range.buffer + range.size, opened_char, closed_char, opened_count);
		if (opened_paren == nullptr) {
			return { nullptr, 0 };
		}

		return { opened_paren, PointerDifference(range.buffer + range.size, opened_paren) };
	}

	// --------------------------------------------------------------------------------------------------

	Stream<wchar_t> FindMatchingParenthesis(Stream<wchar_t> range, wchar_t opened_char, wchar_t closed_char, unsigned int opened_count)
	{
		const wchar_t* opened_paren = FindMatchingParenthesis(range.buffer, range.buffer + range.size, opened_char, closed_char, opened_count);
		if (opened_paren == nullptr) {
			return { nullptr, 0 };
		}

		return { opened_paren, PointerDifference(range.buffer + range.size, opened_paren) };
	}

	// --------------------------------------------------------------------------------------------------

	Stream<char> FindDelimitedString(Stream<char> range, char opened_delimiter, char closed_delimiter, bool skip_whitespace)
	{
		Stream<char> first_delimiter = FindFirstCharacter(range, opened_delimiter);
		Stream<char> first_closed = FindFirstCharacter(range, closed_delimiter);

		first_delimiter.Advance();
		if (skip_whitespace) {
			first_delimiter = SkipWhitespace(first_delimiter);
		}

		Stream<char> substring = Stream<char>(first_delimiter.buffer, first_closed.buffer - first_delimiter.buffer);
		if (skip_whitespace) {
			substring = SkipWhitespace(substring);
		}
		return substring;
	}

	// --------------------------------------------------------------------------------------------------

	unsigned int FindString(const char* string, Stream<const char*> other)
	{
		Stream<char> stream_string = string;
		for (size_t index = 0; index < other.size; index++) {
			if (stream_string == other[index]) {
				return index;
			}
		}
		return -1;
	}

	// --------------------------------------------------------------------------------------------------

	unsigned int FindString(Stream<char> string, Stream<Stream<char>> other)
	{
		for (size_t index = 0; index < other.size; index++) {
			if (string == other[index]) {
				return index;
			}
		}
		return -1;
	}

	// --------------------------------------------------------------------------------------------------

	unsigned int FindString(const wchar_t* string, Stream<const wchar_t*> other) {
		Stream<wchar_t> stream_string = string;
		for (size_t index = 0; index < other.size; index++) {
			if (string == other[index]) {
				return index;
			}
		}
		return -1;
	}

	// --------------------------------------------------------------------------------------------------

	unsigned int FindString(Stream<wchar_t> string, Stream<Stream<wchar_t>> other) {
		for (size_t index = 0; index < other.size; index++) {
			if (string == other[index]) {
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
					current_string = *(CapacityStream<CharacterType>*)OffsetPointer(strings_buffer, index * string_byte_size + offset);
				}
				else {
					current_string = *(Stream<CharacterType>*)OffsetPointer(strings_buffer, index * string_byte_size + offset);
				}
				if (string == current_string) {
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

	void FromNumbersToStrings(Stream<unsigned int> values, CapacityStream<char>& storage, Stream<char>* strings)
	{
		for (size_t index = 0; index < values.size; index++) {
			strings[index].buffer = storage.buffer + storage.size;
			strings[index].size = ConvertIntToChars(storage, values[index]);
		}
	}

	// --------------------------------------------------------------------------------------------------

	void FromNumbersToStrings(size_t count, CapacityStream<char>& storage, Stream<char>* strings, size_t offset)
	{
		for (size_t index = 0; index < count; index++) {
			strings[index].buffer = storage.buffer + storage.size;
			strings[index].size = ConvertIntToChars(storage, index + offset);
		}
	}

	// --------------------------------------------------------------------------------------------------

	Stream<char> GetStringParameter(Stream<char> string)
	{
		Stream<char> opened_paranthesis = FindFirstCharacter(string, '(');
		if (opened_paranthesis.size > 0) {
			Stream<char> closed_paranthesis = FindFirstCharacter(opened_paranthesis, ')');
			if (closed_paranthesis.size > 0) {
				opened_paranthesis = SkipWhitespace(opened_paranthesis);
				closed_paranthesis = SkipWhitespace(closed_paranthesis, -1);
				return { opened_paranthesis.buffer + 1, PointerDifference(closed_paranthesis.buffer, opened_paranthesis.buffer + 1) / sizeof(char) };
			}
		}
		return { nullptr, 0 };
	}

	// --------------------------------------------------------------------------------------------------

	Stream<char> SkipCharacters(Stream<char> characters, char character, unsigned int count) {
		for (unsigned int index = 0; index < count; index++) {
			Stream<char> new_characters = FindFirstCharacter(characters, character);
			if (new_characters.size == 0) {
				return { nullptr, 0 };
			}

			characters = new_characters;
			characters.Advance();
		}

		return characters;
	}

	// --------------------------------------------------------------------------------------------------

	Stream<char> SkipTokens(Stream<char> characters, Stream<char> token, unsigned int count) {
		for (unsigned int index = 0; index < count; index++) {
			Stream<char> new_characters = FindFirstToken(characters, token);
			if (new_characters.size == 0) {
				return { nullptr, 0 };
			}

			characters = new_characters;
			characters.Advance(token.size);
		}

		return characters;
	}

	// --------------------------------------------------------------------------------------------------

	bool IsFloatingPointNumber(Stream<char> characters, bool skip_whitespace)
	{
		// Check for NaN and inf
		if (characters == "NaN" || characters == "INF" || characters == "-INF") {
			return true;
		}

		unsigned int dot_count = 0;
		size_t starting_index = characters[0] == '+' || characters[0] == '-';
		for (size_t index = starting_index; index < characters.size; index++) {
			if ((characters[index] < '0' || characters[index] > '9') && characters[index] != '.') {
				// For the floating point if it has a trailing f allow it
				if (index != characters.size - 1 || characters[index] != 'f') {
					if (!skip_whitespace || !IsWhitespaceEx(characters[index])) {
						return false;
					}
				}
			}

			dot_count += characters[index] == '.';
		}
		return dot_count <= 1;
	}

	// --------------------------------------------------------------------------------------------------

	bool IsIntegerNumber(Stream<char> characters, bool skip_whitespace)
	{
		size_t starting_index = characters[0] == '+' || characters[0] == '-';
		for (size_t index = starting_index; index < characters.size; index++) {
			// The comma is a separator between powers of 10^3
			// Include these as well
			if ((characters[index] < '0' || characters[index] > '9') && characters[index] != ',') {
				if (!skip_whitespace || !IsWhitespaceEx(characters[index])) {
					return false;
				}
			}
		}
		return true;
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	template<const char* (*SkipFunction)(const char* characters, int increment)>
	Stream<char> SkipStream(Stream<char> characters, int increment) {
		if (increment > 0) {
			const char* skipped = SkipFunction(characters.buffer, increment);
			return { skipped, PointerDifference(characters.buffer + characters.size, skipped) / sizeof(char) };
		}
		else {
			const char* last = characters.buffer + characters.size;
			const char* skipped = SkipFunction(last, increment);
			return { characters.buffer, PointerDifference(skipped, characters.buffer) / sizeof(char) };
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

	Stream<char> SkipTag(Stream<char> characters)
	{
		Stream<char> code_identifier = SkipWhitespace(characters);
		Stream<char> code_identifier_end = SkipCodeIdentifier(code_identifier);
		if (code_identifier_end.size > 0) {
			Stream<char> opened_parenthesis = SkipWhitespace(code_identifier_end);
			if (opened_parenthesis.size > 0) {
				if (opened_parenthesis.buffer[0] == '(') {
					Stream<char> closing_parenthesis = FindMatchingParenthesis(opened_parenthesis, '(', ')', 0);
					if (closing_parenthesis.size > 0) {
						return closing_parenthesis.AdvanceReturn();
					}
				}
				else {
					return opened_parenthesis;
				}
			}
		}
		return { nullptr, 0 };
	}

	// -----------------------------------------------------------------------------------------------------------------------------------

	Stream<char> SkipUntilCharacterReverse(const char* string, const char* bound, char character)
	{
		const char* initial_string = string;
		while (string >= bound && *string != character) {
			string--;
		}
		if (string >= bound) {
			return { string + 1, PointerDifference(initial_string, string) };
		}
		return { nullptr, 0 };
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
	static void ConvertDateToStringImplementation(Date date, Stream<CharacterType>& characters, ECS_FORMAT_DATE_FLAGS format_flags) {
		auto flag = [&](size_t integer) {
			CharacterType temp[256];
			Stream<CharacterType> temp_stream = Stream<CharacterType>(temp, 0);
			if (integer < 10) {
				temp_stream.Add(Character<CharacterType>('0'));
			}
			ConvertIntToChars(temp_stream, integer);

			characters.AddStream(temp_stream);
		};

		CharacterType colon_or_dash = (format_flags & ECS_FORMAT_DATE_COLON_INSTEAD_OF_DASH) == 0 ? Character<CharacterType>('-') : Character<CharacterType>(':');

		bool has_hour = false;
		if (HasFlag(format_flags, ECS_FORMAT_DATE_HOUR)) {
			flag(date.hour);
			has_hour = true;
		}

		bool has_minutes = false;
		if (HasFlag(format_flags, ECS_FORMAT_DATE_MINUTES)) {
			if (has_hour) {
				characters.Add(colon_or_dash);
			}
			has_minutes = true;
			flag(date.minute);
		}

		bool has_seconds = false;
		if (HasFlag(format_flags, ECS_FORMAT_DATE_SECONDS)) {
			if (has_minutes || has_hour) {
				characters.Add(colon_or_dash);
			}
			has_seconds = true;
			flag(date.seconds);
		}

		bool has_milliseconds = false;
		if (HasFlag(format_flags, ECS_FORMAT_DATE_MILLISECONDS)) {
			if (has_hour || has_minutes || has_seconds) {
				characters.Add(colon_or_dash);
			}
			has_milliseconds = true;
			flag(date.milliseconds);
		}

		bool has_hour_minutes_seconds_milliseconds = has_hour || has_minutes || has_seconds || has_milliseconds;
		bool has_space_been_written = false;

		bool has_day = false;
		if (HasFlag(format_flags, ECS_FORMAT_DATE_DAY)) {
			if (!has_space_been_written && has_hour_minutes_seconds_milliseconds) {
				characters.Add(Character<CharacterType>(' '));
				has_space_been_written = true;
			}
			has_day = true;
			flag(date.day);
		}

		bool has_month = false;
		if (HasFlag(format_flags, ECS_FORMAT_DATE_MONTH)) {
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

		if (HasFlag(format_flags, ECS_FORMAT_DATE_YEAR)) {
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

	void ConvertDateToString(Date date, Stream<char>& characters, ECS_FORMAT_DATE_FLAGS format_flags)
	{
		ConvertDateToStringImplementation(date, characters, format_flags);
	}

	// --------------------------------------------------------------------------------------------------

	void ConvertDateToString(Date date, CapacityStream<char>& characters, ECS_FORMAT_DATE_FLAGS format_flags)
	{
		Stream<char> stream(characters);
		ConvertDateToString(date, stream, format_flags);
		characters.size = stream.size;
		characters.AssertCapacity();
	}

	// --------------------------------------------------------------------------------------------------

	void ConvertDateToString(Date date, Stream<wchar_t>& characters, ECS_FORMAT_DATE_FLAGS format_flags) {
		ConvertDateToStringImplementation(date, characters, format_flags);
	}

	// --------------------------------------------------------------------------------------------------

	void ConvertDateToString(Date date, CapacityStream<wchar_t>& characters, ECS_FORMAT_DATE_FLAGS format_flags) {
		Stream<wchar_t> stream(characters);
		ConvertDateToString(date, stream, format_flags);
		characters.size = stream.size;
		characters.AssertCapacity();
	}

	// --------------------------------------------------------------------------------------------------

	size_t ConvertDateToStringMaxCharacterCount(ECS_FORMAT_DATE_FLAGS format_flags)
	{
		size_t count = 0;
		count += HasFlag(format_flags, ECS_FORMAT_DATE_MILLISECONDS) ? 4 : 0;
		count += HasFlag(format_flags, ECS_FORMAT_DATE_SECONDS) ? 3 : 0;
		count += HasFlag(format_flags, ECS_FORMAT_DATE_MINUTES) ? 3 : 0;
		count += HasFlag(format_flags, ECS_FORMAT_DATE_HOUR) ? 2 : 0;
		count += HasFlag(format_flags, ECS_FORMAT_DATE_DAY) ? 3 : 0;
		count += HasFlag(format_flags, ECS_FORMAT_DATE_MONTH) ? 3 : 0;
		count += HasFlag(format_flags, ECS_FORMAT_DATE_YEAR) ? 5 : 0;

		count += 3;
		return count;
	}

	// --------------------------------------------------------------------------------------------------

	template<typename CharacterType>
	static Date ConvertStringToDateImplementation(Stream<CharacterType> characters, ECS_FORMAT_DATE_FLAGS format_flags) {
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
		if (format_flags & ECS_FORMAT_DATE_HOUR) {
			date.hour = numbers[0];
			numbers_parsed++;
		}
		if (format_flags & ECS_FORMAT_DATE_MINUTES) {
			ECS_ASSERT(number_count > numbers_parsed);
			date.minute = numbers[numbers_parsed];
			numbers_parsed++;
		}
		if (format_flags & ECS_FORMAT_DATE_SECONDS) {
			ECS_ASSERT(number_count > numbers_parsed);
			date.seconds = numbers[numbers_parsed];
			numbers_parsed++;
		}
		if (format_flags & ECS_FORMAT_DATE_MILLISECONDS) {
			ECS_ASSERT(number_count > numbers_parsed);
			date.milliseconds = numbers[numbers_parsed];
			numbers_parsed++;
		}

		if (format_flags & ECS_FORMAT_DATE_DAY) {
			ECS_ASSERT(number_count > numbers_parsed);
			date.day = numbers[numbers_parsed];
			numbers_parsed++;
		}
		if (format_flags & ECS_FORMAT_DATE_MONTH) {
			ECS_ASSERT(number_count > numbers_parsed);
			date.month = numbers[numbers_parsed];
			numbers_parsed++;
		}
		if (format_flags & ECS_FORMAT_DATE_YEAR) {
			ECS_ASSERT(number_count > numbers_parsed);
			date.year = numbers[numbers_parsed];
			numbers_parsed++;
		}

		return date;
	}

	Date ConvertStringToDate(Stream<char> characters, ECS_FORMAT_DATE_FLAGS format_flags)
	{
		return ConvertStringToDateImplementation(characters, format_flags);
	}

	// --------------------------------------------------------------------------------------------------

	Date ConvertStringToDate(Stream<wchar_t> characters, ECS_FORMAT_DATE_FLAGS format_flags)
	{
		return ConvertStringToDateImplementation(characters, format_flags);
	}

	// --------------------------------------------------------------------------------------------------

	template<typename CharacterType, typename StreamType>
	static void ConvertByteSizeToStringImplementation(size_t byte_size, StreamType& characters) {
		if (byte_size < ECS_KB) {
			ConvertIntToChars(characters, byte_size);
			characters.Add(Character<CharacterType>(' '));
			characters.Add(Character<CharacterType>('b'));
			characters.Add(Character<CharacterType>('y'));
			characters.Add(Character<CharacterType>('t'));
			characters.Add(Character<CharacterType>('e'));
			characters.Add(Character<CharacterType>('s'));
		}
		else if (byte_size < ECS_MB) {
			float kilobytes = (float)byte_size / (float)ECS_KB;
			ConvertFloatToChars(characters, kilobytes, 2);
			characters.Add(Character<CharacterType>(' '));
			characters.Add(Character<CharacterType>('K'));
			characters.Add(Character<CharacterType>('B'));
		}
		else if (byte_size < ECS_GB) {
			float mb = (float)byte_size / (float)ECS_MB;
			ConvertFloatToChars(characters, mb, 2);
			characters.Add(Character<CharacterType>(' '));
			characters.Add(Character<CharacterType>('M'));
			characters.Add(Character<CharacterType>('B'));
		}
		else if (byte_size < ECS_TB) {
			float gb = (float)byte_size / (float)ECS_GB;
			ConvertFloatToChars(characters, gb, 2);
			characters.Add(Character<CharacterType>(' '));
			characters.Add(Character<CharacterType>('G'));
			characters.Add(Character<CharacterType>('B'));
		}
		else if (byte_size < ECS_TB * ECS_KB) {
			float tb = (float)byte_size / (float)ECS_TB;
			ConvertFloatToChars(characters, tb, 2);
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

	template<typename CharacterType>
	static void ReplaceCharacterImpl(Stream<CharacterType> string, CharacterType token_to_be_replaced, CharacterType replacement) {
		auto character = FindFirstCharacter(string, token_to_be_replaced);
		while (character.size > 0) {
			character[0] = replacement;
			character.buffer += 1;
			character.size -= 1;

			character = FindFirstCharacter(character, token_to_be_replaced);
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
	static Stream<CharacterType> ReplaceTokenImpl(Stream<CharacterType> string, Stream<CharacterType> token, Stream<CharacterType> replacement, AllocatorPolymorphic allocator) {
		Stream<CharacterType> result;

		Stream<CharacterType> original_string = string;
		// Determine the size to be allocated
		size_t size_to_allocate = 0;
		Stream<CharacterType> current_token = FindFirstToken(string, token);
		while (current_token.size > 0) {
			size_to_allocate += current_token.buffer - string.buffer;
			size_to_allocate += replacement.size;
			current_token.Advance(token.size);
			string = current_token;
			current_token = FindFirstToken(string, token);
		}

		size_to_allocate += string.size;

		result.Initialize(allocator, size_to_allocate);
		result.size = 0;
		string = original_string;
		current_token = FindFirstToken(string, token);

		while (current_token.size > 0) {
			Stream<CharacterType> first_part;
			first_part.buffer = string.buffer;
			first_part.size = current_token.buffer - string.buffer;
			result.AddStream(first_part);
			result.AddStream(replacement);

			current_token.Advance(token.size);
			string = current_token;
			current_token = FindFirstToken(string, token);
		}

		// Copy the last part
		result.AddStream(string);
		return result;
	}

	Stream<char> ReplaceToken(Stream<char> string, Stream<char> token, Stream<char> replacement, AllocatorPolymorphic allocator) {
		return ReplaceTokenImpl(string, token, replacement, allocator);
	}

	Stream<wchar_t> ReplaceToken(Stream<wchar_t> string, Stream<wchar_t> token, Stream<wchar_t> replacement, AllocatorPolymorphic allocator) {
		return ReplaceTokenImpl(string, token, replacement, allocator);
	}

	// --------------------------------------------------------------------------------------------------

	template<typename CharacterType>
	static Stream<CharacterType> ReplaceCharacterImpl(CapacityStream<CharacterType>& string, Stream<CharacterType> token, Stream<CharacterType> replacement) {
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
		ResizableStream<unsigned int> token_appereances(&stack_allocator, ECS_KB);
		AdditionStream<unsigned int> token_appereances_addition = &token_appereances;
		FindToken(string.ToStream(), token, token_appereances_addition);

		unsigned int token_difference = replacement.size - token.size;
		unsigned int total_size = string.size + token_difference * token_appereances.size;
		ECS_ASSERT(total_size <= string.capacity);

		// Copy the parts of the string that are not being replaced first such that we don't lose the data
		if (token_difference != 0) {
			unsigned int token_count = token_appereances.size;
			for (unsigned int index = token_count; index > 0; index--) {
				unsigned int current_index = token_appereances[index - 1] + token.size;
				unsigned int new_index = token_appereances[index - 1] + token.size + token_difference * index;
				if (current_index != new_index) {
					unsigned int copy_count = index != token_count ? token_appereances[index] - current_index : string.size - current_index;
					memmove(string.buffer + new_index, string.buffer + current_index, copy_count * sizeof(CharacterType));
				}
			}
		}

		for (unsigned int index = 0; index < token_appereances.size; index++) {
			// Replace the tokens now
			unsigned int write_index = token_appereances[index] + index * token_difference;
			string.CopySlice(write_index, replacement);
		}

		string.size = total_size;
		return string;
	}

	Stream<char> ReplaceToken(CapacityStream<char>& string, Stream<char> token, Stream<char> replacement)
	{
		return ReplaceCharacterImpl<char>(string, token, replacement);
	}

	Stream<wchar_t> ReplaceToken(CapacityStream<wchar_t>& string, Stream<wchar_t> token, Stream<wchar_t> replacement)
	{
		return ReplaceCharacterImpl<wchar_t>(string, token, replacement);
	}

	// --------------------------------------------------------------------------------------------------

	template<typename CharacterType>
	static void ReplaceOccurencesImpl(CapacityStream<CharacterType>& string, Stream<ReplaceOccurence<CharacterType>> occurences, CapacityStream<CharacterType>* output_string) {
		// Null terminate the string
		CharacterType previous_character = string[string.size];
		string[string.size] = Character<CharacterType>('\0');

		// Get the list of occurences for all types
		ECS_STACK_CAPACITY_STREAM(uint2, replacement_positions, 512);
		ECS_STACK_ADDITION_STREAM(unsigned int, current_occurence_positions, 1024);

		size_t insert_index = 0;
		auto insert_occurences = [&](uint2 occurence) {
			for (; insert_index < replacement_positions.size && replacement_positions[insert_index].x < occurence.x; insert_index++) {}
			if (insert_index == replacement_positions.size) {
				replacement_positions.AddAssert(occurence);
			}
			else {
				replacement_positions.Insert(insert_index, occurence);
			}
		};

		for (size_t index = 0; index < occurences.size; index++) {
			FindToken(string, occurences[index].string, current_occurence_positions_addition);
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
	static void SplitStringImpl(Stream<CharacterType> string, CharacterType delimiter, CapacityStream<Stream<CharacterType>>& splits)
	{
		Stream<CharacterType> found_delimiter = FindFirstCharacter(string, delimiter);
		while (found_delimiter.size > 0) {
			splits.AddAssert({ string.buffer, string.size - found_delimiter.size });
			found_delimiter.Advance();
			string = found_delimiter;
			found_delimiter = FindFirstCharacter(string, delimiter);
		}

		if (string.size > 0) {
			splits.AddAssert(string);
		}
	}

	template<typename CharacterType>
	static void SplitStringImpl(Stream<CharacterType> string, Stream<CharacterType> delimiter, CapacityStream<Stream<CharacterType>>& splits)
	{
		Stream<CharacterType> found_delimiter = FindFirstToken(string, delimiter);
		while (found_delimiter.size > 0) {
			splits.AddAssert({ string.buffer, string.size - found_delimiter.size });
			found_delimiter.Advance(delimiter.size);
			string = found_delimiter;
			found_delimiter = FindFirstToken(string, delimiter);
		}

		if (string.size > 0) {
			splits.AddAssert(string);
		}
	}

	void SplitString(Stream<char> string, char delimiter, CapacityStream<Stream<char>>& splits)
	{
		SplitStringImpl<char>(string, delimiter, splits);
	}

	// --------------------------------------------------------------------------------------------------

	void SplitString(Stream<wchar_t> string, wchar_t delimiter, CapacityStream<Stream<wchar_t>>& splits)
	{
		SplitStringImpl<wchar_t>(string, delimiter, splits);
	}

	// --------------------------------------------------------------------------------------------------

	void SplitString(Stream<char> string, Stream<char> delimiter, CapacityStream<Stream<char>>& splits)
	{
		SplitStringImpl<char>(string, delimiter, splits);
	}

	// --------------------------------------------------------------------------------------------------

	void SplitString(Stream<wchar_t> string, Stream<wchar_t> delimiter, CapacityStream<Stream<wchar_t>>& splits)
	{
		SplitStringImpl<wchar_t>(string, delimiter, splits);
	}

	// --------------------------------------------------------------------------------------------------

	template<typename CharacterType>
	Stream<CharacterType> IsolateStringImpl(Stream<CharacterType> string, Stream<CharacterType> token, Stream<CharacterType> delimiter) {
		Stream<CharacterType> existing_string = FindFirstToken(string, token);
		if (existing_string.size > 0) {
			Stream<CharacterType> next_delimiter = FindFirstToken(existing_string, delimiter);
			if (next_delimiter.size > 0) {
				return Stream<CharacterType>(
					existing_string.buffer,
					PointerDifference(next_delimiter.buffer, existing_string.buffer) / sizeof(CharacterType)
					);
			}
			return existing_string;
		}
		return {};
	}

	Stream<char> IsolateString(Stream<char> string, Stream<char> token, Stream<char> delimiter)
	{
		return IsolateStringImpl(string, token, delimiter);
	}

	Stream<wchar_t> IsolateString(Stream<wchar_t> string, Stream<wchar_t> token, Stream<wchar_t> delimiter)
	{
		return IsolateStringImpl(string, token, delimiter);
	}

	// --------------------------------------------------------------------------------------------------

	template<typename CharacterType>
	Stream<CharacterType> StringsPrefixImpl(Stream<Stream<CharacterType>> strings) {
		if (strings.size == 0) {
			return {};
		}

		size_t smallest_string_size = ULLONG_MAX;
		for (size_t index = 0; index < strings.size; index++) {
			smallest_string_size = std::min(smallest_string_size, strings[index].size);
		}

		for (size_t prefix_size = smallest_string_size; prefix_size > 0; prefix_size--) {
			bool all_have_it = true;
			Stream<CharacterType> prefix = { strings[0].buffer, prefix_size };
			for (size_t subindex = 0; subindex < strings.size && all_have_it; subindex++) {
				if (!strings[subindex].StartsWith(prefix)) {
					all_have_it = false;
				}
			}

			if (all_have_it) {
				return prefix;
			}
		}

		return {};
	}

	Stream<char> StringsPrefix(Stream<Stream<char>> strings)
	{
		return StringsPrefixImpl(strings);
	}

	Stream<wchar_t> StringsPrefix(Stream<Stream<wchar_t>> strings)
	{
		return StringsPrefixImpl(strings);
	}

	// --------------------------------------------------------------------------------------------------

	template<typename FloatingPoint, bool strict_parsing, typename CharacterType>
	FloatingPoint ConvertCharactersToFloatingPoint(Stream<CharacterType> stream, bool& success) {
		// Check for the special case of nan and inf
		if (stream.size == 3 || stream.size == 4) {
			// Check for nan and inf
			if constexpr (std::is_same_v<CharacterType, char>) {
				if (stream == "NaN") {
					return std::numeric_limits<FloatingPoint>::quiet_NaN();
				}
				else if (stream == "INF") {
					return std::numeric_limits<FloatingPoint>::infinity();
				}
				else if (stream == "-INF") {
					return -std::numeric_limits<FloatingPoint>::infinity();
				}
			}
			else {
				if (stream == L"NaN") {
					return std::numeric_limits<FloatingPoint>::quiet_NaN();
				}
				else if (stream == L"INF") {
					return std::numeric_limits<FloatingPoint>::infinity();
				}
				else if (stream == L"-INF") {
					return -std::numeric_limits<FloatingPoint>::infinity();
				}
			}
		}

		size_t starting_index = stream[0] == Character<CharacterType>('-') || stream[0] == Character<CharacterType>('+') ? 1 : 0;

		Stream<CharacterType> dot = FindFirstCharacter(stream, Character<CharacterType>('.'));
		size_t dot_index = dot.size > 0 ? (size_t)(dot.buffer - stream.buffer) : stream.size;

		if (dot_index < stream.size) {
			int64_t integral_part = ConvertCharactersToIntImpl<int64_t, CharacterType, strict_parsing>(
				Stream<CharacterType>(stream.buffer + starting_index, dot_index - starting_index), 
				success
			);
			if constexpr (strict_parsing) {
				if (!success) {
					return FloatingPoint(0);
				}
			}

			size_t fractional_digit_count = 0;
			int64_t fractional_part = ConvertCharactersToIntImpl<int64_t, CharacterType, strict_parsing>(
				Stream<CharacterType>(stream.buffer + dot_index + 1, stream.size - dot_index - 1), 
				fractional_digit_count, 
				success
			);
			if constexpr (strict_parsing) {
				if (!success) {
					return FloatingPoint(0);
				}
			}

			// Use doubles since they allow better precision for large values
			double integral_float = (double)(integral_part);
			double fractional_float = (double)(fractional_part);

			// Reversed in order to speed up calculations
			double fractional_power = 0.1;
			if (fractional_digit_count > 1) {
				fractional_power = pow(fractional_power, fractional_digit_count);
			}
			fractional_float *= fractional_power;
			FloatingPoint value = (FloatingPoint)(integral_float + fractional_float);
			if (stream[0] == Character<CharacterType>('-')) {
				value = -value;
			}
			return value;
		}
		else {
			FloatingPoint value = 0;
			if (stream.size > 0) {
				int64_t integer = ConvertCharactersToIntImpl<int64_t, CharacterType, strict_parsing>(
					Stream<CharacterType>(stream.buffer + starting_index, stream.size - starting_index),
					success
				);
				if constexpr (strict_parsing) {
					if (!success) {
						return value;
					}
				}

				value = (FloatingPoint)(integer);
				if (stream[0] == Character<CharacterType>('-')) {
					value = -value;
				}
			}
			return value;
		}
	}

	// ----------------------------------------------------------------------------------------------------------

	float ConvertCharactersToFloat(Stream<char> characters) {
		bool dummy;
		return ConvertCharactersToFloatingPoint<float, false>(characters, dummy);
	}

	float ConvertCharactersToFloat(Stream<wchar_t> characters) {
		bool dummy;
		return ConvertCharactersToFloatingPoint<float, false>(characters, dummy);
	}

	// ----------------------------------------------------------------------------------------------------------

	double ConvertCharactersToDouble(Stream<char> characters) {
		bool dummy;
		return ConvertCharactersToFloatingPoint<double, false>(characters, dummy);
	}

	double ConvertCharactersToDouble(Stream<wchar_t> characters) {
		bool dummy;
		return ConvertCharactersToFloatingPoint<double, false>(characters, dummy);
	}

	// ----------------------------------------------------------------------------------------------------------

	int64_t ConvertCharactersToInt(Stream<char> stream) {
		bool dummy;
		return ConvertCharactersToIntImpl<int64_t, char, false>(stream, dummy);
	}

	int64_t ConvertCharactersToInt(Stream<wchar_t> stream) {
		bool dummy;
		return ConvertCharactersToIntImpl<int64_t, wchar_t, false>(stream, dummy);
	}

	// ----------------------------------------------------------------------------------------------------------

	int64_t ConvertCharactersToInt(Stream<char> stream, size_t& digit_count) {
		bool dummy;
		return ConvertCharactersToIntImpl<int64_t, char, false>(stream, digit_count, dummy);
	}

	int64_t ConvertCharactersToInt(Stream<wchar_t> stream, size_t& digit_count) {
		bool dummy;
		return ConvertCharactersToIntImpl<int64_t, wchar_t, false>(stream, digit_count, dummy);
	}

	// ----------------------------------------------------------------------------------------------------------

#define CONVERT_TYPE_TO_CHARS_FROM_ASCII_TO_WIDE(convert_function, chars, ...) char characters[512]; \
		Stream<char> ascii(characters, 0); \
\
		size_t character_count = convert_function(ascii, __VA_ARGS__); \
		ConvertASCIIToWide(chars.buffer + chars.size, ascii, character_count + 1); \
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

	size_t FindWhitespaceCharactersCount(Stream<char> string, char separator_token, CapacityStream<unsigned int>* stack_buffer)
	{
		size_t count = 0;

		Stream<char> token_stream = FindFirstCharacter(string, separator_token);
		Stream<char> new_line_stream = FindFirstCharacter(string, '\n');
		if (stack_buffer == nullptr) {
			while (token_stream.buffer != nullptr && new_line_stream.buffer != nullptr) {
				count += 2;

				token_stream.buffer += 1;
				token_stream.size -= 1;

				new_line_stream.buffer += 1;
				new_line_stream.size -= 1;

				token_stream = FindFirstCharacter(token_stream, separator_token);
				new_line_stream = FindFirstCharacter(new_line_stream, '\n');
			}

			while (token_stream.buffer != nullptr) {
				count++;
				token_stream.buffer += 1;
				token_stream.size -= 1;
				token_stream = FindFirstCharacter(token_stream, separator_token);
			}

			while (new_line_stream.buffer != nullptr) {
				count++;
				new_line_stream.buffer += 1;
				new_line_stream.size -= 1;
				new_line_stream = FindFirstCharacter(new_line_stream, '\n');
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

						token_stream = FindFirstCharacter(token_stream, separator_token);
					}
					else {
						stack_buffer->Add(new_line_difference);

						new_line_stream.buffer += 1;
						new_line_stream.size -= 1;

						new_line_stream = FindFirstCharacter(new_line_stream, '\n');
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
				token_stream = FindFirstCharacter(token_stream, separator_token);
			}

			while (new_line_stream.buffer != nullptr) {
				count++;

				if (stack_buffer->size < stack_buffer->capacity) {
					stack_buffer->Add(new_line_stream.buffer - string.buffer);
				}

				new_line_stream.buffer += 1;
				new_line_stream.size -= 1;
				new_line_stream = FindFirstCharacter(new_line_stream, '\n');
			}
		}
		return count;
	}

	// ----------------------------------------------------------------------------------------------------------

	// it searches for spaces and next line characters
	template<typename WordStream>
	void FindWhitespaceCharacters(WordStream& spaces, Stream<char> string, char separator_token) {
		size_t position = 0;

		Stream<char> token_stream = FindFirstCharacter(string, separator_token);
		Stream<char> new_line_stream = FindFirstCharacter(string, '\n');

		while (token_stream.buffer != nullptr && new_line_stream.buffer != nullptr) {
			unsigned int token_difference = token_stream.buffer - string.buffer;
			unsigned int new_line_difference = new_line_stream.buffer - string.buffer;

			if (token_difference < new_line_difference) {
				spaces.Add(token_difference);

				token_stream.buffer += 1;
				token_stream.size -= 1;

				token_stream = FindFirstCharacter(token_stream, separator_token);
			}
			else {
				spaces.Add(new_line_difference);

				new_line_stream.buffer += 1;
				new_line_stream.size -= 1;

				new_line_stream = FindFirstCharacter(new_line_stream, '\n');
			}
		}

		while (token_stream.buffer != nullptr) {
			spaces.Add(token_stream.buffer - string.buffer);

			token_stream.buffer += 1;
			token_stream.size -= 1;
			token_stream = FindFirstCharacter(token_stream, separator_token);
		}

		while (new_line_stream.buffer != nullptr) {
			spaces.Add(new_line_stream.buffer - string.buffer);

			new_line_stream.buffer += 1;
			new_line_stream.size -= 1;
			new_line_stream = FindFirstCharacter(new_line_stream, '\n');
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
			token_positions[index] = FindFirstCharacter(sentence, tokens[index]);
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
			token_positions[index_of_min] = FindFirstCharacter(token_positions[index_of_min], token_mapping[index_of_min]);
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
		Stream<char> token_stream = FindFirstCharacter(sentence, token);

		size_t starting_position = 0;
		while (token_stream.buffer != nullptr) {
			size_t current_position = token_stream.buffer - sentence.buffer;
			if (current_position > starting_position + 1) {
				words.Add(uint2((unsigned int)(token_stream.buffer - sentence.buffer), (unsigned int)(current_position - starting_position - 1)));
			}
			starting_position = current_position + 1;

			token_stream.buffer += 1;
			token_stream.size -= 1;
			token_stream = FindFirstCharacter(token_stream, token);
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
		// Redirect to the double version since it will allow for better precision
		return ConvertDoubleToChars(chars, (double)value, precision);
	}

	ECS_TEMPLATE_FUNCTION_4_BEFORE(size_t, ConvertFloatToChars, Stream<char>&, CapacityStream<char>&, Stream<wchar_t>&, CapacityStream<wchar_t>&, float, size_t);

	// ----------------------------------------------------------------------------------------------------------

	template<typename StringStream>
	size_t ConvertDoubleToChars(StringStream& chars, double value, size_t precision) {
		auto add = [&](auto add_string) {
			if constexpr (std::is_same_v<Stream<char>, StringStream> || std::is_same_v<Stream<wchar_t>, StringStream>) {
				chars.AddStream(add_string);
			}
			else {
				chars.AddStreamAssert(add_string);
			}
		};

		if constexpr (std::is_same_v<typename StringStream::T, char>) {
			// If the value is NaN or infinity, handle it separately
			if (isnan(value)) {
				add("NaN");
				return 3;
			}
			else if (value == std::numeric_limits<double>::infinity()) {
				add("INF");
				return 3;
			}
			else if (value == -std::numeric_limits<double>::infinity()) {
				add("-INF");
				return 4;
			}
		}
		else {
			// If the value is NaN or infinity, handle it separately
			if (isnan(value)) {
				add(L"NaN");
				return 3;
			}
			else if (value == std::numeric_limits<double>::infinity()) {
				add(L"INF");
				return 3;
			}
			else if (value == -std::numeric_limits<double>::infinity()) {
				add(L"-INF");
				return 4;
			}
		}

		ECS_ASSERT(precision < 16);
		double power_multiply = CalculateDoublePrecisionPower(precision);
		int64_t rounded_int;

		double new_value = round(value * power_multiply);
		rounded_int = static_cast<int64_t>(new_value);

		return ConvertFloatingPointIntegerToChars<StringStream>(chars, rounded_int, precision);
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

	char ConvertCharactersToBool(Stream<wchar_t> characters) {
		return ConvertCharactersToBoolImpl(characters);
	}

	// ----------------------------------------------------------------------------------------------------------

	template<bool strict_parsing, typename CharacterType>
	void* ConvertCharactersToPointerImpl(Stream<CharacterType> characters, bool& success)
	{
		Stream<CharacterType> prefix;
		if constexpr (std::is_same_v<CharacterType, char>) {
			prefix = "0x";
		}
		else {
			prefix = L"0x";
		}

		if constexpr (!strict_parsing) {
			Stream<CharacterType> prefix_in_string = FindFirstToken(characters, prefix);
			if (prefix_in_string.size > 0) {
				// Start from the prefix
				characters = characters.StartDifference(prefix_in_string);
				// Skip the prefix
				characters.Advance(2);
			}
		}
		else {
			if (characters[0] < Character<CharacterType>('0') || characters[0] > Character<CharacterType>('9')) {
				success = false;
				return nullptr;
			}

			if ((characters[1] < Character<CharacterType>('0') || characters[1] > Character<CharacterType>('9')) && characters[1] != Character<CharacterType>('x')) {
				success = false;
				return nullptr;
			}
			else if (characters[1] == Character<CharacterType>('x')) {
				characters.Advance(2);
			}
		}

		// Determine all the hex characters suchn that we can start in reverse order to build the value
		size_t index = 0;
		while (index < characters.size && IsHexChar(characters[index])) {
			index++;
		}
		if constexpr (strict_parsing) {
			if (index < characters.size) {
				success = false;
				return nullptr;
			}
		}

		if (index > 16) {
			// Too many digits - they do not fit into a 64 bit value
			success = false;
			return nullptr;
		}

		size_t current_shift_value = 0;
		size_t pointer_value = 0;
		size_t current_hex_value = 0;
		for (int64_t subindex = (int64_t)index - 1; subindex >= 0; subindex--) {
			if (IsNumberCharacter(characters[subindex])) {
				current_hex_value = characters[subindex] - Character<CharacterType>('0');
			}
			else if (characters[subindex] >= Character<CharacterType>('a') && characters[subindex] <= Character<CharacterType>('f')) {
				current_hex_value = characters[subindex] - Character<CharacterType>('a') + 10;
			}
			else {
				current_hex_value = characters[subindex] - Character<CharacterType>('A') + 10;
			}

			current_hex_value <<= current_shift_value;
			pointer_value |= current_hex_value;
			current_shift_value += 4;
		}

		if constexpr (strict_parsing) {
			success = true;
		}
		return (void*)pointer_value;
	}

	// ----------------------------------------------------------------------------------------------------------

	void* ConvertCharactersToPointer(Stream<char> characters)
	{
		bool dummy;
		return ConvertCharactersToPointerImpl<false>(characters, dummy);
	}

	void* ConvertCharactersToPointer(Stream<wchar_t> characters) 
	{
		bool dummy;
		return ConvertCharactersToPointerImpl<false>(characters, dummy);
	}

	// ----------------------------------------------------------------------------------------------------------

	int64_t ConvertCharactersToIntStrict(Stream<char> string, bool& success)
	{
		return ConvertCharactersToIntImpl<int64_t, char, true>(string, success);
	}

	// ----------------------------------------------------------------------------------------------------------

	float ConvertCharactersToFloatStrict(Stream<char> string, bool& success)
	{
		return ConvertCharactersToFloatingPoint<float, true>(string, success);
	}

	// ----------------------------------------------------------------------------------------------------------

	double ConvertCharactersToDoubleStrict(Stream<char> string, bool& success)
	{
		return ConvertCharactersToFloatingPoint<double, true>(string, success);
	}

	// ----------------------------------------------------------------------------------------------------------

	void* ConvertCharactersToPointerStrict(Stream<char> string, bool& success)
	{
		return ConvertCharactersToPointerImpl<true>(string, success);
	}

	// ----------------------------------------------------------------------------------------------------------

	bool ConvertCharactersToBoolStrict(Stream<char> string, bool& success)
	{
		char value = ConvertCharactersToBool(string);
		if (value == -1) {
			success = false;
			return false;
		}
		else {
			success = true;
			return value == 0 ? false : true;
		}
	}

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


}