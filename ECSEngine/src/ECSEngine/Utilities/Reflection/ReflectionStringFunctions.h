#pragma once
#include "../../Core.h"
#include "ReflectionTypes.h"

namespace ECSEngine {

	namespace Reflection {

		// Uses a jump table
		ECSENGINE_API size_t GetReflectionBasicFieldTypeByteSize(ReflectionBasicFieldType type);

		// Uses a jump table
		ECSENGINE_API size_t GetReflectionBasicFieldTypeStringSize(ReflectionBasicFieldType type);

		// Uses a jump table
		// The Enum type is used instead of an ASCII string. The Wchar_t type is interpreted as a string of wide characters
		ECSENGINE_API void GetReflectionBasicFieldTypeString(ReflectionBasicFieldType type, CapacityStream<char>* string);

		// Uses a jump table
		// The Enum type is used instead of an ASCII string. The Wchar_t type is interpreted as a string of wide characters
		ECSENGINE_API size_t GetReflectionStreamFieldTypeStringSize(ReflectionStreamFieldType type);

		// Uses a jump table
		ECSENGINE_API void GetReflectionStreamFieldTypeString(ReflectionStreamFieldType type, CapacityStream<char>* string);

		// This one parses things like float, uint8_t, char, etc
		ECSENGINE_API ReflectionBasicFieldType ConvertStringToBasicFieldType(Stream<char> string);

		// This one parses primitive types, float, uint8_t, char, Stream<float>, float*
		ECSENGINE_API void ConvertStringToPrimitiveType(Stream<char> string, ReflectionBasicFieldType& basic_type, ReflectionStreamFieldType& stream_type);

		// This one parses the STRING() variant of the ReflectionBasicFieldType::
		ECSENGINE_API ReflectionBasicFieldType ConvertStringAliasToBasicFieldType(Stream<char> string);

		ECSENGINE_API ReflectionStreamFieldType ConvertStringToStreamFieldType(Stream<char> string);

		// Uses a jump table to convert the type
		// The Enum type is used for ASCII stream. Wchar_t will be treated as a wide character stream (In both cases a Stream<char/wchar_t>* is expected as data)
		// Returns the amount of characters written
		ECSENGINE_API size_t ConvertReflectionBasicFieldTypeToString(ReflectionBasicFieldType type, const void* data, CapacityStream<char>& characters);

		// Uses a jump table to convert the type
		// The Enum type is used for ASCII stream. Wchar_t will be treated as a wide character stream (In both cases a Stream<char/wchar_t>* is expected as data)
		// Returns whether or not it succeded - it can fail for example for when there are alphabet characters mixed with digits for integers
		ECSENGINE_API bool ParseReflectionBasicFieldType(ReflectionBasicFieldType type, Stream<char> characters, void* data);

		// Returns how many characters this takes up
		ECSENGINE_API size_t ConvertReflectionBasicFieldTypeToStringSize(ReflectionBasicFieldType type, const void* data);

		// Will try to find out which basic type it is
		// For integers it will return Int64 for signed ones and UInt64 for unsigned ones and the corresponding types for 2, 3 and 4 components
		// Floating point numbers cannot be distinguished - floats will be reported as doubles
		// For normal ASCII strings it will return Int8
		// If the normal types cannot be deduced (e.g. integer, float, double), then the field will be reported as a character stream (ASCII)
		// Booleans will be reported as unsigned integers
		// If a mismatch happens, it will return unknown
		ECSENGINE_API ReflectionBasicFieldType DeduceTypeFromString(Stream<char> characters);

		// Finds out which part of the definition is just the user defined type from the stream type
		// E.g. Stream<MyType> -> MyType
		// E.g. MyType* -> MyType
		ECSENGINE_API Stream<char> GetUserDefinedTypeFromStreamUserDefined(Stream<char> definition, ReflectionStreamFieldType stream_type);

	}

}