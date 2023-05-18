#include "ecspch.h"
#include "ShaderReflection.h"
#include "../Utilities/FunctionInterfaces.h"
#include "Shaders/Macros.hlsli"
#include "../Utilities/File.h"
#include "../Utilities/Reflection/Reflection.h"

#define STRING_FORMAT_TABLE_CAPACITY 128
#define FLOAT_FORMAT_TABLE_CAPACITY 128
#define INTEGER_FORMAT_TABLE_CAPACITY 128
#define CB_TYPE_MAPPING_CAPACITY 128

#define COLOR_FLOAT3_TAG "FLOAT3"

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

const char* SAMPLER_KEYWORDS[] = {
	"SamplerState"
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

	// Returns the count of a basic type array inside a constant buffer. It returns 0 if there is no basic type array
	unsigned short ParseBasicTypeArrayCount(const char* starting_character, const char* new_line_character) {
		unsigned short result = 0;

		starting_character = function::SkipWhitespace(starting_character);
		if (starting_character < new_line_character) {
			if (*starting_character == '[') {
				// We have a basic type array
				const char* starting_int_val = starting_character + 1;
				while (*starting_character != ']' && starting_character < new_line_character) {
					starting_character++;
				}

				if (starting_character < new_line_character) {
					// The bracket is closed
					Stream<char> parse_string = { starting_int_val, function::PointerDifference(starting_character, starting_int_val) };
					result = function::ConvertCharactersToInt(parse_string);
				}
			}
		}

		return result;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	// Returns the byte offset of the element. If no packoffset is specified, then it will return USHORT_MAX
	unsigned short ParsePackoffset(const char* starting_character, const char* new_line_character) {
		unsigned short offset = USHORT_MAX;

		starting_character = function::SkipWhitespace(starting_character);
		if (starting_character < new_line_character) {
			const char* semicolon = strchr(starting_character, ':');
			if (semicolon != nullptr && semicolon < new_line_character) {
				// We have a packoffset specification
				const char* packoffset_start = function::SkipWhitespace(semicolon + 1);
				const char* paranthese_start = function::SkipCodeIdentifier(packoffset_start);
				paranthese_start = function::SkipWhitespace(paranthese_start);
				if (*paranthese_start == '(') {
					const char* paranthese_end = strchr(paranthese_start, ')');
					if (paranthese_end != nullptr && paranthese_end < new_line_character) {
						// Determine if there is a dot in between
						const char* dot = strchr(paranthese_start, '.');
						Stream<char> int_parse = { paranthese_start, function::PointerDifference(paranthese_end, paranthese_start) };
						offset = 0;
						if (dot != nullptr) {
							if (dot < new_line_character) {
								// Get the x, y, z, w specifier
								if (dot[1] == 'y') {
									offset = 4;
								}
								else if (dot[1] == 'z') {
									offset = 8;
								}
								else if (dot[1] == 'w') {
									offset = 12;
								}
							}
						}

						// The values are in strides of 16 bytes - 4 floats
						offset += (unsigned short)function::ConvertCharactersToInt(int_parse) * 16;
					}
				}
			}
		}

		return offset;
	}

	struct ShaderReflectionConstantBufferField {
		Stream<char> name;
		Stream<char> definition;
		Stream<char> tag;
		unsigned short basic_array_count;
		// Can be 1, 2, 3 or 4 - for matrix types
		unsigned char basic_component_count;
		Reflection::ReflectionBasicFieldType basic_type;
		// This is used to align fields that cross alignment boundaries
		unsigned char alignment;
		unsigned short pointer_offset;
		// Matrices can have only default values, not min and max values
		double4 default_value[4];
		double4 min_value;
		double4 max_value;
	};

	// It will parse the color tags as well
	void ParseShaderReflectionConstantBufferFieldTags(
		ShaderReflectionConstantBufferField* field,
		const char* semicolon,
		const char* new_line
	) {
		Stream<char> ignore_tag = "_";

		Stream<char> parse_range = { semicolon + 1, function::PointerDifference(new_line, semicolon + 1) / sizeof(char) };

		// Get the default value and mins first.
		// If it is a color afterwards we need to modify the default value

		Stream<char> current_token = STRING(ECS_REFLECT_RANGE);
		Stream<char> token = function::FindFirstToken(parse_range, current_token);
		if (token.size > 0) {
			// Consider this for integers and floats only that are vectors of at max 4 components
			if (field->basic_type != Reflection::ReflectionBasicFieldType::UserDefined && field->basic_component_count == 1) {
				token.Advance(current_token.size + 1);
				token.size--;

				ECS_STACK_CAPACITY_STREAM(double4, parsed_values, 2);
				function::ParseDouble4s(token, parsed_values, ',', ',', ignore_tag);
				field->min_value = parsed_values[0];
				field->max_value = parsed_values[1];
			}
		}

		current_token = STRING(ECS_REFLECT_DEFAULT);
		token = function::FindFirstToken(parse_range, current_token);
		if (token.size > 0) {
			token.Advance(current_token.size + 1);
			token.size--;

			CapacityStream<double4> default_values;
			default_values.InitializeFromBuffer(field->default_value, 0, 4);
			function::ParseDouble4s(token, default_values, ',', ',', ignore_tag);
			ECS_ASSERT(default_values.size == field->basic_component_count);
		}

		current_token = STRING(ECS_REFLECT_PARAMETERS);
		token = function::FindFirstToken(parse_range, current_token);
		if (token.size > 0) {
			// Consider this only for integers and floats that are vectors of at max 4 components
			if (field->basic_type != Reflection::ReflectionBasicFieldType::UserDefined && field->basic_component_count == 1) {
				token.Advance(current_token.size + 1);
				token.size--;

				ECS_STACK_CAPACITY_STREAM(double4, all_values, 3);
				function::ParseDouble4s(token, all_values, ',', ',', ignore_tag);
				field->default_value[0] = all_values[0];
				field->min_value = all_values[1];
				field->max_value = all_values[2];
			}
		}

		token = function::FindFirstToken(parse_range, STRING(ECS_REFLECT_AS_COLOR));
		if (token.size > 0) {
			// Consider this only if a float3 or float4
			if ((field->basic_type == Reflection::ReflectionBasicFieldType::Float3 || field->basic_type == Reflection::ReflectionBasicFieldType::Float4)
				&& field->basic_component_count == 1) {

				// If the field is a float3, then also add the float3 color tag
				if (field->basic_type == Reflection::ReflectionBasicFieldType::Float3) {
					field->tag = COLOR_FLOAT3_TAG;
				}

				field->basic_type = Reflection::ReflectionBasicFieldType::UserDefined;
				field->definition = STRING(Color);

				// Convert the default value, if any
				if (field->default_value[0].x != DBL_MAX) {
					if (field->default_value[0].x < 1.0 || field->default_value[0].y < 1.0 || field->default_value[0].z < 1.0 || field->default_value[0].w < 1.0) {
						// Bring the values into the 0 - 255 range
						double range = 255.0;
						field->default_value[0].x *= range;
						field->default_value[0].y *= range;
						field->default_value[0].z *= range;
						field->default_value[0].w *= range;
					}

					// We need to copy this value back into the field default value
					Color color_value = (double*)field->default_value;
					memcpy(field->default_value, &color_value, sizeof(Color));
				}
			}
		}
		
		token = function::FindFirstToken(parse_range, STRING(ECS_REFLECT_AS_FLOAT_COLOR));
		if (token.size > 0) {
			if ((field->basic_type == Reflection::ReflectionBasicFieldType::Float3 || field->basic_type == Reflection::ReflectionBasicFieldType::Float4)
				&& field->basic_component_count == 1) {
				if (field->basic_type == Reflection::ReflectionBasicFieldType::Float3) {
					field->tag = COLOR_FLOAT3_TAG;
				}

				field->basic_type = Reflection::ReflectionBasicFieldType::UserDefined;
				field->definition = STRING(ColorFloat);

				if (field->default_value[0].x != DBL_MAX) {
					ColorFloat color_float = (double*)field->default_value;
					memcpy(field->default_value, &color_float, sizeof(ColorFloat));
				}
			}
		}
	}

	// Returns true if it managed to determine the type of the field
	bool ParseShaderReflectionConstantBufferField(
		const ShaderReflection* shader_reflection, 
		ShaderReflectionConstantBufferField* field,
		const char* type_start, 
		const char* new_line,
		unsigned short* current_pointer_offset
	) {
		const char* type_end = function::SkipCodeIdentifier(type_start);
		Stream<char> type_identifier = { type_start, function::PointerDifference(type_end, type_start) / sizeof(char) };
		
		ShaderConstantBufferReflectionTypeMapping mapping;
		if (shader_reflection->cb_mapping_table.TryGetValue(type_identifier, mapping)) {
			field->basic_type = mapping.basic_type;
			field->basic_component_count = mapping.component_count;

			unsigned short basic_type_byte_size = Reflection::GetReflectionBasicFieldTypeByteSize(mapping.basic_type);
			unsigned short packoffset = ParsePackoffset(type_start, new_line);
			if (packoffset == USHORT_MAX) {
				// We need to align this field if it crosses the 16 byte alignment boundary
				field->pointer_offset = *current_pointer_offset;
				unsigned short upper_boundary = function::AlignPointerStack(field->pointer_offset, 16);
				if (field->pointer_offset + basic_type_byte_size > upper_boundary) {
					// Align it to the next boundary
					field->pointer_offset = upper_boundary;			
				}
			}
			else {
				field->pointer_offset = packoffset;
			}
			field->definition = Reflection::GetBasicFieldDefinition(mapping.basic_type);

			const char* name_start = function::SkipWhitespace(type_end);
			const char* name_end = function::SkipCodeIdentifier(name_start);
			field->name = { name_start, function::PointerDifference(name_end, name_start) / sizeof(char) };

			// Set DBL_MAX the value of the defaults, min and max
			field->max_value.x = DBL_MAX;
			field->min_value.x = DBL_MAX;
			for (unsigned char index = 0; index < 4; index++) {
				field->default_value[index].x = DBL_MAX;
			}

			// Determine the array count;
			field->basic_array_count = ParseBasicTypeArrayCount(name_end + 1, new_line);
			size_t basic_byte_size = (size_t)basic_type_byte_size * (size_t)mapping.component_count;
			size_t total_byte_size = field->basic_array_count == 0 ? basic_byte_size : basic_byte_size * field->basic_array_count;
			*current_pointer_offset = (unsigned short)(field->pointer_offset + total_byte_size);

			// Parse the default and min/max values
			const char* semicolon = strchr(name_end, ';');
			if (semicolon == nullptr) {
				return false;
			}

			ParseShaderReflectionConstantBufferFieldTags(field, semicolon, new_line);
			return true;
		}
		return false;
	}

	// Returns how many fields where used - there can be more fields used for example for matrix types
	unsigned int ConvertConstantBufferFieldToReflectionField(
		const ShaderReflectionConstantBufferField* shader_field, 
		Reflection::ReflectionField* reflection_fields,
		AllocatorPolymorphic allocator,
		CapacityStream<char>* matrix_field_name_storage,
		Stream<ShaderReflectionBufferMatrixField>* matrix_field
	) {
		if (matrix_field != nullptr && shader_field->basic_component_count > 1) {
			matrix_field->buffer[matrix_field->size].name.buffer = matrix_field_name_storage->buffer + matrix_field_name_storage->size;
			matrix_field->buffer[matrix_field->size].name.Copy(shader_field->name);
			matrix_field_name_storage->size += shader_field->name.size;
			matrix_field_name_storage->AssertCapacity();
		}
		reflection_fields[0].name = function::StringCopy(allocator, shader_field->name);

		reflection_fields[0].definition = function::StringCopy(allocator, shader_field->definition);
		reflection_fields[0].info.basic_type = shader_field->basic_type;
		reflection_fields[0].info.pointer_offset = shader_field->pointer_offset;
		reflection_fields[0].info.stream_type = shader_field->basic_array_count == 0 ? Reflection::ReflectionStreamFieldType::Basic : 
			Reflection::ReflectionStreamFieldType::BasicTypeArray;
		if (shader_field->basic_array_count == 0) {
			reflection_fields[0].info.basic_type_count = 1;
		}
		else {
			reflection_fields[0].info.basic_type_count = shader_field->basic_array_count;
		}

		if (shader_field->default_value[0].x != DBL_MAX) {
			memcpy(&reflection_fields[0].info.default_bool, shader_field->default_value, sizeof(double4));
			reflection_fields[0].info.has_default_value = true;
		}
		else {
			reflection_fields[0].info.has_default_value = false;
		}
		reflection_fields[0].info.stream_byte_size = 0;
		if (shader_field->basic_type == Reflection::ReflectionBasicFieldType::UserDefined) {
			if (function::CompareStrings(shader_field->definition, "Color")) {
				reflection_fields[0].info.byte_size = sizeof(Color);
			}
			else if (function::CompareStrings(shader_field->definition, "ColorFloat")) {
				reflection_fields[0].info.byte_size = sizeof(ColorFloat);
			}
			else {
				ECS_ASSERT(false);
			}
		}
		else {
			reflection_fields[0].info.byte_size = Reflection::GetReflectionBasicFieldTypeByteSize(shader_field->basic_type);
		}
		reflection_fields[0].tag = { nullptr, 0 };

		if (shader_field->basic_component_count > 1) {
			// A space and the integer value will be written
			size_t name_resize_size = reflection_fields[0].name.size + 2;

			Deallocate(allocator, reflection_fields[0].name.buffer);

			// Change the name for the first field as well
			reflection_fields[0].name.Initialize(allocator, name_resize_size);
			shader_field->name.CopyTo(reflection_fields[0].name.buffer);
			reflection_fields[0].name.size = shader_field->name.size;
			reflection_fields[0].name.Add(' ');
			function::ConvertIntToChars(reflection_fields[0].name, 0);

			// Copy the first field to all the other
			for (unsigned char index = 1; index < shader_field->basic_component_count; index++) {
				memcpy(reflection_fields + index, reflection_fields + 0, sizeof(reflection_fields[0]));
				reflection_fields[index].name.Initialize(allocator, name_resize_size);
				shader_field->name.CopyTo(reflection_fields[index].name.buffer);
				reflection_fields[index].name.size = shader_field->name.size;
				reflection_fields[index].name.Add(' ');
				function::ConvertIntToChars(reflection_fields[index].name, index);
				// Copy the definition such that on deallocation everything is homogeneous
				reflection_fields[index].definition = function::StringCopy(allocator, shader_field->definition);

				// Check to see if it has a default value
				if (shader_field->default_value[0].x != DBL_MAX) {
					memcpy(&reflection_fields[index].info.default_bool, shader_field->default_value + index, sizeof(double4));
				}
				reflection_fields[index].info.pointer_offset = shader_field->pointer_offset + index * reflection_fields[0].info.byte_size;
			}
		}
		else {
			ECS_STACK_CAPACITY_STREAM(char, range_tag, 512);

			auto write_values = [&](unsigned char basic_type_count, double4 values) {
				if (basic_type_count > 1) {
					range_tag.Add('{');
					double* double_values = (double*)&shader_field->min_value;
					for (unsigned char index = 0; index < basic_type_count - 1; index++) {
						function::ConvertDoubleToChars(range_tag, double_values[index], 5);
						range_tag.Add(',');
					}
					function::ConvertDoubleToChars(range_tag, double_values[basic_type_count - 1], 5);
					range_tag.Add('}');
				}
				else {
					function::ConvertDoubleToChars(range_tag, shader_field->min_value.x, 5);
				}
			};

			// See if it has any lower bounds/upper bound
			// Use a high precision of 5 when writing the doubles
			if (shader_field->min_value.x != DBL_MAX) {
				range_tag.Copy(STRING(ECS_UI_RANGE_REFLECT));
				range_tag.Add('(');
				unsigned char basic_type_count = Reflection::BasicTypeComponentCount(shader_field->basic_type);
				// Write the values as is
				write_values(basic_type_count, shader_field->min_value);
				range_tag.Add(',');

				if (shader_field->max_value.x != DBL_MAX) {
					write_values(basic_type_count, shader_field->max_value);
				}
				else {
					range_tag.Add('_');
				}
				range_tag.Add(')');
			}
			else if (shader_field->max_value.x != DBL_MAX) {
				range_tag.Copy(STRING(ECS_UI_RANGE_REFLECT));
				range_tag.Add('(');
				range_tag.Add('_');
				range_tag.Add(',');

				unsigned char basic_type_count = Reflection::BasicTypeComponentCount(shader_field->basic_type);
				write_values(basic_type_count, shader_field->max_value);

				range_tag.Add(')');
			}

			if (shader_field->tag.size > 0) {
				range_tag.Add(' ');
				range_tag.AddStream(shader_field->tag);
			}

			reflection_fields[0].tag = function::StringCopy(allocator, range_tag);
		}

		return shader_field->basic_component_count;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	Stream<char> SetName(const char* name, size_t size, AllocatorPolymorphic allocator) {
		return function::StringCopy(allocator, Stream<char>(name, size));
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	Stream<char> SetName(const char* starting_ptr, const char* ending_ptr, AllocatorPolymorphic allocator) {
		size_t name_size = ending_ptr - starting_ptr;
		return SetName(starting_ptr, name_size, allocator);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	unsigned short GetRegisterIndex(const char* register_ptr) {
		const char* number_start = strchr(register_ptr, '(');
		const char* number_end = strchr(number_start, ')');
		return function::ConvertCharactersToInt(Stream<char>(number_start, number_end - number_start));
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	// Returns whether or not the register is a default one
	bool ParseNameAndRegister(const char* type_start, AllocatorPolymorphic allocator, Stream<char>& output_name, unsigned short& register_index) {
		const char* current_character = type_start;
		// Get the buffer name
		while (*current_character != ' ' && *current_character != '\t') {
			current_character++;
		}
		while (*current_character == ' ' || *current_character == '\t') {
			current_character++;
		}
		const char* name_start = current_character;
		while (function::IsCodeIdentifierCharacter(*current_character)) {
			current_character++;
		}
		output_name = SetName(name_start, current_character, allocator);

		// Get the buffer register
		const char* register_ptr = strstr(current_character, REGISTER_STRING);
		if (register_ptr != nullptr) {
			register_index = GetRegisterIndex(register_ptr);
			return false;
		}
		return true;
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
		cb_mapping_table.InitializeFromBuffer(buffer, CB_TYPE_MAPPING_CAPACITY);

		InitializeCBTypeMappingTable();

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
	bool ReflectProperty(const ShaderReflection* reflection, Stream<wchar_t> path, Functor&& functor) {
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

	bool ShaderReflection::ReflectVertexShaderInput(Stream<wchar_t> path, CapacityStream<D3D11_INPUT_ELEMENT_DESC>& elements, AllocatorPolymorphic allocator) const
	{
		return ReflectProperty(this, path, [&](Stream<char> data) {
			return ReflectVertexShaderInputSource(data, elements, allocator);
		});
	}

	bool ShaderReflection::ReflectVertexShaderInputSource(Stream<char> source_code, CapacityStream<D3D11_INPUT_ELEMENT_DESC>& elements, AllocatorPolymorphic allocator) const
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
				elements[current_index].SemanticName = SetName(semantic_name, semantic_name_size, allocator).buffer;

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

			return true;
		}
		return false;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	// Returns whether or not it succeded
	bool ShaderReflection::ReflectShaderBuffers(
		Stream<wchar_t> path, 
		CapacityStream<ShaderReflectedBuffer>& buffers, 
		AllocatorPolymorphic allocator,
		ShaderReflectionBuffersOptions options
	) const {
		return ReflectProperty(this, path, [&](Stream<char> data) {
			return ReflectShaderBuffersSource(data, buffers, allocator, options);
		});
	}

	bool ShaderReflection::ReflectShaderBuffersSource(
		Stream<char> source_code, 
		CapacityStream<ShaderReflectedBuffer>& buffers, 
		AllocatorPolymorphic allocator,
		ShaderReflectionBuffersOptions options
	) const
	{
		// Make the last character \0 - it will be a non important character
		source_code[source_code.size - 1] = '\0';

		const char* omit_string = STRING(ECS_REFLECT_OMIT);
		size_t omit_string_size = strlen(omit_string);

		auto clean = [&]() {
			if (options.constant_buffer_reflection != nullptr) {
				ShaderReflectedBuffersDeallocate(buffers, allocator, *options.constant_buffer_reflection);
			}
			else {
				ShaderReflectedBuffersDeallocate(buffers, allocator);
			}
		};

		// Search for keywords
		// Cbuffer is a special keyword; don't include in the for loop
		Stream<char> current_parse_range = source_code;
		const char* cbuffer_ptr = strstr(source_code.buffer, BUFFER_KEYWORDS[ECS_SHADER_BUFFER_CONSTANT]);
		while (cbuffer_ptr != nullptr) {
			// Check for omit macro
			Stream<char> before_end_line = function::SkipUntilCharacterReverse(cbuffer_ptr, current_parse_range.buffer, '\n');
			before_end_line = function::SkipWhitespace(before_end_line);

			const char* end_bracket = function::FindMatchingParenthesis(cbuffer_ptr, source_code.buffer + source_code.size, '{', '}', 0);
			if (end_bracket == nullptr) {
				clean();
				return false;
			}
			Stream<char> parse_buffer_code = { cbuffer_ptr, function::PointerDifference(end_bracket, cbuffer_ptr) + 1 };

			if (memcmp(before_end_line.buffer, omit_string, omit_string_size) != 0) {
				Reflection::ReflectionType* reflection_type = nullptr;
				if (options.constant_buffer_reflection != nullptr) {
					ECS_ASSERT(options.constant_buffer_reflection->size < options.constant_buffer_reflection->capacity);
					reflection_type = options.constant_buffer_reflection->buffer + buffers.size;
				}
				Stream<ShaderReflectionBufferMatrixField>* matrix_type = nullptr;
				if (reflection_type != nullptr) {
					if (options.matrix_types_storage.buffer != nullptr && options.matrix_types != nullptr) {
						matrix_type = options.matrix_types + buffers.size;
						matrix_type->size = 0;
						matrix_type->buffer = options.matrix_types_storage.buffer + options.matrix_types_storage.size;
					}
				}
				buffers[buffers.size] = ReflectShaderStruct(this, parse_buffer_code, buffers.size, reflection_type, allocator, matrix_type, &options.matrix_type_name_storage);
				if (buffers[buffers.size].byte_size == -1) {
					clean();
					return false;
				}
				if (matrix_type != nullptr) {
					options.matrix_types_storage.size += matrix_type->size;
					options.matrix_types_storage.AssertCapacity();
				}
				buffers.size++;
			}

			current_parse_range.buffer = parse_buffer_code.buffer + parse_buffer_code.size;
			current_parse_range.size = function::PointerDifference(source_code.buffer + source_code.size, current_parse_range.buffer);
			cbuffer_ptr = strstr(current_parse_range.buffer, BUFFER_KEYWORDS[ECS_SHADER_BUFFER_CONSTANT]);
		}

		if (!options.only_constant_buffers) {
			size_t constant_buffer_count = buffers.size;
			for (size_t index = 1; index < std::size(BUFFER_KEYWORDS); index++) {
				current_parse_range = source_code;
				const char* current_buffer = strstr(current_parse_range.buffer, BUFFER_KEYWORDS[index]);
				while (current_buffer != nullptr) {
					const char* semicolon = strchr(current_buffer, ';');
					if (semicolon == nullptr) {
						clean();
						return false;
					}

					Stream<char> before_end_line = function::SkipUntilCharacterReverse(cbuffer_ptr, current_parse_range.buffer, '\n');
					before_end_line = function::SkipWhitespace(before_end_line);
					if (memcmp(before_end_line.buffer, omit_string, omit_string_size) != 0) {
						size_t current_index = buffers.size;
						// Byte size is zero - ignore for buffer types different from the constant buffer
						// The type is associated with the iteration index
						buffers[current_index].byte_size = 0;
						buffers[current_index].type = (ECS_SHADER_BUFFER_TYPE)index;

						Stream<char> name;
						unsigned short register_index;
						bool default_register = ParseNameAndRegister(current_buffer, allocator, name, register_index);
						buffers[current_index].name = name;
						buffers[current_index].register_index = default_register ? current_index - constant_buffer_count : register_index;

						buffers.size++;
					}

					current_parse_range = { semicolon, function::PointerDifference(source_code.buffer + source_code.size, semicolon) };
					current_buffer = strstr(current_parse_range.buffer, BUFFER_KEYWORDS[index]);
				}
			}
		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	// The assign functor receives as parameters the following
	//	size_t index - describes the index of the keyword that is currently matched
	//	Stream<char> name - already allocated
	//	unsigned short register_index - no need to check for defaulted value
	template<typename AssignFunctor>
	bool ReflectShaderBasicStructure(Stream<char> source_code, AllocatorPolymorphic allocator, Stream<const char*> keywords, AssignFunctor&& assign_functor) {
		// Make the last character \0 - it will be a non important character
		source_code[source_code.size - 1] = '\0';

		// Look for the sampler keywords
		Stream<char> initial_source_code = source_code;
		unsigned short current_register = 0;

		const char* omit_string = STRING(ECS_REFLECT_OMIT);
		size_t omit_string_size = strlen(omit_string);
		for (size_t index = 0; index < keywords.size; index++) {
			Stream<char> current_parse_range = source_code;
			Stream<char> keyword = function::FindFirstToken(current_parse_range, keywords[index]);
			while (keyword.size > 0) {
				// Check for omit macro
				Stream<char> before_end_line = function::SkipUntilCharacterReverse(keyword.buffer, current_parse_range.buffer, '\n');
				before_end_line = function::SkipWhitespace(before_end_line);
				// Get the semicolon. If it is missing then error
				Stream<char> semicolon = function::FindFirstCharacter(keyword, ';');
				if (semicolon.size == 0) {
					// An error
					return false;
				}

				if (memcmp(before_end_line.buffer, omit_string, omit_string_size) != 0) {
					Stream<char> name = { nullptr, 0 };
					unsigned short register_index = 0;
					bool default_value = ParseNameAndRegister(keyword.buffer, allocator, name, register_index);

					if (default_value) {
						register_index = current_register;
					}
					assign_functor(index, name, register_index);
					current_register++;
				}

				current_parse_range = semicolon;
				keyword = function::FindFirstToken(current_parse_range, keywords[index]);
			}
		}

		return true;
	}

	bool ShaderReflection::ReflectShaderTextures(Stream<wchar_t> path, CapacityStream<ShaderReflectedTexture>& textures, AllocatorPolymorphic allocator) const
	{
		return ReflectProperty(this, path, [&](Stream<char> data) {
			return ReflectShaderTexturesSource(data, textures, allocator);
		});
	}

	bool ShaderReflection::ReflectShaderTexturesSource(Stream<char> source_code, CapacityStream<ShaderReflectedTexture>& textures, AllocatorPolymorphic allocator) const
	{
		return ReflectShaderBasicStructure(source_code, allocator, { TEXTURE_KEYWORDS, std::size(TEXTURE_KEYWORDS) },
			[&](size_t index, Stream<char> name, unsigned short register_index) {
				textures.AddAssert({ name, (ECS_SHADER_TEXTURE_TYPE)index, register_index });
			}
		);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	bool ShaderReflection::ReflectShaderSamplers(Stream<wchar_t> path, CapacityStream<ShaderReflectedSampler>& samplers, AllocatorPolymorphic allocator) const 
	{
		return ReflectProperty(this, path, [&](Stream<char> data) {
			return ReflectShaderSamplersSource(data, samplers, allocator);
		});
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	bool ShaderReflection::ReflectShaderSamplersSource(Stream<char> source_code, CapacityStream<ShaderReflectedSampler>& samplers, AllocatorPolymorphic allocator) const 
	{
		return ReflectShaderBasicStructure(source_code, allocator, { SAMPLER_KEYWORDS, std::size(SAMPLER_KEYWORDS) },
			[&](size_t index, Stream<char> name, unsigned short register_index) {
				samplers.AddAssert({ name, register_index });
			}
		);

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	bool ShaderReflection::ReflectShaderMacros(
		Stream<char> source_code, 
		CapacityStream<Stream<char>>* defined_macros,
		CapacityStream<Stream<char>>* conditional_macros,
		AllocatorPolymorphic allocator
	) const
	{
		function::SourceCodeMacros macros;
		function::GetSourceCodeMacrosCTokens(&macros);
		macros.allocator = allocator;
		macros.defined_macros = defined_macros;
		macros.conditional_macros = conditional_macros;
		function::GetSourceCodeMacros(source_code, &macros);

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	bool ShaderReflection::ReflectVertexBufferMapping(Stream<wchar_t> path, CapacityStream<ECS_MESH_INDEX>& mapping) const
	{
		return ReflectProperty(this, path, [&](Stream<char> data) {
			return ReflectVertexBufferMappingSource(data, mapping);
		});
	}

	bool ShaderReflection::ReflectVertexBufferMappingSource(Stream<char> source_code, CapacityStream<ECS_MESH_INDEX>& mapping) const
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

	bool ShaderReflection::ReflectShader(Stream<char> source_code, const ReflectedShader* reflected_shader) const
	{
		bool has_input_layout_descriptor = false;
		bool has_input_layout_mapping = false;
		bool has_buffers = false;
		bool has_textures = false;
		bool has_samplers = false;
		bool has_macros = false;

		AllocatorPolymorphic main_allocator = reflected_shader->allocator;
		AllocatorPolymorphic macro_allocator = reflected_shader->macro_allocator;

		auto deallocate = [&]() {
			if (has_input_layout_descriptor) {
				ShaderInputLayoutDeallocate(*reflected_shader->input_layout_descriptor, main_allocator);
			}

			if (has_input_layout_mapping) {

			}

			if (has_buffers) {
				Stream<Reflection::ReflectionType> constant_buffers = reflected_shader->buffer_options.constant_buffer_reflection != nullptr ? 
					reflected_shader->buffer_options.constant_buffer_reflection->ToStream() : Stream<Reflection::ReflectionType>(nullptr, 0);
				ShaderReflectedBuffersDeallocate(*reflected_shader->buffers, main_allocator, constant_buffers);
			}

			if (has_textures) {
				ShaderReflectedTexturesDeallocate(*reflected_shader->textures, main_allocator);
			}

			if (has_samplers) {
				ShaderReflectedSamplersDeallocate(*reflected_shader->samplers, main_allocator);
			}

			if (has_macros) {

			}
		};

		if (reflected_shader->input_layout_descriptor != nullptr) {
			if (!ReflectVertexShaderInputSource(source_code, *reflected_shader->input_layout_descriptor, main_allocator)) {
				return false;
			}
			else {
				has_input_layout_descriptor = true;
			}
		}

		if (reflected_shader->input_layout_mapping != nullptr) {
			if (!ReflectVertexBufferMappingSource(source_code, *reflected_shader->input_layout_mapping)) {
				deallocate();
				return false;
			}
			else {
				has_input_layout_mapping = true;
			}
		}

		if (reflected_shader->buffers != nullptr) {
			if (!ReflectShaderBuffersSource(source_code, *reflected_shader->buffers, main_allocator, reflected_shader->buffer_options)) {
				deallocate();
				return false;
			}
			else {
				has_buffers = true;
			}
		}

		if (reflected_shader->textures != nullptr) {
			if (!ReflectShaderTexturesSource(source_code, *reflected_shader->textures, main_allocator)) {
				deallocate();
				return false;
			}
			else {
				has_textures = true;
			}
		}

		if (reflected_shader->samplers != nullptr) {
			if (!ReflectShaderSamplersSource(source_code, *reflected_shader->samplers, main_allocator)) {
				deallocate();
				return false;
			}
			else {
				has_samplers = true;
			}
		}

		if (reflected_shader->conditional_macros != nullptr || reflected_shader->defined_macros != nullptr) {
			if (!ReflectShaderMacros(source_code, reflected_shader->defined_macros, reflected_shader->conditional_macros, macro_allocator)) {
				deallocate();
				return false;
			}
			else {
				has_macros = true;
			}
		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	Stream<char> VERTEX_SHADER_TYPE_KEYWORDS[] = {
		STRING(SV_Position),
		STRING(SV_POSITION),
		STRING(VS_INPUT),
		STRING(VS_OUTPUT),
		STRING(SV_VertexID)
	};

	Stream<char> PIXEL_SHADER_TYPE_KEYWORDS[] = {
		STRING(SV_Target),
		STRING(SV_TARGET)
	};

	Stream<char> GEOMETRY_SHADER_TYPE_KEYWORDS[] = {
		STRING([maxvertexcount),
		STRING(TriangleStream),
		STRING(SV_GSInstanceID)
	};

	Stream<char> DOMAIN_SHADER_TYPE_KEYWORDS[] = {
		STRING([domain),
		STRING(OutputPatch),
		STRING(SV_DomainLocation)
	};

	Stream<char> HULL_SHADER_TYPE_KEYWORDS[] = {
		STRING(SV_TessFactor),
		STRING(SV_InsideTessFactor),
		STRING(InputPatch)
	};

	Stream<char> COMPUTE_SHADER_TYPE_KEYWORDS[] = {
		STRING([numthreads),
		STRING(SV_DispatchThreadID),
		STRING(SV_GroupID),
		STRING(SV_GroupIndex),
		STRING(SV_GroupThreadID)
	};

	Stream<Stream<char>> SHADER_TYPE_KEYWORDS[] = {
		{ VERTEX_SHADER_TYPE_KEYWORDS, std::size(VERTEX_SHADER_TYPE_KEYWORDS) },
		{ PIXEL_SHADER_TYPE_KEYWORDS, std::size(PIXEL_SHADER_TYPE_KEYWORDS) },
		{ DOMAIN_SHADER_TYPE_KEYWORDS, std::size(DOMAIN_SHADER_TYPE_KEYWORDS) },
		{ HULL_SHADER_TYPE_KEYWORDS, std::size(HULL_SHADER_TYPE_KEYWORDS) },
		{ GEOMETRY_SHADER_TYPE_KEYWORDS, std::size(GEOMETRY_SHADER_TYPE_KEYWORDS) },
		{ COMPUTE_SHADER_TYPE_KEYWORDS, std::size(COMPUTE_SHADER_TYPE_KEYWORDS) }
	};

	ECS_SHADER_TYPE ShaderReflection::DetermineShaderType(Stream<char> source_code) const
	{
		// Start from the compute shader downwards to the vertex shader because
		// the geometry shader, domain shader or hull shader can use the SV_Position
		// found in the vertex shader
		for (int64_t index = ECS_SHADER_TYPE_COUNT - 1; index >= 0; index--) {
			for (size_t subindex = 0; subindex < SHADER_TYPE_KEYWORDS[index].size; subindex++) {
				if (function::FindFirstToken(SHADER_TYPE_KEYWORDS[index][subindex], source_code).size > 0) {
					return (ECS_SHADER_TYPE)index;
				}
			}
		}
		return ECS_SHADER_TYPE_COUNT;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	void ECSEngine::ShaderReflection::InitializeCBTypeMappingTable()
	{
		ResourceIdentifier identifier;

		ShaderConstantBufferReflectionTypeMapping mapping;

#define ADD(type, reflection_type) mapping.basic_type = Reflection::ReflectionBasicFieldType::reflection_type; ADD_TO_FORMAT_TABLE(type, mapping, cb_mapping_table)

		// Scalar values + vector values
		mapping.component_count = 1;

		ADD(bool, Bool);
		ADD(int, Int32);
		ADD(uint, UInt32);
		ADD(float, Float);
		ADD(double, Double);
		ADD(bool2, Bool2);
		ADD(int2, Int2);
		ADD(uint2, UInt2);
		ADD(float2, Float2);
		ADD(double2, Double2);
		ADD(bool3, Bool3);
		ADD(int3, Int3);
		ADD(uint3, UInt3);
		ADD(float3, Float3);
		ADD(double3, Double3);
		ADD(bool4, Bool4);
		ADD(int4, Int4);
		ADD(uint4, UInt4);
		ADD(float4, Float4);
		ADD(double4, Double4);

#define ADD_MATRIX_TYPE(row_count)	ADD(bool##row_count##x1, Bool); \
									ADD(int##row_count##x1, Int32); \
									ADD(uint##row_count##x1, UInt32); \
									ADD(float##row_count##x1, Float); \
									ADD(double##row_count##x1, Double); \
									ADD(bool##row_count##x2, Bool2); \
									ADD(int##row_count##x2, Int2); \
									ADD(uint##row_count##x2, UInt2); \
									ADD(float##row_count##x2, Float2); \
									ADD(double##row_count##x2, Double2); \
									ADD(bool##row_count##x3, Bool3); \
									ADD(int##row_count##x3, Int3); \
									ADD(uint##row_count##x3, UInt3); \
									ADD(float##row_count##x3, Float3); \
									ADD(double##row_count##x3, Double3); \
									ADD(bool##row_count##x4, Bool4); \
									ADD(int##row_count##x4, Int4); \
									ADD(uint##row_count##x4, UInt4); \
									ADD(float##row_count##x4, Float4); \
									ADD(double##row_count##x4, Double4);

		// Matrix types
		ADD_MATRIX_TYPE(1);

		mapping.component_count = 2;
		ADD_MATRIX_TYPE(2);

		mapping.component_count = 3;
		ADD_MATRIX_TYPE(3);

		mapping.component_count = 4;
		ADD_MATRIX_TYPE(4);

#undef ADD_MATRIX_TYPE
#undef ADD

	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	size_t ShaderReflection::MemoryOf()
	{
		return ShaderReflectionFormatTable::MemoryOf(STRING_FORMAT_TABLE_CAPACITY) + ShaderReflectionFloatFormatTable::MemoryOf(FLOAT_FORMAT_TABLE_CAPACITY) +
			ShaderReflectionIntegerFormatTable::MemoryOf(INTEGER_FORMAT_TABLE_CAPACITY) * 2 + ShaderReflectionCBTypeMapping::MemoryOf(CB_TYPE_MAPPING_CAPACITY);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	ShaderReflectedBuffer ReflectShaderStruct(
		const ShaderReflection* shader_reflection, 
		Stream<char> source,
		unsigned int default_slot_position,
		Reflection::ReflectionType* reflection_type, 
		AllocatorPolymorphic allocator,
		Stream<ShaderReflectionBufferMatrixField>* matrix_types,
		CapacityStream<char>* matrix_type_name_storage
	)
	{
		ShaderReflectedBuffer reflected_buffer;

		ECS_ASSERT(reflection_type == nullptr || allocator.allocator != nullptr);

		reflected_buffer.type = ECS_SHADER_BUFFER_COUNT;

		const char* starting_pointer = source.buffer;
		size_t cbuffer_size = strlen(BUFFER_KEYWORDS[0]);
		if (memcmp(starting_pointer, BUFFER_KEYWORDS[ECS_SHADER_BUFFER_CONSTANT], cbuffer_size) == 0) {
			// Get buffer name
			starting_pointer += cbuffer_size + 1;
			reflected_buffer.type = ECS_SHADER_BUFFER_CONSTANT;
		}
		else {
			size_t struct_size = strlen("struct");
			if (memcmp(starting_pointer, "struct", struct_size * sizeof(char)) == 0) {
				starting_pointer += struct_size + 1;
			}
		}

		while (!function::IsAlphabetCharacter(*starting_pointer)) {
			starting_pointer++;
		}
		const char* name_start = starting_pointer;
		while (function::IsCodeIdentifierCharacter(*starting_pointer)) {
			starting_pointer++;
		}

		// Name
		reflected_buffer.name = SetName(name_start, starting_pointer, allocator);

		// Register index
		char* end_bracket = (char*)function::FindMatchingParenthesis(starting_pointer, source.buffer + source.size, '{', '}', 0);
		*end_bracket = '\0';

		const char* register_ptr = strstr(starting_pointer, REGISTER_STRING);
		if (register_ptr == nullptr) {
			reflected_buffer.register_index = default_slot_position;
		}
		else {
			reflected_buffer.register_index = GetRegisterIndex(register_ptr);
		}

		const char* end_line = strchr(starting_pointer, '\n');
		const char* current_character = end_line + 1;
		current_character += *current_character == '{';

		ECS_STACK_CAPACITY_STREAM(Reflection::ReflectionField, reflected_fields, 64);

		// byte size
		unsigned short total_byte_size = 0;
		unsigned short current_offset = 0;
		const char* next_new_line = strchr(current_character + 1, '\n');
		while (current_character != nullptr && next_new_line != nullptr) {
			current_character += *current_character == '\n';
			current_character = function::SkipWhitespace(current_character);

			const char* start_type_ptr = current_character;
			current_character = function::SkipCodeIdentifier(current_character);
			Stream<char> identifier = { start_type_ptr, function::PointerDifference(current_character, start_type_ptr) };

			ShaderConstantBufferReflectionTypeMapping mapping;
			if (shader_reflection->cb_mapping_table.TryGetValue(identifier, mapping)) {
				size_t byte_size = Reflection::GetReflectionBasicFieldTypeByteSize(mapping.basic_type) * (size_t)mapping.component_count;
				total_byte_size += (unsigned short)byte_size;

				if (reflection_type != nullptr) {
					// We need to record other properties
					ShaderReflectionConstantBufferField buffer_field;
					ParseShaderReflectionConstantBufferField(shader_reflection, &buffer_field, identifier.buffer, next_new_line, &current_offset);
					unsigned int previous_offset = buffer_field.pointer_offset;
					unsigned int field_count = ConvertConstantBufferFieldToReflectionField(
						&buffer_field, 
						reflected_fields.buffer + reflected_fields.size, 
						allocator, 
						matrix_type_name_storage, 
						matrix_types
					); 
					if (field_count > 1) {
						if (matrix_types != nullptr) {
							matrix_types->buffer[matrix_types->size].position = { reflected_fields.size, field_count };
							matrix_types->size++;
						}
					}
					reflected_fields.size += field_count;
					total_byte_size = 0;
				}
				else {
					unsigned short parsed_offset = ParsePackoffset(identifier.buffer, next_new_line);
					if (parsed_offset != USHORT_MAX) {
						current_offset = parsed_offset;
						total_byte_size = byte_size;
					}
				}

				current_character = (char*)next_new_line;
				next_new_line = strchr(current_character + 1, '\n');
			}
			else {
				reflected_buffer.byte_size = -1;
				return reflected_buffer;
			}
		}
		*end_bracket = '}';
		reflected_buffer.byte_size = current_offset == 0 ? total_byte_size : current_offset + total_byte_size;
		if (reflection_type != nullptr) {
			reflection_type->fields.InitializeAndCopy(allocator, reflected_fields);
			reflection_type->name.InitializeAndCopy(allocator, reflected_buffer.name);
			reflection_type->tag = { nullptr, 0 };
			reflection_type->evaluations = { nullptr, 0 };
			reflection_type->alignment = 8;
			reflection_type->byte_size = reflected_buffer.byte_size;
			// At the moment it is blittable
			reflection_type->is_blittable = true;
			reflection_type->folder_hierarchy_index = -1;
		}

		return reflected_buffer;
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	void ShaderInputLayoutDeallocate(Stream<D3D11_INPUT_ELEMENT_DESC> elements, AllocatorPolymorphic allocator)
	{
		for (size_t index = 0; index < elements.size; index++) {
			DeallocateIfBelongs(allocator, elements[index].SemanticName);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	void ShaderConstantBufferReflectionTypeCopy(
		const Reflection::ReflectionType* reflection_type,
		Reflection::ReflectionType* copy,
		AllocatorPolymorphic allocator
	) {
		copy->fields.Initialize(allocator, reflection_type->fields.size);
		copy->name.InitializeAndCopy(allocator, reflection_type->name);
		if (reflection_type->tag.size > 0) {
			copy->tag.InitializeAndCopy(allocator, reflection_type->tag);
		}
		else {
			copy->tag = { nullptr, 0 };
		}

		for (size_t index = 0; index < reflection_type->fields.size; index++) {
			memcpy(copy->fields.buffer + index, reflection_type->fields.buffer + index, sizeof(Reflection::ReflectionField));
			copy->fields[index].name.InitializeAndCopy(allocator, reflection_type->fields[index].name);
			copy->fields[index].definition.InitializeAndCopy(allocator, reflection_type->fields[index].definition);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	void ShaderConstantBufferReflectionTypeDeallocate(
		const Reflection::ReflectionType* reflection_type,
		AllocatorPolymorphic allocator
	) {
		for (size_t index = 0; index < reflection_type->fields.size; index++) {
			Deallocate(allocator, reflection_type->fields[index].name.buffer);
			Deallocate(allocator, reflection_type->fields[index].definition.buffer);
			if (reflection_type->fields[index].tag.size > 0) {
				Deallocate(allocator, reflection_type->fields[index].tag.buffer);
			}
		}

		Deallocate(allocator, reflection_type->name.buffer);
		Deallocate(allocator, reflection_type->fields.buffer);
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	void ShaderReflectedTexturesDeallocate(
		Stream<ShaderReflectedTexture> textures,
		AllocatorPolymorphic allocator
	) {
		if (textures.size > 0) {
			for (size_t index = 0; index < textures.size; index++) {
				Deallocate(allocator, textures[index].name.buffer);
			}
			DeallocateIfBelongs(allocator, textures.buffer);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	void ShaderReflectedSamplersDeallocate(
		Stream<ShaderReflectedSampler> samplers,
		AllocatorPolymorphic allocator
	) {
		if (samplers.size > 0) {
			for (size_t index = 0; index < samplers.size; index++) {
				Deallocate(allocator, samplers[index].name.buffer);
			}
			DeallocateIfBelongs(allocator, samplers.buffer);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

	void ShaderReflectedBuffersDeallocate(
		Stream<ShaderReflectedBuffer> buffers,
		AllocatorPolymorphic allocator,
		Stream<Reflection::ReflectionType> reflection_types
	) {
		for (size_t index = 0; index < buffers.size; index++) {
			Deallocate(allocator, buffers[index].name.buffer);
		}
		for (size_t index = 0; index < reflection_types.size; index++) {
			ShaderConstantBufferReflectionTypeDeallocate(reflection_types.buffer + index, allocator);
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------------------------

}