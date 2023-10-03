#pragma once
#include "../Core.h"
#include "BasicTypes.h"
#include "PointerUtilities.h"

namespace ECSEngine {

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

		Stream<CharacterType> parse_range = { initial_buffer, PointerDifference(characters->buffer, initial_buffer) / sizeof(CharacterType) };
		return functor(parse_range);
	}

	// It will advance the pointer such that it will be at the delimiter after the value
	template<typename Integer, typename CharacterType>
	Integer ParseInteger(Stream<CharacterType>* characters, CharacterType delimiter) {
		return ParseType<CharacterType, Integer>(characters, delimiter, [](Stream<CharacterType> stream_characters) {
			return (Integer)ConvertCharactersToInt(stream_characters);
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
			if (characters[0] == delimiter) {
				characters.Advance();
				if (characters.size > 0) {
					values.AddAssert(ParseInteger<Integer>(&characters, delimiter));
				}
			}
			else {
				values.AddAssert(ParseInteger<Integer>(&characters, delimiter));
			}
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

}