#include "ecspch.h"
#include "ReflectionStringFunctions.h"
#include "../FunctionInterfaces.h"
#include "../Function.h"

namespace ECSEngine {

	namespace Reflection {

		struct Pairing {
			const char* string;
			ReflectionBasicFieldType type;
		};

#define FIELD_TYPE_STRING(string, reflection)  { STRING(string), ReflectionBasicFieldType::reflection }

		Pairing ECS_REFLECTION_BASIC_FIELD_TYPE_PAIRINGS[] = {
			FIELD_TYPE_STRING(char, Int8),
			FIELD_TYPE_STRING(int8_t, Int8),
			FIELD_TYPE_STRING(unsigned char, UInt8),
			FIELD_TYPE_STRING(uint8_t, UInt8),
			FIELD_TYPE_STRING(short, Int16),
			FIELD_TYPE_STRING(int16_t, Int16),
			FIELD_TYPE_STRING(unsigned short, UInt16),
			FIELD_TYPE_STRING(uint16_t, UInt16),
			FIELD_TYPE_STRING(int, Int32),
			FIELD_TYPE_STRING(int32_t, Int32),
			FIELD_TYPE_STRING(unsigned int, UInt32),
			FIELD_TYPE_STRING(uint32_t, UInt32),
			FIELD_TYPE_STRING(long long, Int64),
			FIELD_TYPE_STRING(int64_t, Int64),
			FIELD_TYPE_STRING(unsigned long long, UInt64),
			FIELD_TYPE_STRING(uint64_t, UInt64),

			FIELD_TYPE_STRING(char2, Char2),
			FIELD_TYPE_STRING(uchar2, UChar2),
			FIELD_TYPE_STRING(short2, Short2),
			FIELD_TYPE_STRING(ushort2, UShort2),
			FIELD_TYPE_STRING(int2, Int2),
			FIELD_TYPE_STRING(uint2, UInt2),
			FIELD_TYPE_STRING(long2, Long2),
			FIELD_TYPE_STRING(ulong2, ULong2),

			FIELD_TYPE_STRING(char3, Char3),
			FIELD_TYPE_STRING(uchar3, UChar3),
			FIELD_TYPE_STRING(short3, Short3),
			FIELD_TYPE_STRING(ushort3, UShort3),
			FIELD_TYPE_STRING(int3, Int3),
			FIELD_TYPE_STRING(uint3, UInt3),
			FIELD_TYPE_STRING(long3, Long3),
			FIELD_TYPE_STRING(ulong3, ULong3),

			FIELD_TYPE_STRING(char4, Char4),
			FIELD_TYPE_STRING(uchar4, UChar4),
			FIELD_TYPE_STRING(short4, Short4),
			FIELD_TYPE_STRING(ushort4, UShort4),
			FIELD_TYPE_STRING(int4, Int4),
			FIELD_TYPE_STRING(uint4, UInt4),
			FIELD_TYPE_STRING(long4, Long4),
			FIELD_TYPE_STRING(ulong4, ULong4),

			FIELD_TYPE_STRING(float, Float),
			FIELD_TYPE_STRING(float2, Float2),
			FIELD_TYPE_STRING(float3, Float3),
			FIELD_TYPE_STRING(float4, Float4),

			FIELD_TYPE_STRING(double, Double),
			FIELD_TYPE_STRING(double2, Double2),
			FIELD_TYPE_STRING(double3, Double3),
			FIELD_TYPE_STRING(double4, Double4),

			FIELD_TYPE_STRING(bool, Bool),
			FIELD_TYPE_STRING(bool2, Bool2),
			FIELD_TYPE_STRING(bool3, Bool3),
			FIELD_TYPE_STRING(bool4, Bool4),

			FIELD_TYPE_STRING(wchar_t, Wchar_t)
		};

#undef FIELD_TYPE_STRING

#define FIELD_TYPE_STRING(string) ToStream(STRING(string))

		// Jump table
		Stream<char> ECS_REFLECTION_BASIC_FIELD_TYPE_ALIAS_STRINGS[] = {
			FIELD_TYPE_STRING(Int8),
			FIELD_TYPE_STRING(UInt8),
			FIELD_TYPE_STRING(Int16),
			FIELD_TYPE_STRING(UInt16),
			FIELD_TYPE_STRING(Int32),
			FIELD_TYPE_STRING(UInt32),
			FIELD_TYPE_STRING(Int64),
			FIELD_TYPE_STRING(UInt64),
			FIELD_TYPE_STRING(Wchar_t),
			FIELD_TYPE_STRING(Float),
			FIELD_TYPE_STRING(Double),
			FIELD_TYPE_STRING(Bool),
			// Replace the enum field with the ASCII string field
			FIELD_TYPE_STRING(ASCIIString),
			FIELD_TYPE_STRING(Char2),
			FIELD_TYPE_STRING(UChar2),
			FIELD_TYPE_STRING(Short2),
			FIELD_TYPE_STRING(UShort2),
			FIELD_TYPE_STRING(Int2),
			FIELD_TYPE_STRING(UInt2),
			FIELD_TYPE_STRING(Long2),
			FIELD_TYPE_STRING(ULong2),
			FIELD_TYPE_STRING(Char3),
			FIELD_TYPE_STRING(UChar3),
			FIELD_TYPE_STRING(Short3),
			FIELD_TYPE_STRING(UShort3),
			FIELD_TYPE_STRING(Int3),
			FIELD_TYPE_STRING(UInt3),
			FIELD_TYPE_STRING(Long3),
			FIELD_TYPE_STRING(ULong3),
			FIELD_TYPE_STRING(Char4),
			FIELD_TYPE_STRING(UChar4),
			FIELD_TYPE_STRING(Short4),
			FIELD_TYPE_STRING(UShort4),
			FIELD_TYPE_STRING(Int4),
			FIELD_TYPE_STRING(UInt4),
			FIELD_TYPE_STRING(Long4),
			FIELD_TYPE_STRING(ULong4),
			FIELD_TYPE_STRING(Float2),
			FIELD_TYPE_STRING(Float3),
			FIELD_TYPE_STRING(Float4),
			FIELD_TYPE_STRING(Double2),
			FIELD_TYPE_STRING(Double3),
			FIELD_TYPE_STRING(Double4),
			FIELD_TYPE_STRING(Bool2),
			FIELD_TYPE_STRING(Bool3),
			FIELD_TYPE_STRING(Bool4),
			FIELD_TYPE_STRING(UserDefined),
			FIELD_TYPE_STRING(Unknown)
		};

		static_assert(std::size(ECS_REFLECTION_BASIC_FIELD_TYPE_ALIAS_STRINGS) == (unsigned int)ReflectionBasicFieldType::COUNT);

		// Jump table
		Stream<char> ECS_REFLECTION_STREAM_FIELD_TYPE_STRINGS[] = {
			FIELD_TYPE_STRING(Basic),
			FIELD_TYPE_STRING(Pointer),
			FIELD_TYPE_STRING(BasicTypeArray),
			FIELD_TYPE_STRING(Stream),
			FIELD_TYPE_STRING(CapacityStream),
			FIELD_TYPE_STRING(ResizableStream),
			FIELD_TYPE_STRING(Unknown)
		};

		static_assert(std::size(ECS_REFLECTION_STREAM_FIELD_TYPE_STRINGS) == (unsigned int)ReflectionStreamFieldType::COUNT);

#undef FIELD_TYPE_STRING

		typedef size_t (*ConvertFunction)(const void*, CapacityStream<char>&);

		template<typename Integer>
		size_t ConvertInteger(const void* address, CapacityStream<char>& characters) {
			return function::ConvertIntToChars(characters, *(Integer*)address);
		}

		size_t ConvertFloat(const void* address, CapacityStream<char>& characters) {
			return function::ConvertFloatToChars(characters, *(float*)address, 4);
		}

		size_t ConvertDouble(const void* address, CapacityStream<char>& characters) {
			return function::ConvertDoubleToChars(characters, *(double*)address, 4);
		}

#define COMMA_SEPARATE characters.Add(','); characters.AddSafe(' ');

		template<ConvertFunction basic_convert, int byte_size>
		size_t ConvertBasic2(const void* address, CapacityStream<char>& characters) {
			size_t first_size = basic_convert(address, characters);
			COMMA_SEPARATE;
			size_t second_size = basic_convert(function::OffsetPointer(address, byte_size), characters);

			return first_size + second_size + 2;
		}

		template<ConvertFunction basic_convert, int byte_size>
		size_t ConvertBasic3(const void* address, CapacityStream<char>& characters) {
			size_t first_size = basic_convert(address, characters);

			// Add a comma to separate the values
			COMMA_SEPARATE;
			size_t second_size = basic_convert(function::OffsetPointer(address, byte_size), characters);
			COMMA_SEPARATE;
			size_t third_size = basic_convert(function::OffsetPointer(address, 2 * byte_size), characters);

			return first_size + second_size + third_size + 4;
		}

		template<ConvertFunction basic_convert, int byte_size>
		size_t ConvertBasic4(const void* address, CapacityStream<char>& characters) {
			size_t first_size = basic_convert(address, characters);

			COMMA_SEPARATE;
			size_t second_size = basic_convert(function::OffsetPointer(address, byte_size), characters);
			COMMA_SEPARATE;
			size_t third_size = basic_convert(function::OffsetPointer(address, 2 * byte_size), characters);
			COMMA_SEPARATE;
			size_t fourth_size = basic_convert(function::OffsetPointer(address, 3 * byte_size), characters);

			return first_size + second_size + third_size + fourth_size + 6;
		}
		
		size_t ConvertBool(const void* address, CapacityStream<char>& characters) {
			const bool* state = (const bool*)address;
			characters.AddSafe(*state ? '1' : '0');
			return 1;
		}
		
		size_t ConvertTypeCannotBeConvert(const void* address, CapacityStream<char>& characters) {
			ECS_ASSERT(false, "Trying to convert a reflected field which is user defined, enum or unknown.");
			return 0;
		}

		size_t ConvertASCIICharacters(const void* address, CapacityStream<char>& characters) {
			Stream<char>* ascii_characters = (Stream<char>*)address;
			characters.AddStreamSafe(*ascii_characters);
			return ascii_characters->size;
		}

		// Blit the characters - no conversion
		size_t ConvertWideCharacters(const void* address, CapacityStream<char>& characters) {
			Stream<wchar_t>* wide_characters = (Stream<wchar_t>*)address;
			memcpy(characters.buffer + characters.size, wide_characters->buffer, wide_characters->size * sizeof(wchar_t));
			characters.size += wide_characters->size * sizeof(wchar_t);
			characters.AssertCapacity();
			// Now convert the 0 bytes into values of 1 - the 0 bytes will interfere with the C functions
			for (size_t index = 0; index < wide_characters->size; index++) {
				if (characters[index * 2 + 1] == 0) {
					characters[index * 2 + 1] = 1;
				}
			}

			return wide_characters->size * sizeof(wchar_t);
		}

#define NOTHING

#define INTEGER_GROUP(function, end) function int8_t> end, \
								function uint8_t> end, \
								function int16_t> end, \
								function uint16_t> end, \
								function int32_t> end, \
								function uint32_t> end, \
								function int64_t> end, \
								function uint64_t> end

#define BASIC_INTEGER_GROUP(function, basic_function) function<basic_function<int8_t>, 1>, \
										function<basic_function<uint8_t>, 1>, \
										function<basic_function<int16_t>, 2>, \
										function<basic_function<uint16_t>, 2>, \
										function<basic_function<int32_t>, 4>, \
										function<basic_function<uint32_t>, 4>, \
										function<basic_function<int64_t>, 8>, \
										function<basic_function<uint64_t>, 8>

		// Jump table for converting reflected fields into strings
		ConvertFunction REFLECT_CONVERT_FIELD_DATA_INTO_STRING[] = {
			INTEGER_GROUP(ConvertInteger<, NOTHING),
			// Wide char
			ConvertWideCharacters,
			ConvertFloat,
			ConvertDouble,
			ConvertBool,
			// Enum - use a place holder for ASCII characters
			ConvertASCIICharacters,
			BASIC_INTEGER_GROUP(ConvertBasic2, ConvertInteger),
			BASIC_INTEGER_GROUP(ConvertBasic3, ConvertInteger),
			BASIC_INTEGER_GROUP(ConvertBasic4, ConvertInteger),
			ConvertBasic2<ConvertFloat, sizeof(float)>,
			ConvertBasic3<ConvertFloat, sizeof(float)>,
			ConvertBasic4<ConvertFloat, sizeof(float)>,
			ConvertBasic2<ConvertDouble, sizeof(double)>,
			ConvertBasic3<ConvertDouble, sizeof(double)>,
			ConvertBasic4<ConvertDouble, sizeof(double)>,
			// Bool 2
			ConvertBasic2<ConvertBool, sizeof(bool)>,
			// Bool 3
			ConvertBasic3<ConvertBool, sizeof(bool)>,
			// Bool 4
			ConvertBasic4<ConvertBool, sizeof(bool)>,
			// User defined
			ConvertTypeCannotBeConvert,
			// Unknown
			ConvertTypeCannotBeConvert
		};

		static_assert(std::size(REFLECT_CONVERT_FIELD_DATA_INTO_STRING) == (unsigned int)ReflectionBasicFieldType::COUNT);

		typedef bool (*ParseFunction)(void* data, Stream<char> characters);

		Stream<char> TrimWhitespaces(Stream<char> characters) {
			Stream<char> trimmed_characters = characters;

			while (trimmed_characters.size > 0 && (*trimmed_characters.buffer == '\t' || *trimmed_characters.buffer == ' ')) {
				trimmed_characters.buffer++;
				trimmed_characters.size--;
			}

			while (trimmed_characters.size > 0 && (trimmed_characters[trimmed_characters.size - 1] == '\t' || trimmed_characters[trimmed_characters.size - 1] == ' ')) {
				trimmed_characters.size--;
			}

			return trimmed_characters;
		}

#define DELIMITERS '\n', ',', '\0', ']'

		template<typename Integer>
		bool ParseInteger(void* data, Stream<char> characters) {
			Integer* ptr = (Integer*)data;

			Stream<char> trimmed_characters = TrimWhitespaces(characters);
			bool is_integer = function::IsIntegerNumber(trimmed_characters);
			if (is_integer) {
				Integer integer = function::ConvertCharactersToInt<Integer>(trimmed_characters);
				*ptr = integer;
				return true;
			}
			else {
				return false;
			}
		}

		bool ParseFloat(void* data, Stream<char> characters) {
			float* ptr = (float*)data;
			Stream<char> trimmed_characters = TrimWhitespaces(characters);
			bool is_floating_point = function::IsFloatingPointNumber(trimmed_characters);
			if (is_floating_point) {
				float value = function::ConvertCharactersToFloat(trimmed_characters);
				*ptr = value;
				return true;
			}
			else {
				return false;
			}
		}

		bool ParseDouble(void* data, Stream<char> characters) {
			double* ptr = (double*)data;
			Stream<char> trimmed_characters = TrimWhitespaces(characters);
			bool is_floating_point = function::IsFloatingPointNumber(trimmed_characters);
			if (is_floating_point) {
				double value = function::ConvertCharactersToDouble(trimmed_characters);
				*ptr = value;
				return true;
			}
			else {
				return false;
			}
		}

		bool ParseBool(void* data, Stream<char> characters) {
			bool* ptr = (bool*)data;
			Stream<char> trimmed_characters = TrimWhitespaces(characters);
			if (trimmed_characters.size > 1) {
				return false;
			}
			if (trimmed_characters[0] != '1' && trimmed_characters[0] != '0') {
				return false;
			}
			bool value = trimmed_characters[0] == '1';
			*ptr = value;

			return true;
		}

		// For enum, user defined or unknown
		bool ParseError(void* data, Stream<char> characters) {
			ECS_ASSERT(false);
			return false;
		}

		template<ParseFunction basic_parse, int byte_size>
		bool ParseBasic2(void* data, Stream<char> characters) {
			Stream<char> trimmed_characters = TrimWhitespaces(characters);
			// Look for a comma - if it doesn't exist fail
			char last_character = trimmed_characters[trimmed_characters.size];
			trimmed_characters[trimmed_characters.size] = '\0';

			const char* comma = strchr(trimmed_characters.buffer, ',');
			trimmed_characters[trimmed_characters.size] = last_character;

			if (comma == nullptr) {
				return false;
			}

			bool first_success = basic_parse(data, Stream<char>(trimmed_characters.buffer, comma - trimmed_characters.buffer));
			if (first_success) {
				return basic_parse(function::OffsetPointer(data, byte_size), Stream<char>(comma + 1, trimmed_characters.buffer + trimmed_characters.size - comma - 1));
			}
			return false;
		}

		template<ParseFunction basic_parse, int byte_size>
		bool ParseBasic3(void* data, Stream<char> characters) {
			Stream<char> trimmed_characters = TrimWhitespaces(characters);
			// Look for 2 commas - if they don't exist fail
			char last_character = trimmed_characters[trimmed_characters.size];
			trimmed_characters[trimmed_characters.size] = '\0';

			const char* first_comma = strchr(trimmed_characters.buffer, ',');
			if (first_comma == nullptr) {
				trimmed_characters[trimmed_characters.size] = last_character;
				return false;
			}
			const char* second_comma = strchr(first_comma + 1, ',');
			trimmed_characters[trimmed_characters.size] = last_character;

			if (second_comma == nullptr) {
				return false;
			}

			bool first_success = basic_parse(data, Stream<char>(trimmed_characters.buffer, first_comma - trimmed_characters.buffer));
			if (first_success) {
				bool second_success = basic_parse(function::OffsetPointer(data, byte_size), Stream<char>(first_comma + 1, second_comma - first_comma - 1));
				if (second_success) {
					return basic_parse(function::OffsetPointer(data, 2 * byte_size), Stream<char>(second_comma + 1, trimmed_characters.buffer + trimmed_characters.size - second_comma - 1));
				}
				return false;
			}
			return false;
		}

		template<ParseFunction basic_parse, int byte_size>
		bool ParseBasic4(void* data, Stream<char> characters) {
			Stream<char> trimmed_characters = TrimWhitespaces(characters);
			// Look for 3 commas - if they don't exist fail
			char last_character = trimmed_characters[trimmed_characters.size];
			trimmed_characters[trimmed_characters.size] = '\0';

			const char* first_comma = strchr(trimmed_characters.buffer, ',');
			if (first_comma == nullptr) {
				trimmed_characters[trimmed_characters.size] = last_character;
				return false;
			}
			const char* second_comma = strchr(first_comma + 1, ',');
			if (second_comma == nullptr) {
				trimmed_characters[trimmed_characters.size] = last_character;
				return false;
			}
			const char* third_comma = strchr(second_comma + 1, ',');
			trimmed_characters[trimmed_characters.size] = last_character;
			if (third_comma == nullptr) {
				return false;
			}

			bool first_success = basic_parse(data, Stream<char>(trimmed_characters.buffer, first_comma - trimmed_characters.buffer));
			if (first_success) {
				bool second_success = basic_parse(function::OffsetPointer(data, byte_size), Stream<char>(first_comma + 1, second_comma - first_comma - 1));
				if (second_success) {
					bool third_success = basic_parse(function::OffsetPointer(data, 2 * byte_size), Stream<char>(second_comma + 1, third_comma - second_comma - 1));
					if (third_success) {
						return basic_parse(function::OffsetPointer(data, 3 * byte_size), Stream<char>(third_comma + 1, trimmed_characters.buffer + trimmed_characters.size - third_comma - 1));
					}
					return false;
				}
				return false;
			}
			return false;
		}

		bool ParseASCIICharacters(void* data, Stream<char> characters) {
			Stream<char>* ascii_characters = (Stream<char>*)data;
			Stream<char> trimmed_characters = TrimWhitespaces(characters);
			*ascii_characters = trimmed_characters;

			return true;
		}

		bool ParseWideCharacters(void* data, Stream<char> characters) {
			Stream<wchar_t>* wide_characters = (Stream<wchar_t>*)data;
			Stream<char> trimmed_characters = TrimWhitespaces(characters);
			// There can only be an even number of characters
			if (trimmed_characters.size % 2 == 1) {
				return false;
			}

			wide_characters->buffer = (wchar_t*)trimmed_characters.buffer;
			wide_characters->size = trimmed_characters.size / 2;

			char* ascii_characters = (char*)wide_characters->buffer;
			// Now convert the bytes 1 into bytes 0
			for (size_t index = 0; index < wide_characters->size; index++) {
				if (ascii_characters[index * 2 + 1] == 1) {
					ascii_characters[index * 2 + 1] = 0;
				}
			}

			return true;
		}

#undef DELIMITERS

		ParseFunction REFLECT_PARSE_FIELD_DATA_FROM_STRING[] = {
			INTEGER_GROUP(ParseInteger< , NOTHING),
			// Wide char
			ParseWideCharacters,
			ParseFloat,
			ParseDouble,
			ParseBool,
			// Enum - use it as a placeholder for ascii stream
			ParseASCIICharacters,
			BASIC_INTEGER_GROUP(ParseBasic2, ParseInteger),
			BASIC_INTEGER_GROUP(ParseBasic3, ParseInteger),
			BASIC_INTEGER_GROUP(ParseBasic4, ParseInteger),
			ParseBasic2<ParseFloat, sizeof(float)>,
			ParseBasic3<ParseFloat, sizeof(float)>,
			ParseBasic4<ParseFloat, sizeof(float)>,
			ParseBasic2<ParseDouble, sizeof(double)>,
			ParseBasic3<ParseDouble, sizeof(double)>,
			ParseBasic4<ParseDouble, sizeof(double)>,
			// Bool 2
			ParseBasic2<ParseBool, sizeof(bool)>,
			// Bool 3
			ParseBasic3<ParseBool, sizeof(bool)>,
			// Bool 4
			ParseBasic4<ParseBool, sizeof(bool)>,
			// User defined
			ParseError,
			// Unknown
			ParseError
		};

#undef PARSE_INTEGER_GROUP

		size_t REFLECTION_BASIC_FIELD_TYPE_BYTE_SIZE[] = {
			sizeof(int8_t),
			sizeof(uint8_t),
			sizeof(int16_t),
			sizeof(uint16_t),
			sizeof(int32_t),
			sizeof(uint32_t),
			sizeof(int64_t),
			sizeof(uint64_t),
			sizeof(wchar_t),
			sizeof(float),
			sizeof(double),
			sizeof(bool),
			sizeof(unsigned char),
			sizeof(char2),
			sizeof(uchar2),
			sizeof(short2),
			sizeof(ushort2),
			sizeof(int2),
			sizeof(uint2),
			sizeof(long2),
			sizeof(ulong2),
			sizeof(char3),
			sizeof(uchar3),
			sizeof(short3),
			sizeof(ushort3),
			sizeof(int3),
			sizeof(uint3),
			sizeof(long3),
			sizeof(ulong3),
			sizeof(char4),
			sizeof(uchar4),
			sizeof(short4),
			sizeof(ushort4),
			sizeof(int4),
			sizeof(uint4),
			sizeof(long4),
			sizeof(ulong4),
			sizeof(float2),
			sizeof(float3),
			sizeof(float4),
			sizeof(double2),
			sizeof(double3),
			sizeof(double4),
			sizeof(bool2),
			sizeof(bool3),
			sizeof(bool4),
			0,
			0
		};

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionBasicFieldTypeByteSize(ReflectionBasicFieldType type)
		{
			return REFLECTION_BASIC_FIELD_TYPE_BYTE_SIZE[(unsigned int)type];
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionBasicFieldTypeStringSize(ReflectionBasicFieldType type)
		{
			return ECS_REFLECTION_BASIC_FIELD_TYPE_ALIAS_STRINGS[(unsigned int)type].size;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void GetReflectionBasicFieldTypeString(ReflectionBasicFieldType type, CapacityStream<char>* string)
		{
			string->AddStreamSafe(ECS_REFLECTION_BASIC_FIELD_TYPE_ALIAS_STRINGS[(unsigned int)type]);
			string->buffer[string->size] = '\0';
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t GetReflectionStreamFieldTypeStringSize(ReflectionStreamFieldType type)
		{
			return ECS_REFLECTION_STREAM_FIELD_TYPE_STRINGS[(unsigned int)type].size;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void GetReflectionStreamFieldTypeString(ReflectionStreamFieldType type, CapacityStream<char>* string)
		{
			string->AddStreamSafe(ECS_REFLECTION_STREAM_FIELD_TYPE_STRINGS[(unsigned int)type]);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionBasicFieldType ConvertStringToBasicFieldType(Stream<char> string)
		{
			for (size_t index = 0; index < std::size(ECS_REFLECTION_BASIC_FIELD_TYPE_PAIRINGS); index++) {
				if (function::CompareStrings(string, ToStream(ECS_REFLECTION_BASIC_FIELD_TYPE_PAIRINGS[index].string))) {
					return ECS_REFLECTION_BASIC_FIELD_TYPE_PAIRINGS[index].type;
				}
			}

			return ReflectionBasicFieldType::Unknown;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		void ConvertStringToPrimitiveType(Stream<char> string, ReflectionBasicFieldType& basic_type, ReflectionStreamFieldType& stream_type)
		{
			char previous_char = string[string.size];
			string[string.size] = '\0';

			basic_type = ReflectionBasicFieldType::UserDefined;
			stream_type = ReflectionStreamFieldType::Basic;

			const char* opened_bracket = strchr(string.buffer, '<');
			const char* closed_bracket = strchr(string.buffer, '>');

			if (opened_bracket != nullptr) {
				opened_bracket = function::SkipWhitespace(opened_bracket + 1);
				closed_bracket = function::SkipWhitespace(closed_bracket - 1, -1);

				basic_type = ConvertStringToBasicFieldType({ opened_bracket, function::PointerDifference(closed_bracket, opened_bracket) - 1 });

				const char* asterisk = strchr(opened_bracket, '*');
				if (asterisk == nullptr) {
					if (memcmp(string.buffer, "Stream<", sizeof("Stream<") - 1) == 0) {
						stream_type = ReflectionStreamFieldType::Stream;
					}
					else if (memcmp(string.buffer, "CapacityStream<", sizeof("CapacityStream<") - 1) == 0) {
						stream_type = ReflectionStreamFieldType::CapacityStream;
					}
					else if (memcmp(string.buffer, "ResizableStream<", sizeof("ResizableStream<") - 1) == 0) {
						stream_type = ReflectionStreamFieldType::ResizableStream;
					}
				}
				else {
					basic_type = ReflectionBasicFieldType::Unknown;
				}
			}
			else {
				const char* asterisk = strchr(string.buffer, '*');
				if (asterisk == nullptr) {
					basic_type = ConvertStringToBasicFieldType(string);
				}
				else {
					stream_type = ReflectionStreamFieldType::Pointer;

					asterisk = function::SkipWhitespace(asterisk - 1, -1);
					basic_type = ConvertStringToBasicFieldType({ string.buffer, function::PointerDifference(asterisk, string.buffer) });
				}
			}

			string[string.size] = previous_char;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionBasicFieldType ConvertStringAliasToBasicFieldType(Stream<char> string) {
			unsigned int index = function::FindString(string, Stream<Stream<char>>(ECS_REFLECTION_BASIC_FIELD_TYPE_ALIAS_STRINGS, std::size(ECS_REFLECTION_BASIC_FIELD_TYPE_ALIAS_STRINGS)));
			if (index != -1) {
				return (ReflectionBasicFieldType)index;
			}
			return ReflectionBasicFieldType::Unknown;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionStreamFieldType ConvertStringToStreamFieldType(Stream<char> string) {
			for (size_t index = 0; index < std::size(ECS_REFLECTION_STREAM_FIELD_TYPE_STRINGS); index++) {
				if (ECS_REFLECTION_STREAM_FIELD_TYPE_STRINGS[index].size < string.size) {
					if (memcmp(string.buffer, ECS_REFLECTION_STREAM_FIELD_TYPE_STRINGS[index].buffer, ECS_REFLECTION_STREAM_FIELD_TYPE_STRINGS[index].size) == 0) {
						return (ReflectionStreamFieldType)index;
					}
				}
			}

			return ReflectionStreamFieldType::Unknown;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t ConvertReflectionBasicFieldTypeToString(ReflectionBasicFieldType type, const void* data, CapacityStream<char>& characters)
		{
			return REFLECT_CONVERT_FIELD_DATA_INTO_STRING[(unsigned int)type](data, characters);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		bool ParseReflectionBasicFieldType(ReflectionBasicFieldType type, Stream<char> characters, void* data)
		{
			return REFLECT_PARSE_FIELD_DATA_FROM_STRING[(unsigned int)type](data, characters);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		size_t ConvertReflectionBasicFieldTypeToStringSize(ReflectionBasicFieldType type, const void* data)
		{
			ECS_STACK_CAPACITY_STREAM(char, stack_allocation, 512);
			return ConvertReflectionBasicFieldTypeToString(type, data, stack_allocation);
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		ReflectionBasicFieldType DeduceTypeFromString(Stream<char> characters)
		{
			const char* initial_characters = characters.buffer;

			// temporarly replace the last character with a '\0'
			char last_character = characters[characters.size - 1];
			characters[characters.size - 1] = '\0';

			// Search for commas that would be used for basic types
			ECS_STACK_CAPACITY_STREAM(const char*, comma_positions, 64);
			const char* comma_position = strchr(initial_characters, ',');
			while (comma_position != nullptr) {
				comma_positions.AddSafe(comma_position);
				comma_position = strchr(comma_position + 1, ',');
			}
			
			// Restore the last character
			characters[characters.size - 1] = last_character;

			// If there are more than 3 commas - fail
			if (comma_positions.size > 3) {
				return ReflectionBasicFieldType::Unknown;
			}

			auto is_integer = [](const char* initial_characters, const char* last_character) {
				bool minus = false;
				// Check for integers first - only one - and digits
				for (const char* current_character = initial_characters; current_character != last_character; current_character++) {
					if (*current_character < '0' || *current_character > '9') {
						// Exclude whitespaces
						if (*current_character != ' ' && *current_character != '\n' && *current_character != '\t') {
							if (*current_character == '-') {
								if (!minus) {
									minus = true;
								}
								else {
									// Not an integer 
									return bool2(false, false);
								}
							}
							else {
								// Not an integer 
								return bool2(false, false);
							}
						}
					}
				}

				return bool2(true, minus);
			};

			auto is_float_or_double = [](const char* initial_characters, const char* last_character) {
				bool minus = false;
				bool dot = false;
				// Check for integers first - only one - and digits
				for (const char* current_character = initial_characters; current_character != last_character; current_character++) {
					if (*current_character < '0' || *current_character > '9') {
						// Exclude whitespaces
						if (*current_character != ' ' && *current_character != '\n' && *current_character != '\t') {
							if (*current_character == '-') {
								if (!minus) {
									minus = true;
								}
								else {
									// Not a number
									return false;
								}
							}
							else if (*current_character == '.') {
								if (!dot) {
									dot = true;
								}
								else {
									return false;
								}
							}
							else {
								// Not a number
								return false;
							}
						}
					}
				}

				return true;
			};

			bool2 integer_type = { false, false };
			bool double_type = false;

			const char* starting_validation_character = initial_characters;
			const char* ending_validation_character = comma_positions.size > 0 ? comma_positions[0] : characters.buffer + characters.size;
			for (size_t index = 0; index < comma_positions.size + 1; index++) {
				bool2 current_integer = is_integer(starting_validation_character, ending_validation_character);
				bool both_false = false;
				if (current_integer.x) {
					if (double_type) {
						// The types are incoherent between commas - fail
						return ReflectionBasicFieldType::Unknown;
					}
					// The integer type will be signed if the current one is signed
					integer_type.y = current_integer.y ? true : integer_type.y;
					integer_type.x = true;
				}
				else {
					bool current_double = is_float_or_double(starting_validation_character, ending_validation_character);
					if (current_double) {
						if (integer_type.x) {
							// The types are incoherent between commas - fail
							return ReflectionBasicFieldType::Unknown;
						}
						double_type = true;
					}
					else {
						both_false = true;
					}
				}

				// Characters or wide characters or incorrect sequence - fail only if a comma was detected
				// If no comma was detected this is fine - it can be a narrow or wide character stream
				if (both_false && comma_positions.size > 0) {
					return ReflectionBasicFieldType::Unknown;
				}
				// update the start and end
				starting_validation_character = comma_positions[index];
				ending_validation_character = index == comma_positions.size - 1 ? characters.buffer + characters.size : comma_positions[index + 1];
			}

			// Comma positions is zero - might be narrow or wide character stream
			if (comma_positions.size == 0 && !integer_type.x && !double_type) {
				// Detect if it is wide - check for ascii values and zeros in between them
				// Ignore tabs and spaces until a different character is found
				while (*initial_characters == '\t' || *initial_characters == ' ') {
					initial_characters++;
				}

				// There is no space in between them - fail
				if (initial_characters == characters.buffer + characters.size) {
					return ReflectionBasicFieldType::Unknown;
				}

				const char* starting_stream = initial_characters;
				// For every 2 bytes check that the top one is zero
				while (initial_characters < characters.buffer + characters.size) {
					if (initial_characters[0] != '\0') {
						// Normal character stream
						return ReflectionBasicFieldType::Int8;
					}
				}
				// Got to the end - wide chars
				return ReflectionBasicFieldType::Wchar_t;
			}

#define CASE(signed_type, unsigned_type, floating_type) if (integer_type.x) { if (integer_type.y) return ReflectionBasicFieldType::signed_type; return ReflectionBasicFieldType::unsigned_type; } \
														else if (double_type) { return ReflectionBasicFieldType::floating_type; }

			switch (comma_positions.size) {
			case 0:
				CASE(Int64, UInt64, Double);
			case 1:
				CASE(Long2, ULong2, Double2);
			case 2:
				CASE(Long3, ULong3, Double3);
			case 3:
				CASE(Long4, ULong4, Double4);
			}

#undef CASE

			// If an error managed to get to here return unknown
			return ReflectionBasicFieldType::Unknown;
		}

		// ----------------------------------------------------------------------------------------------------------------------------

		Stream<char> GetUserDefinedTypeFromStreamUserDefined(const char* definition, ReflectionStreamFieldType stream_type)
		{
			Stream<char> stream_definition = ToStream(definition);
			switch (stream_type)
			{
			case ReflectionStreamFieldType::Basic:
				return stream_definition;
				break;
			case ReflectionStreamFieldType::Pointer:
			{
				const char* asterisk = strchr(definition, '*');
				asterisk = function::SkipWhitespace(asterisk, -1);
				return { definition, function::PointerDifference(asterisk, definition) + 1 };
			}
				break;
			case ReflectionStreamFieldType::BasicTypeArray:
			{
				return stream_definition;
			}
				break;
			case ReflectionStreamFieldType::Stream:
			case ReflectionStreamFieldType::CapacityStream:
			case ReflectionStreamFieldType::ResizableStream:
			{
				const char* opened_bracket = strchr(definition, '<');
				const char* closed_bracket = strchr(definition, '>');
				opened_bracket = function::SkipWhitespace(opened_bracket);
				closed_bracket = function::SkipWhitespace(closed_bracket, -1);

				return { opened_bracket, function::PointerDifference(closed_bracket, opened_bracket) + 1 };
			}
				break;
			default:
				break;
			}

			return { nullptr, 0 };
		}

		// ----------------------------------------------------------------------------------------------------------------------------

	}

}