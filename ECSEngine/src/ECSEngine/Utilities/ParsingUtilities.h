#pragma once
#include "../Core.h"
#include "BasicTypes.h"
#include "PointerUtilities.h"
#include "StringUtilities.h"

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

	// Based on a format where {#} represents a token to be parsed, it will add them to the capacity stream
	// Returns true if it could match all the tokens, else false
	ECSENGINE_API bool ParseTokensFromFormat(Stream<char> string, Stream<char> format, CapacityStream<Stream<char>>* tokens);

	// It will try to transform the token according to the value given
	// Returns true if the token matches the value type, else false
	template<typename Value>
	bool ParseValueFromToken(Stream<char> token, Value* value) {
		// Include the Stream<char> as well - this function can consist the basis for other functions
		// That might need Stream<char>
		if constexpr (std::is_same_v<std::remove_const_t<Value>, Stream<char>>) {
			*value = token;
			return true;
		}
		else if constexpr (std::is_integral_v<std::remove_const_t<Value>>) {
			// Try to parse as integer
			bool success = false;
			*value = ConvertCharactersToIntStrict(token, success);
			return success;
		}
		else if constexpr (std::is_same_v<std::remove_const_t<Value>, float>) {
			bool success = false;
			*value = ConvertCharactersToFloatStrict(token, success);
			return success;
		}
		else if constexpr (std::is_same_v<std::remove_const_t<Value>, double>) {
			bool success = false;
			*value = ConvertCharactersToDoubleStrict(token, success);
			return success;
		}
		else if constexpr (std::is_pointer_v<std::remove_const_t<Value>>) {
			bool success = false;
			*value = (Value)ConvertCharactersToPointerStrict(token, success);
			return success;
		}
		else if constexpr (std::is_same_v<std::remove_const_t<Value>, bool>) {
			bool success = false;
			*value = ConvertCharactersToBoolStrict(token, success);
			return success;
		}
		else {
			static_assert(false, "Invalid parse value type from token");
		}
	}

	namespace Internal {
		template<typename Value, typename... Values>
		bool ParseValueFromTokens(Stream<Stream<char>> tokens, Value* value, Values... values) {
			bool success = ParseValueFromToken(tokens[0], value);
			if (!success) {
				return false;
			}
			
			if constexpr (sizeof...(Values) > 0) {
				return ParseValueFromTokens(tokens.AdvanceReturn(), values...);
			}
			else {
				return true;
			}
		}
	}

	// Based on a format where {#} represents a token to be parsed it will try to parse the values according
	// To the primitive types. Returns true if the format is matched all values correspond to the type of the
	// Values, else false
	template<typename... Values>
	bool ParseValuesFromFormat(Stream<char> string, Stream<char> format, Values... values) {
		ECS_STACK_CAPACITY_STREAM(Stream<char>, tokens, 128);
		bool success = ParseTokensFromFormat(string, format, &tokens);
		if (success) {
			return Internal::ParseValueFromTokens(tokens, values...);
		}
		return false;
	}

}