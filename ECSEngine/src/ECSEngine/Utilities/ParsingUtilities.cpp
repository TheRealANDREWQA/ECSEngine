#include "ecspch.h"
#include "ParsingUtilities.h"
#include "StringUtilities.h"

namespace ECSEngine {

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

		Stream<CharacterType> ext_delimiter = FindFirstCharacter(*characters, Character<CharacterType>('}'));
		if (ext_delimiter.size > 0) {
			Stream<CharacterType> parse_range = { characters->buffer, PointerDifference(ext_delimiter.buffer, characters->buffer) / sizeof(CharacterType) };
			// Multiple values
			Stream<CharacterType> delimiter = FindFirstCharacter(parse_range, internal_delimiter);

			double* double_array = (double*)&result;
			size_t double_array_index = 0;
			while (delimiter.size > 0 && double_array_index < 4) {
				Stream<CharacterType> current_range = { parse_range.buffer, PointerDifference(delimiter.buffer, parse_range.buffer) / sizeof(CharacterType) };
				double_array[double_array_index++] = ConvertCharactersToDouble(current_range);

				delimiter.buffer++;
				delimiter.size--;
				Stream<CharacterType> new_delimiter = FindFirstCharacter(delimiter, internal_delimiter);
				if (new_delimiter.size > 0) {
					parse_range = { delimiter.buffer, PointerDifference(new_delimiter.buffer, delimiter.buffer) / sizeof(CharacterType) };
					delimiter = new_delimiter;
				}
				else {
					parse_range = delimiter;
					delimiter.size = 0;
				}
			}

			if (double_array_index < 4) {
				// Try to parse a last value
				double_array[double_array_index] = ConvertCharactersToDouble(parse_range);
			}
			*characters = ext_delimiter;
			Stream<CharacterType> new_characters = FindFirstCharacter(*characters, external_delimiter);
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
				const CharacterType* starting_value = SkipWhitespace(characters->buffer);
				const CharacterType* delimiter = FindFirstCharacter(*characters, external_delimiter).buffer;
				if (delimiter == nullptr) {
					result.x = ConvertCharactersToDouble(*characters);
					characters->size = 0;
					return result;
				}
				else {
					// It has a delimiter
					const CharacterType* ending_value = SkipCodeIdentifier(delimiter, -1);
					Stream<CharacterType> compare_characters = { starting_value, PointerDifference(ending_value, starting_value) / sizeof(wchar_t) };
					if (compare_characters == ignore_tag) {
						*characters = delimiter;
						characters->Advance();
						return double4(DBL_MAX, DBL_MAX, DBL_MAX, DBL_MAX);
					}
					else {
						// Advance the whole characters - unknown sequence
						characters->Advance(characters->size);
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
			values.AddAssert(ParseFloat(&characters, delimiter));
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
			values.AddAssert(ParseDouble(&characters, delimiter));
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
			characters = SkipWhitespaceEx(characters);
		}
		else {
			const CharacterType* skipped = SkipWhitespace(characters.buffer);
			if (skipped < characters.buffer + characters.size) {
				characters.size -= PointerDifference(skipped, characters.buffer) / sizeof(CharacterType);
				characters.buffer = (CharacterType*)skipped;
			}
		}

		while (characters.size > 0) {
			values.AddAssert(ParseBool(&characters, delimiter));
			if (characters.size > 0) {
				characters.Advance();
			}

			if constexpr (std::is_same_v<CharacterType, char>) {
				characters = SkipWhitespaceEx(characters);
			}
			else {
				const CharacterType* skipped = SkipWhitespace(characters.buffer);
				if (skipped < characters.buffer + characters.size) {
					characters.size -= PointerDifference(skipped, characters.buffer) / sizeof(CharacterType);
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
			values.AddAssert(ParseDouble4(&characters, external_delimiter, internal_delimiter, ignore_tag));
			if (characters.size > 0) {
				characters.Advance();
			}
		}
	}

	template ECSENGINE_API void ParseDouble4s(Stream<char>, CapacityStream<double4>&, char, char, Stream<char>);
	template ECSENGINE_API void ParseDouble4s(Stream<wchar_t>, CapacityStream<double4>&, wchar_t, wchar_t, Stream<wchar_t>);

	// ----------------------------------------------------------------------------------------------------------

	bool ParseTokensFromFormat(Stream<char> string, Stream<char> format, CapacityStream<Stream<char>>* tokens)
	{
		ECS_STACK_CAPACITY_STREAM(unsigned int, token_indices, 512);
		AdditionStream<unsigned int> addition_stream = &token_indices;
		FindToken(format, ECS_FORMAT_SPECIFIER, addition_stream);

		size_t specifier_size = strlen(ECS_FORMAT_SPECIFIER);

		unsigned int format_starting_index = 0;
		unsigned int string_starting_index = 0;
		for (unsigned int index = 0; index < token_indices.size; index++) {
			// Try to match the previous part before the token
			Stream<char> format_previous_part = { format.buffer + format_starting_index, token_indices[index] - format_starting_index };
			Stream<char> string_previous_part = { string.buffer + string_starting_index, format_previous_part.size };
			if (format_previous_part == string_previous_part) {
				// Add the token up to the next character in the format
				unsigned int next_character_index = token_indices[index] + specifier_size;
				Stream<char> parse_range = { string.buffer + string_starting_index + format_previous_part.size, string.size - string_starting_index - format_previous_part.size };
				if (next_character_index < format.size) {
					char next_character = format[token_indices[index] + specifier_size];
					Stream<char> next_character_in_range = FindFirstCharacter(parse_range, next_character);
					if (next_character_in_range.size > 0) {
						// We found a next character - take the token from the start of the parse range up to it
						Stream<char> token = parse_range.StartDifference(next_character_in_range);
						tokens->AddAssert(token);

						// Update the starting indices
						format_starting_index = next_character_index;
						string_starting_index = token.buffer - string.buffer + token.size;
					}
					else {
						// Returns since the parsing did not match
						return false;
					}
				}
				else {
					// This specifier is exactly at the end
					// Add the token from the string up to the end of the string
					tokens->AddAssert(parse_range);
				}
			}
			else {
				// The parsing failed since the format is not matched
				return false;
			}
		}

		return true;
	}

	// ----------------------------------------------------------------------------------------------------------

}