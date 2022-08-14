#include "ecspch.h"
#include "ShaderReflection.h"
#include "../Utilities/FunctionInterfaces.h"
#include "Shaders/Macros.hlsli"
#include "../Utilities/File.h"

constexpr size_t STRING_FORMAT_TABLE_CAPACITY = 128;
constexpr size_t FLOAT_FORMAT_TABLE_CAPACITY = 256;
constexpr size_t INTEGER_FORMAT_TABLE_CAPACITY = 256;

const char* VERTEX_SHADER_INPUT_STRUCTURE_NAME = "VS_INPUT";
const char* REGISTER_STRING = "register";

const char* BUFFER_KEYWORDS[] = {
	"cbuffer",
	"tbuffer",
	"Buffer",
	"RWBuffer",
	"ByteAddressBuffer",
	"RWByteAddressBuffer",
	"StructuredBuffer",
	"RWStructuredBuffer",
	"AppendStructuredBuffer",
	"ConsumeStructuredBuffer"
};

const char* TEXTURE_KEYWORDS[] = {
	"Texture1D",
	"Texture2D",
	"Texture3D",
	"RWTexture1D",
	"RWTexture2D",
	"RWTexture3D",
	"Texture1DArray",
	"Texture2DArray",
	"Texture3DArray",
	"TextureCube"
};

const char* VERTEX_BUFFER_MAPPINGS[] = {
	"POSITION",
	"NORMAL",
	"UV",
	"COLOR",
	"TANGENT",
	"BONE_WEIGHT",
	"BONE_INFLUENCE"
};

const char* VERTEX_BUFFER_MACRO_MAPPINGS[] = {
	STRING(ECS_REFLECT_POSITION),
	STRING(ECS_REFLECT_NORMAL),
	STRING(ECS_REFLECT_UV),
	STRING(ECS_REFLECT_COLOR),
	STRING(ECS_REFLECT_TANGENT),
	STRING(ECS_REFLECT_BONE_WEIGHT),
	STRING(ECS_REFLECT_BONE_INFLUENCE)
};

const char* DXGI_FLOAT_FORMAT_MAPPINGS[] = {
	STRING(ECS_REFLECT_UNORM_8),
	STRING(ECS_REFLECT_SNORM_8),
	STRING(ECS_REFLECT_UNORM_16),
	STRING(ECS_REFLECT_SNORM_16)
};

const char* DXGI_UINT_FORMAT_MAPPINGS[] = {
	STRING(ECS_REFLECT_UINT_8),
	STRING(ECS_REFLECT_UINT_16),
	STRING(ECS_REFLECT_UINT_32)
};

const char* DXGI_SINT_FORMAT_MAPPINGS[] = {
	STRING(ECS_REFLECT_SINT_8),
	STRING(ECS_REFLECT_SINT_16),
	STRING(ECS_REFLECT_SINT_32)
};

namespace ECSEngine {

#define ADD_TO_FORMAT_TABLE(name, format, table) identifier.ptr = #name; identifier.size = strlen(#name); \
ECS_ASSERT(!table.Insert(format, identifier));

#define ADD_TO_STRING_FORMAT_TABLE(format, table) ADD_TO_FORMAT_TABLE(format, format, table);


#define INITIALIZE_STRING_FORMAT_TABLE(table) ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_UNKNOWN, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32G32B32A32_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32G32B32A32_FLOAT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32G32B32A32_UINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32G32B32A32_SINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32G32B32_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32G32B32_FLOAT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32G32B32_UINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32G32B32_SINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16G16B16A16_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16G16B16A16_FLOAT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16G16B16A16_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16G16B16A16_UINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16G16B16A16_SNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16G16B16A16_SINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32G32_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32G32_FLOAT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32G32_UINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32G32_SINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32G8X24_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_X32_TYPELESS_G8X24_UINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R10G10B10A2_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R10G10B10A2_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R10G10B10A2_UINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R11G11B10_FLOAT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R8G8B8A8_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R8G8B8A8_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R8G8B8A8_UINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R8G8B8A8_SNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R8G8B8A8_SINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16G16_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16G16_FLOAT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16G16_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16G16_UINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16G16_SNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16G16_SINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_D32_FLOAT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32_FLOAT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32_UINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R32_SINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R24G8_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_D24_UNORM_S8_UINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R24_UNORM_X8_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_X24_TYPELESS_G8_UINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R8G8_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R8G8_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R8G8_UINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R8G8_SNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R8G8_SINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16_FLOAT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_D16_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16_UINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16_SNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R16_SINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R8_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R8_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R8_UINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R8_SNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R8_SINT, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_A8_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R1_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R9G9B9E5_SHAREDEXP, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R8G8_B8G8_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_G8R8_G8B8_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC1_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC1_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC1_UNORM_SRGB, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC2_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC2_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC2_UNORM_SRGB, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC3_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC3_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC3_UNORM_SRGB, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC4_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC4_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC4_SNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC5_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC5_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC5_SNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_B5G6R5_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_B5G5R5A1_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_B8G8R8A8_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_B8G8R8X8_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_B8G8R8A8_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_B8G8R8X8_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_B8G8R8X8_UNORM_SRGB, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC6H_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC6H_UF16, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC6H_SF16, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC7_TYPELESS, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC7_UNORM, table); \
	ADD_TO_STRING_FORMAT_TABLE(DXGI_FORMAT_BC7_UNORM_SRGB, table);


	// ------------------------------------------------------------------------------------------------------------------------------------------

	Stream<char> SetName(char* starting_ptr, char* ending_ptr, CapacityStream<char>& name_pool) {
		size_t name_size = ending_ptr - starting_ptr;
		char* current_name = name_pool.buffer + name_pool.size;
		memcpy(current_name, starting_ptr, name_size);
		current_name[name_size] = '\0';
		name_pool.size += name_size + 1;
		return { current_name, name_size };
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	Stream<char> SetName(char* name, size_t size, CapacityStream<char>& name_pool) {
		char* current_name = name_pool.buffer + name_pool.size;
		memcpy(current_name, name, size);
		current_name[size] = '\0';
		name_pool.size += size + 1;
		return { current_name, size };
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	unsigned short GetRegisterIndex(char* register_ptr) {
		char* number_start = strchr(register_ptr, '(');
		number_start++;
		while (function::IsAlphabetCharacter(*number_start)) {
			number_start++;
		}

		char* number_end = number_start;
		while (function::IsNumberCharacter(*number_end)) {
			number_end++;
		}
		return function::ConvertCharactersToInt(Stream<char>(number_start, number_end - number_start));
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	// Returns whether or not the register is a default one
	bool ParseNameAndRegister(char* type_start, CapacityStream<char>& name_pool, Stream<char>& output_name, unsigned short& register_index) {
		char* current_character = type_start;
		// Get the buffer name
		while (*current_character != ' ' && *current_character != '\t') {
			current_character++;
		}
		while (*current_character == ' ' || *current_character == '\t') {
			current_character++;
		}
		char* name_start = current_character;
		while (function::IsCodeIdentifierCharacter(*current_character)) {
			current_character++;
		}
		output_name = SetName(name_start, current_character, name_pool);

		// Get the buffer register
		char* register_ptr = strstr(current_character, REGISTER_STRING);
		if (register_ptr != nullptr) {
			register_index = GetRegisterIndex(register_ptr);
			return false;
		}
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	size_t GetBasicTypeByteSize(const char* type_start, const char* type_end) {
		// Get the number of components 
		const char* current_character = type_start;
		while (!function::IsNumberCharacter(*current_character) && current_character < type_end) {
			current_character++;
		}

		// No number character has been found - only a component - 4 bytes sized
		if (current_character == type_end) {
			return 4;
		}

		// A number character has been found
		size_t component_count = 0;
		while (function::IsNumberCharacter(*current_character) && current_character < type_end) {
			component_count *= 10;
			component_count += *current_character - '0';
			current_character++;
		}

		// If component count is 0 or greater than 4 - error - return -1
		if (component_count == 0 || component_count > 4) {
			return -1;
		}

		// Matrix types must be followed by 'x'
		if (*current_character == 'x') {
			size_t row_count = 0;
			current_character++;
			while (function::IsNumberCharacter(*current_character) && current_character < type_end) {
				row_count *= 10;
				row_count += *current_character - '0';
				current_character++;
			}

			// If row count is 0 or greater than 4 - error - return -1
			if (row_count == 0 || row_count > 4) {
				return -1;
			}

			return row_count * component_count * 4;
		}
	
		// No matrix type
		return component_count * 4;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	size_t GetBasicTypeSemanticCount(const char* type_start, const char* type_end) {
		// Get the number of components 
		const char* current_character = type_start;
		while (!function::IsNumberCharacter(*current_character) && current_character < type_end) {
			current_character++;
		}

		// No number character has been found - only a component - semantic count is 1
		if (current_character == type_end) {
			return 1;
		}

		// A number character has been found
		size_t component_count = 0;
		while (function::IsNumberCharacter(*current_character) && current_character < type_end) {
			component_count *= 10;
			component_count += *current_character - '0';
			current_character++;
		}

		// If component count is 0 or greater than 4 - error - return -1
		if (component_count == 0 || component_count > 4) {
			return -1;
		}

		// Matrix types must be followed by 'x'
		if (*current_character == 'x') {
			size_t row_count = 0;
			current_character++;
			while (function::IsNumberCharacter(*current_character) && current_character < type_end) {
				row_count *= 10;
				row_count += *current_character - '0';
				current_character++;
			}

			// If row count is 0 or greater than 4 - error - return -1
			if (row_count == 0 || row_count > 4) {
				return -1;
			}

			// Semantic count will be the component count - which is the count of rows
			return row_count;
		}

		// No matrix type - a single semantic instance
		return 1;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	DXGI_FORMAT GetModifiedIntegerFormat(
		Stream<const char*> mappings, 
		const char* colon_character, 
		const char* type_start, 
		const char* basic_type_end, 
		const ShaderReflectionIntegerFormatTable& integer_table
	) {
		unsigned int modifier_index = ECS_SHADER_REFLECTION_SINT_32;

		// Look for uint modifiers
		for (size_t index = 0; index < mappings.size; index++) {
			const char* modifier = strstr(colon_character, mappings[index]);
			if (modifier != nullptr) {
				modifier_index = index;
				break;
			}
		}

		ResourceIdentifier identifier(type_start, basic_type_end - type_start);
		ShaderReflectionIntegerExtendedFormat extended_format;
		bool success = integer_table.TryGetValue(identifier, extended_format);
		if (!success) {
			return DXGI_FORMAT_FORCE_UINT;
		}
		return extended_format.formats[modifier_index];
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	ShaderReflection::ShaderReflection() {}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	ShaderReflection::ShaderReflection(void* allocation)
	{
		uintptr_t buffer = (uintptr_t)allocation;
		string_table.InitializeFromBuffer(buffer, STRING_FORMAT_TABLE_CAPACITY);
		float_table.InitializeFromBuffer(buffer, FLOAT_FORMAT_TABLE_CAPACITY);
		unsigned_table.InitializeFromBuffer(buffer, INTEGER_FORMAT_TABLE_CAPACITY);
		signed_table.InitializeFromBuffer(buffer, INTEGER_FORMAT_TABLE_CAPACITY);

		ResourceIdentifier identifier;

		INITIALIZE_STRING_FORMAT_TABLE(string_table);

		ShaderReflectionFloatExtendedFormat float_format;
		ShaderReflectionIntegerExtendedFormat integer_format;

		// 1 component float
		float_format.formats[ECS_SHADER_REFLECTION_FLOAT] = DXGI_FORMAT_R32_FLOAT;
		float_format.formats[ECS_SHADER_REFLECTION_UNORM_8] = DXGI_FORMAT_R8_UNORM;
		float_format.formats[ECS_SHADER_REFLECTION_SNORM_8] = DXGI_FORMAT_R8_SNORM;
		float_format.formats[ECS_SHADER_REFLECTION_UNORM_16] = DXGI_FORMAT_R16_UNORM;
		float_format.formats[ECS_SHADER_REFLECTION_SNORM_16] = DXGI_FORMAT_R16_SNORM;
		ADD_TO_FORMAT_TABLE(float, float_format, float_table);

		// 2 component float
		float_format.formats[ECS_SHADER_REFLECTION_FLOAT] = DXGI_FORMAT_R32G32_FLOAT;
		float_format.formats[ECS_SHADER_REFLECTION_UNORM_8] = DXGI_FORMAT_R8G8_UNORM;
		float_format.formats[ECS_SHADER_REFLECTION_SNORM_8] = DXGI_FORMAT_R8G8_SNORM;
		float_format.formats[ECS_SHADER_REFLECTION_UNORM_16] = DXGI_FORMAT_R16G16_UNORM;
		float_format.formats[ECS_SHADER_REFLECTION_SNORM_16] = DXGI_FORMAT_R16G16_SNORM;
		ADD_TO_FORMAT_TABLE(float2, float_format, float_table);

		// 3 component float
		float_format.formats[ECS_SHADER_REFLECTION_FLOAT] = DXGI_FORMAT_R32G32B32_FLOAT;

		// There are no equivalent formats for the 3 component width - error if asked for
		// this type of format
		float_format.formats[ECS_SHADER_REFLECTION_UNORM_8] = DXGI_FORMAT_FORCE_UINT;
		float_format.formats[ECS_SHADER_REFLECTION_SNORM_8] = DXGI_FORMAT_FORCE_UINT;
		float_format.formats[ECS_SHADER_REFLECTION_UNORM_16] = DXGI_FORMAT_FORCE_UINT;
		float_format.formats[ECS_SHADER_REFLECTION_SNORM_16] = DXGI_FORMAT_FORCE_UINT;
		ADD_TO_FORMAT_TABLE(float3, float_format, float_table);

		// 4 component float
		float_format.formats[ECS_SHADER_REFLECTION_FLOAT] = DXGI_FORMAT_R32G32B32A32_FLOAT;
		float_format.formats[ECS_SHADER_REFLECTION_UNORM_8] = DXGI_FORMAT_R8G8B8A8_UNORM;
		float_format.formats[ECS_SHADER_REFLECTION_SNORM_8] = DXGI_FORMAT_R8G8B8A8_SNORM;
		float_format.formats[ECS_SHADER_REFLECTION_UNORM_16] = DXGI_FORMAT_R16G16B16A16_UNORM;
		float_format.formats[ECS_SHADER_REFLECTION_SNORM_16] = DXGI_FORMAT_R16G16B16A16_SNORM;
		ADD_TO_FORMAT_TABLE(float4, float_format, float_table);

		// 1 component integer
		integer_format.formats[ECS_SHADER_REFLECTION_UINT_8] = DXGI_FORMAT_R8_UINT;
		integer_format.formats[ECS_SHADER_REFLECTION_UINT_16] = DXGI_FORMAT_R16_UINT;
		integer_format.formats[ECS_SHADER_REFLECTION_UINT_32] = DXGI_FORMAT_R32_UINT;
		ADD_TO_FORMAT_TABLE(uint, integer_format, unsigned_table);

		integer_format.formats[ECS_SHADER_REFLECTION_SINT_8] = DXGI_FORMAT_R8_SINT;
		integer_format.formats[ECS_SHADER_REFLECTION_SINT_16] = DXGI_FORMAT_R16_SINT;
		integer_format.formats[ECS_SHADER_REFLECTION_SINT_32] = DXGI_FORMAT_R32_SINT;
		ADD_TO_FORMAT_TABLE(int, integer_format, signed_table);

		// 2 component integer
		integer_format.formats[ECS_SHADER_REFLECTION_UINT_8] = DXGI_FORMAT_R8G8_UINT;
		integer_format.formats[ECS_SHADER_REFLECTION_UINT_16] = DXGI_FORMAT_R16G16_UINT;
		integer_format.formats[ECS_SHADER_REFLECTION_UINT_32] = DXGI_FORMAT_R32G32_UINT;
		ADD_TO_FORMAT_TABLE(uint2, integer_format, unsigned_table);

		integer_format.formats[ECS_SHADER_REFLECTION_SINT_8] = DXGI_FORMAT_R8G8_SINT;
		integer_format.formats[ECS_SHADER_REFLECTION_SINT_16] = DXGI_FORMAT_R16G16_SINT;
		integer_format.formats[ECS_SHADER_REFLECTION_SINT_32] = DXGI_FORMAT_R32G32_SINT;
		ADD_TO_FORMAT_TABLE(int2, integer_format, signed_table);

		// 3 component integer

		// There are no 3 component 8 or 16 bit formats - only 32 bit
		integer_format.formats[ECS_SHADER_REFLECTION_UINT_8] = DXGI_FORMAT_FORCE_UINT;
		integer_format.formats[ECS_SHADER_REFLECTION_UINT_16] = DXGI_FORMAT_FORCE_UINT;
		integer_format.formats[ECS_SHADER_REFLECTION_UINT_32] = DXGI_FORMAT_R32G32B32_UINT;
		ADD_TO_FORMAT_TABLE(uint3, integer_format, unsigned_table);

		integer_format.formats[ECS_SHADER_REFLECTION_SINT_8] = DXGI_FORMAT_FORCE_UINT;
		integer_format.formats[ECS_SHADER_REFLECTION_SINT_16] = DXGI_FORMAT_FORCE_UINT;
		integer_format.formats[ECS_SHADER_REFLECTION_SINT_32] = DXGI_FORMAT_R32G32B32_SINT;
		ADD_TO_FORMAT_TABLE(int3, integer_format, signed_table);

		// 4 component integer
		integer_format.formats[ECS_SHADER_REFLECTION_UINT_8] = DXGI_FORMAT_R8G8B8A8_UINT;
		integer_format.formats[ECS_SHADER_REFLECTION_UINT_16] = DXGI_FORMAT_R16G16B16A16_UINT;
		integer_format.formats[ECS_SHADER_REFLECTION_UINT_32] = DXGI_FORMAT_R32G32B32A32_UINT;
		ADD_TO_FORMAT_TABLE(uint4, integer_format, unsigned_table);

		integer_format.formats[ECS_SHADER_REFLECTION_SINT_8] = DXGI_FORMAT_R8G8B8A8_SINT;
		integer_format.formats[ECS_SHADER_REFLECTION_SINT_16] = DXGI_FORMAT_R16G16B16A16_SINT;
		integer_format.formats[ECS_SHADER_REFLECTION_SINT_32] = DXGI_FORMAT_R32G32B32A32_SINT;
		ADD_TO_FORMAT_TABLE(int4, integer_format, signed_table);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	template<typename Functor>
	bool ReflectProperty(ShaderReflection* reflection, Stream<wchar_t> path, Functor&& functor) {
		Stream<char> file_contents = ReadWholeFileText(path);
		if (file_contents.buffer != nullptr) {
			// Make \0 the last character
			file_contents[file_contents.size - 1] = '\0';
			bool status = functor(file_contents);
			free(file_contents.buffer);
			return status;
		}

		return false;
	}

	bool ShaderReflection::ReflectVertexShaderInput(Stream<wchar_t> path, CapacityStream<D3D11_INPUT_ELEMENT_DESC>& elements, CapacityStream<char> semantic_name_pool)
	{
		return ReflectProperty(this, path, [&](Stream<char> data) {
			return ReflectVertexShaderInputSource(data, elements, semantic_name_pool);
		});
	}

	bool ShaderReflection::ReflectVertexShaderInputSource(Stream<char> source_code, CapacityStream<D3D11_INPUT_ELEMENT_DESC>& elements, CapacityStream<char> semantic_name_pool)
	{
		// Make the last character \0 - it will be a non important character
		source_code[source_code.size - 1] = '\0';

		char* structure_name = strstr(source_code.buffer, VERTEX_SHADER_INPUT_STRUCTURE_NAME);
		bool is_increment_input_slot = false;
		if (structure_name != nullptr) {
			// Check increment input slot
			is_increment_input_slot = strstr(structure_name, STRING(ECS_REFLECT_INCREMENT_INPUT_SLOT)) != nullptr;

			// Iterate through all of the input's fields and fill the information
			while (*structure_name != '\n') {
				structure_name++;
			}
			structure_name++;

			char* last_character = structure_name;
			last_character = *last_character == '{' ? last_character + 2 : last_character;
			while (*last_character != '}') {
				size_t current_index = elements.size;

				const char* type_start = function::SkipWhitespace(last_character);
				const char* type_end = function::SkipCodeIdentifier(type_start);

				// make the end line character \0 for C functions
				char* end_line = strchr(last_character, '\n');
				if (end_line == nullptr) {
					return false;
				}
				*end_line = '\0';

				if (type_start > end_line || type_end > end_line) {
					return false;
				}

				// advance till the colon : to get the semantic
				char* current_character = last_character;
				const char* colon_character = strchr(current_character, ':');
				if (colon_character == nullptr) {
					return false;
				}
				current_character = (char*)colon_character;

				current_character++;
				while (*current_character == ' ' || *current_character == '\t') {
					current_character++;
				}
				char* semantic_name = current_character;
				while (function::IsCodeIdentifierCharacter(*current_character)) {
					current_character++;
				}
				size_t semantic_name_size = current_character - semantic_name;

				// Get the format - if there is any
				while (*current_character == '\t' || *current_character == ' ') {
					current_character++;
				}

				DXGI_FORMAT format;
				const char* reflect_format = strstr(current_character, STRING(ECS_REFLECT_FORMAT));
				// Inferred format 
				if (reflect_format == nullptr) {
					// Get the basic type - the "row" of the matrix
					char* x_character = (char*)strchr(type_start, 'x');
					const char* basic_type_end = type_end;
					if (x_character != nullptr) {
						basic_type_end = x_character;
					}

					// Make end \0 for strstr
					char* mutable_type_end = (char*)type_end;
					char mutable_type_initial = *mutable_type_end;
					*mutable_type_end = '\0';

					// Check for modifiers 
					const char* has_float = strstr(type_start, "float");
					if (has_float != nullptr) {
						unsigned int modifier_index = 0;

						// Look for float modifiers
						for (size_t index = 0; index < std::size(DXGI_FLOAT_FORMAT_MAPPINGS); index++) {
							const char* modifier = strstr(colon_character, DXGI_FLOAT_FORMAT_MAPPINGS[index]);
							if (modifier != nullptr) {
								modifier_index = index + 1;
								break;
							}
						}

						ResourceIdentifier identifier(type_start, basic_type_end - type_start);
						ShaderReflectionFloatExtendedFormat extended_format;
						bool success = float_table.TryGetValue(identifier, extended_format);
						if (!success) {
							return false;
						}

						format = extended_format.formats[modifier_index];
					}
					else {
						// UINT
						const char* has_uint = strstr(type_start, "uint");
						if (has_uint != nullptr) {
							format = GetModifiedIntegerFormat(
								Stream<const char*>(DXGI_UINT_FORMAT_MAPPINGS, std::size(DXGI_UINT_FORMAT_MAPPINGS)),
								colon_character,
								type_start,
								basic_type_end,
								unsigned_table
							);
						}
						else {
							// SINT
							const char* has_sint = strstr(type_start, "int");
							if (has_sint != nullptr) {
								format = GetModifiedIntegerFormat(
									Stream<const char*>(DXGI_UINT_FORMAT_MAPPINGS, std::size(DXGI_UINT_FORMAT_MAPPINGS)),
									colon_character,
									type_start,
									basic_type_end,
									signed_table
								);
							}
						}
					}

					*mutable_type_end = mutable_type_initial;
				}
				else {
					current_character = (char*)reflect_format + strlen(STRING(ECS_REFLECT_FORMAT)) + 1;
					char* parenthese = current_character + 1;
					while (*parenthese != ')') {
						parenthese++;
					}

					// Format
					ResourceIdentifier identifier(current_character, parenthese - current_character);
					bool success = string_table.TryGetValue(identifier, format);
					if (!success) {
						return false;
					}

					current_character = parenthese;
				}

				// Check for incorrect formats
				if (format == DXGI_FORMAT_FORCE_UINT) {
					return false;
				}

				elements[current_index].Format = format;
				// Aligned offset
				elements[current_index].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;

				// Semantic name
				elements[current_index].SemanticName = SetName(semantic_name, semantic_name_size, semantic_name_pool).buffer;

				// Input slot - only do the check if increment input slot has not been specified
				if (!is_increment_input_slot) {
					char* input_slot = (char*)strstr(colon_character, STRING(ECS_REFLECT_INPUT_SLOT));
					if (input_slot != nullptr) {
						input_slot += strlen(STRING(ECS_REFLECT_INPUT_SLOT)) + 1;
						char* starting_input_slot = input_slot;
						while (function::IsNumberCharacter(*input_slot)) {
							input_slot++;
						}
						elements[current_index].InputSlot = function::ConvertCharactersToInt(Stream<char>(starting_input_slot, input_slot - starting_input_slot));
					}
					else {
						elements[current_index].InputSlot = 0;
					}
				}
				else {
					elements[current_index].InputSlot = elements.size;
				}

				// Instance
				char* instance = strstr(current_character, STRING(ECS_REFLECT_INSTANCE));
				if (instance == nullptr) {
					elements[current_index].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
					elements[current_index].InstanceDataStepRate = 0;
				}
				else {
					elements[current_index].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
					instance += strlen(STRING(ECS_REFLECT_INSTANCE)) + 1;
					char* instance_step_rate_start = instance;
					while (function::IsNumberCharacter(*instance)) {
						instance++;
					}
					elements[current_index].InstanceDataStepRate = function::ConvertCharactersToInt(Stream<char>(instance_step_rate_start, instance - instance_step_rate_start));
				}

				// Semantic count
				char* semantic_count = strstr(current_character, STRING(ECS_REFLECT_SEMANTIC_COUNT));
				size_t integer_semantic_count = 0;
				if (semantic_count == nullptr) {
					// Deduce the semantic count - for matrix types the second number gives the semantic count
					integer_semantic_count = GetBasicTypeSemanticCount(type_start, type_end);
					// Incorrect type
					if (integer_semantic_count == -1) {
						return false;
					}
				}
				else {
					semantic_count += strlen(STRING(ECS_REFLECT_SEMANTIC_COUNT)) + 2;
					char* semantic_count_start = semantic_count;
					while (function::IsNumberCharacter(*semantic_count)) {
						semantic_count++;
					}
					integer_semantic_count = function::ConvertCharactersToInt(Stream<char>(semantic_count_start, semantic_count - semantic_count_start));
				}

				elements[current_index].SemanticIndex = 0;
				elements.size += integer_semantic_count;
				elements.AssertCapacity();
				for (size_t index = 1; index < integer_semantic_count; index++) {
					memcpy(elements.buffer + current_index + index, elements.buffer + current_index, sizeof(D3D11_INPUT_ELEMENT_DESC));
					elements[current_index + index].SemanticIndex = index;
				}

				*end_line = '\n';
				last_character = end_line + 1;
			}
			semantic_name_pool.AssertCapacity();

			return true;
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	// Returns whether or not it succeded
	bool ShaderReflection::ReflectShaderBuffers(Stream<wchar_t> path, CapacityStream<ShaderReflectedBuffer>& buffers, CapacityStream<char> name_pool) {
		return ReflectProperty(this, path, [&](Stream<char> data) {
			return ReflectShaderBuffersSource(data, buffers, name_pool);
		});
	}

	bool ShaderReflection::ReflectShaderBuffersSource(Stream<char> source_code, CapacityStream<ShaderReflectedBuffer>& buffers, CapacityStream<char> name_pool)
	{
		// Make the last character \0 - it will be a non important character
		source_code[source_code.size - 1] = '\0';

		// Search for keywords
		// Cbuffer is a special keyword; don't include in the for loop
		char* cbuffer_ptr = strstr(source_code.buffer, BUFFER_KEYWORDS[0]);
		while (cbuffer_ptr != nullptr) {
			// Get buffer name
			cbuffer_ptr += strlen(BUFFER_KEYWORDS[0]) + 1;

			while (!function::IsAlphabetCharacter(*cbuffer_ptr)) {
				cbuffer_ptr++;
			}
			char* name_start = cbuffer_ptr;
			while (function::IsCodeIdentifierCharacter(*cbuffer_ptr)) {
				cbuffer_ptr++;
			}

			// Name and type
			size_t current_index = buffers.size;
			buffers[current_index].name = SetName(name_start, cbuffer_ptr, name_pool);
			buffers[current_index].type = ShaderBufferType::Constant;

			// Register index
			char* end_bracket = strchr(cbuffer_ptr, '}');
			*end_bracket = '\0';

			char* register_ptr = strstr(cbuffer_ptr, REGISTER_STRING);
			if (register_ptr == nullptr) {
				buffers[current_index].register_index = current_index;
			}
			else {
				buffers[current_index].register_index = GetRegisterIndex(register_ptr);
			}

			char* end_line = strchr(cbuffer_ptr, '\n');
			char* current_character = end_line + 1;

			// byte size
			size_t total_byte_size = 0;
			while (current_character != end_bracket) {
				while (!function::IsAlphabetCharacter(*current_character)) {
					current_character++;
				}

				char* start_type_ptr = current_character;
				while (function::IsCodeIdentifierCharacter(*current_character)) {
					current_character++;
				}

				size_t byte_size = GetBasicTypeByteSize(start_type_ptr, current_character);
				if (byte_size == -1) {
					return false;
				}
				total_byte_size += byte_size;
			}
			*end_bracket = '}';
			cbuffer_ptr = strstr(end_bracket + 1, BUFFER_KEYWORDS[0]);
			buffers.size++;
		}

		size_t constant_buffer_count = buffers.size;
		for (size_t index = 1; index < std::size(BUFFER_KEYWORDS); index++) {
			char* current_buffer = strstr(source_code.buffer, BUFFER_KEYWORDS[index]);
			while (current_buffer != nullptr) {
				char* end_line = strchr(current_buffer, '\n');
				*end_line = '\0';

				size_t current_index = buffers.size;
				// Byte size is zero - ignore for buffer types different from the constant buffer
				// The type is associated with the iteration index
				buffers[current_index].byte_size = 0;
				buffers[current_index].type = (ShaderBufferType)index;

				Stream<char> name;
				unsigned short register_index;
				bool default_register = ParseNameAndRegister(current_buffer, name_pool, name, register_index);
				buffers[current_index].name = name;
				buffers[current_index].register_index = default_register ? current_index - constant_buffer_count : register_index;

				buffers.size++;
				*end_line = '\n';
				current_buffer = strstr(end_line + 1, BUFFER_KEYWORDS[index]);
			}
		}

		name_pool.AssertCapacity();
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	bool ShaderReflection::ReflectShaderTextures(Stream<wchar_t> path, CapacityStream<ShaderReflectedTexture>& textures, CapacityStream<char> name_pool)
	{
		return ReflectProperty(this, path, [&](Stream<char> data) {
			return ReflectShaderTexturesSource(data, textures, name_pool);
		});
	}

	bool ShaderReflection::ReflectShaderTexturesSource(Stream<char> source_code, CapacityStream<ShaderReflectedTexture>& textures, CapacityStream<char> name_pool)
	{
		// Make the last character \0 - it will be a non important character
		source_code[source_code.size - 1] = '\0';

		// Linearly iterate through all keywords and add each texture accordingly
		for (size_t index = 0; index < std::size(TEXTURE_KEYWORDS); index++) {
			char* texture_ptr = strstr(source_code.buffer, TEXTURE_KEYWORDS[index]);
			while (texture_ptr != nullptr) {
				char* end_line = strchr(texture_ptr, '\n');
				*end_line = '\0';

				Stream<char> name;
				unsigned short register_index;
				bool default_value = ParseNameAndRegister(texture_ptr, name_pool, name, register_index);

				size_t current_index = textures.size;
				textures[current_index].name = name;
				textures[current_index].type = (ShaderTextureType)index;
				textures[current_index].register_index = default_value ? current_index : register_index;

				*end_line = '\n';
				texture_ptr = strstr(end_line + 1, TEXTURE_KEYWORDS[index]);
				textures.size++;
			}
		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	bool ShaderReflection::ReflectVertexBufferMapping(Stream<wchar_t> path, CapacityStream<ECS_MESH_INDEX>& mapping)
	{
		return ReflectProperty(this, path, [&](Stream<char> data) {
			return ReflectVertexBufferMappingSource(data, mapping);
		});
	}

	bool ShaderReflection::ReflectVertexBufferMappingSource(Stream<char> source_code, CapacityStream<ECS_MESH_INDEX>& mapping)
	{
		// Make the last character \0 - it will be a non important character
		source_code[source_code.size - 1] = '\0';

		// Returns whether or not it succeded
		auto KeywordLoop = [](const char* struct_ptr, CapacityStream<ECS_MESH_INDEX>& mapping, Stream<const char*> keywords) {
			// Make the } \0 to avoid searching past the structure
			char* closing_bracket = (char*)strchr(struct_ptr, '}');
			if (closing_bracket == nullptr) {
				return false;
			}
			*closing_bracket = '\0';

			for (size_t index = 0; index < keywords.size; index++) {
				char* current_mapping = (char*)strstr(struct_ptr, keywords[index]);
				if (current_mapping != nullptr) {
					// Count the semicolons to determinte the current index
					// Null the first character after the first semicolon and start counting the semicolons
					current_mapping = strchr(current_mapping, ';');
					if (current_mapping == nullptr) {
						return false;
					}

					current_mapping++;
					char previous_value = *current_mapping;
					*current_mapping = '\0';

					size_t semicolon_count = 0;
					const char* semicolon_ptr = struct_ptr;
					while ((semicolon_ptr = strchr(semicolon_ptr + 1, ';')) != nullptr) {
						semicolon_count++;
					}

					*current_mapping = previous_value;
					semicolon_count--;

					if (semicolon_count == -1) {
						return false;
					}

					mapping[semicolon_count] = (ECS_MESH_INDEX)index;
					mapping.size = std::max(mapping.size, (unsigned int)semicolon_count + 1);
				}
			}

			*closing_bracket = '}';
			return true;
		};

		const char* struct_ptr = strstr(source_code.buffer, VERTEX_SHADER_INPUT_STRUCTURE_NAME);
		// Linearly iterate through all keywords and add each mapping accordingly
		bool success = KeywordLoop(struct_ptr, mapping, Stream<const char*>(VERTEX_BUFFER_MAPPINGS, std::size(VERTEX_BUFFER_MAPPINGS)));

		if (!success) {
			return false;
		}

		// Check macros now
		success = KeywordLoop(struct_ptr, mapping, Stream<const char*>((VERTEX_BUFFER_MACRO_MAPPINGS), std::size(VERTEX_BUFFER_MACRO_MAPPINGS)));

		return success;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	size_t ShaderReflection::MemoryOf()
	{
		return ShaderReflectionFormatTable::MemoryOf(STRING_FORMAT_TABLE_CAPACITY) + ShaderReflectionFloatFormatTable::MemoryOf(FLOAT_FORMAT_TABLE_CAPACITY) +
			ShaderReflectionIntegerFormatTable::MemoryOf(INTEGER_FORMAT_TABLE_CAPACITY) * 2;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

}