#include "ecspch.h"
#include "ShaderReflection.h"
#include "../Utilities/FunctionInterfaces.h"

constexpr size_t FORMAT_TABLE_CAPACITY = 128;
constexpr size_t TYPE_TABLE_CAPACITY = 256;

constexpr const char* VERTEX_SHADER_INPUT_STRUCTURE_NAME = "VS_INPUT";
constexpr const char* REGISTER_STRING = "register";

constexpr const char* BUFFER_KEYWORDS[] = {
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

constexpr const char* TEXTURE_KEYWORDS[] = {
	"Texture1D",
	"Texture2D",
	"Texture3D",
	"Texture1DArray",
	"Texture2DArray",
	"Texture3DArray",
	"TextureCube"
};

using Hash = ECSEngine::HashFunctionAdditiveString;

namespace ECSEngine {

#define ADD_TO_FORMAT_TABLE(format) identifier.ptr = #format; identifier.size = strlen(#format); \
hash = Hash::Hash(identifier); \
ECS_ASSERT(!format_table.Insert(hash, format, identifier));

#define ADD_TO_TYPE_TABLE(type, byte_size) identifier.ptr = #type; identifier.size = strlen(#type); \
hash = Hash::Hash(identifier); \
ECS_ASSERT(!type_table.Insert(hash, byte_size, identifier));

#define ADD_BASIC_TYPE(type, byte_size) ADD_TO_TYPE_TABLE(type, byte_size); \
ADD_TO_TYPE_TABLE(type##1, byte_size); \
ADD_TO_TYPE_TABLE(type##2, byte_size * 2); \
ADD_TO_TYPE_TABLE(type##3, byte_size * 3); \
ADD_TO_TYPE_TABLE(type##4, byte_size * 4); \
ADD_TO_TYPE_TABLE(type##1x1, byte_size); \
ADD_TO_TYPE_TABLE(type##1x2, byte_size * 2); \
ADD_TO_TYPE_TABLE(type##1x3, byte_size * 3); \
ADD_TO_TYPE_TABLE(type##1x4, byte_size * 4); \
ADD_TO_TYPE_TABLE(type##2x1, byte_size * 2); \
ADD_TO_TYPE_TABLE(type##2x2, byte_size * 4); \
ADD_TO_TYPE_TABLE(type##2x3, byte_size * 6); \
ADD_TO_TYPE_TABLE(type##2x4, byte_size * 8); \
ADD_TO_TYPE_TABLE(type##3x1, byte_size * 3); \
ADD_TO_TYPE_TABLE(type##3x2, byte_size * 6); \
ADD_TO_TYPE_TABLE(type##3x3, byte_size * 9); \
ADD_TO_TYPE_TABLE(type##3x4, byte_size * 12); \
ADD_TO_TYPE_TABLE(type##4x1, byte_size * 4); \
ADD_TO_TYPE_TABLE(type##4x2, byte_size * 8); \
ADD_TO_TYPE_TABLE(type##4x3, byte_size * 12); \
ADD_TO_TYPE_TABLE(type##4x4, byte_size * 16); 


	Stream<char> SetName(char* starting_ptr, char* ending_ptr, CapacityStream<char>& name_pool) {
		size_t name_size = ending_ptr - starting_ptr;
		char* current_name = name_pool.buffer + name_pool.size;
		memcpy(current_name, starting_ptr, name_size);
		current_name[name_size] = '\0';
		name_pool.size += name_size + 1;
		return { current_name, name_size };
	}

	Stream<char> SetName(char* name, size_t size, CapacityStream<char>& name_pool) {
		char* current_name = name_pool.buffer + name_pool.size;
		memcpy(current_name, name, size);
		current_name[size] = '\0';
		name_pool.size += size + 1;
		return { current_name, size };
	}

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
		return function::ConvertCharactersToInt<unsigned short>(Stream<char>(number_start, number_end - number_start));
	}

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
		while (function::IsAlphabetCharacter(*current_character) || function::IsNumberCharacter(*current_character) || *current_character == '_') {
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

	ShaderReflection::ShaderReflection() {}

	ShaderReflection::ShaderReflection(void* allocation)
	{
		uintptr_t buffer = (uintptr_t)allocation;
		format_table.InitializeFromBuffer(buffer, FORMAT_TABLE_CAPACITY);
		type_table.InitializeFromBuffer(buffer, TYPE_TABLE_CAPACITY);

		ResourceIdentifier identifier;
		unsigned int hash;

		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_UNKNOWN);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32G32B32A32_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32G32B32A32_FLOAT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32G32B32A32_UINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32G32B32A32_SINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32G32B32_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32G32B32_FLOAT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32G32B32_UINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32G32B32_SINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16G16B16A16_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16G16B16A16_FLOAT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16G16B16A16_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16G16B16A16_UINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16G16B16A16_SNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16G16B16A16_SINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32G32_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32G32_FLOAT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32G32_UINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32G32_SINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32G8X24_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_X32_TYPELESS_G8X24_UINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R10G10B10A2_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R10G10B10A2_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R10G10B10A2_UINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R11G11B10_FLOAT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R8G8B8A8_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R8G8B8A8_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R8G8B8A8_UINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R8G8B8A8_SNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R8G8B8A8_SINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16G16_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16G16_FLOAT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16G16_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16G16_UINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16G16_SNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16G16_SINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_D32_FLOAT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32_FLOAT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32_UINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R32_SINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R24G8_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_D24_UNORM_S8_UINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R24_UNORM_X8_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_X24_TYPELESS_G8_UINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R8G8_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R8G8_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R8G8_UINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R8G8_SNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R8G8_SINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16_FLOAT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_D16_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16_UINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16_SNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R16_SINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R8_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R8_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R8_UINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R8_SNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R8_SINT);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_A8_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R1_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R9G9B9E5_SHAREDEXP);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R8G8_B8G8_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_G8R8_G8B8_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC1_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC1_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC1_UNORM_SRGB);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC2_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC2_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC2_UNORM_SRGB);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC3_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC3_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC3_UNORM_SRGB);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC4_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC4_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC4_SNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC5_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC5_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC5_SNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_B5G6R5_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_B5G5R5A1_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_B8G8R8A8_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_B8G8R8X8_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_B8G8R8A8_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_B8G8R8X8_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_B8G8R8X8_UNORM_SRGB);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC6H_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC6H_UF16);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC6H_SF16);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC7_TYPELESS);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC7_UNORM);
		ADD_TO_FORMAT_TABLE(DXGI_FORMAT_BC7_UNORM_SRGB);

		ADD_BASIC_TYPE(float, 4);
		ADD_BASIC_TYPE(bool, 4);
		ADD_BASIC_TYPE(int, 4);
		ADD_BASIC_TYPE(uint, 4);
		ADD_BASIC_TYPE(double, 8)
	}

	bool ShaderReflection::ReflectVertexShaderInput(const wchar_t* path, CapacityStream<D3D11_INPUT_ELEMENT_DESC>& elements, CapacityStream<char> semantic_name_pool)
	{
		return ReflectVertexShaderInput(ToStream(path), elements, semantic_name_pool);
	}

	bool ShaderReflection::ReflectVertexShaderInput(Stream<wchar_t> path, CapacityStream<D3D11_INPUT_ELEMENT_DESC>& elements, CapacityStream<char> semantic_name_pool)
	{
		std::ifstream stream(std::wstring(path.buffer, path.size));

		if (stream.good()) {
			// Read the whole file
			size_t byte_size = function::GetFileByteSize(stream);
			char* allocation = (char*)_malloca(byte_size);
			stream.read(allocation, byte_size);

			// Make sure to make \0 the last read character
			// since read count may differ from byte size
			size_t read_count = stream.gcount();
			allocation[read_count] = '\0';

			char* structure_name = strstr(allocation, VERTEX_SHADER_INPUT_STRUCTURE_NAME);
			if (structure_name != nullptr) {
				// Iterate through all of the input's fields and fill the information
				while (*structure_name != '\n') {
					structure_name++;
				}
				structure_name++;

				char* last_character = structure_name;
				last_character = (char*)function::PredicateValue<uintptr_t>(*last_character == '{', (uintptr_t)last_character + 2, (uintptr_t)last_character);
				while (*last_character != '}') {
					size_t current_index = elements.size;

					// make the end line character /0 for C functions
					char* end_line = strchr(last_character, '\n');
					if (end_line == nullptr) {
						_freea(allocation);
						return false;
					}
					*end_line = '\0';

					// advance till the colon : to get the semantic
					char* current_character = last_character;
					current_character = strchr(current_character, ':');
					if (current_character == nullptr) {
						_freea(allocation);
						return false;
					}

					current_character++;
					while (*current_character == ' ' || *current_character == '\t') {
						current_character++;
					}
					char* semantic_name = current_character;
					while (function::IsCodeIdentifierCharacter(*current_character)) {
						current_character++;
					}
					size_t semantic_name_size = current_character - semantic_name;

					// Get the format
					while (*current_character == '\t' || *current_character == ' ') {
						current_character++;
					}

					current_character = strstr(current_character, STRING(ECS_REFLECT_FORMAT));
					if (current_character == nullptr) {
						_freea(allocation);
						return false;
					}

					current_character += strlen(STRING(ECS_REFLECT_FORMAT)) + 1;
					char* parenthese = current_character + 1;
					while (*parenthese != ')') {
						parenthese++;
					}

					// Format
					ResourceIdentifier identifier(current_character, parenthese - current_character);
					unsigned int hash = Hash::Hash(identifier);
					DXGI_FORMAT format;
					bool success = format_table.TryGetValue(hash, identifier, format);
					if (!success) {
						_freea(allocation);
						return false;
					}
					elements[current_index].Format = format;
					// Aligned offset
					elements[current_index].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;

					// Semantic name
					elements[current_index].SemanticName = SetName(semantic_name, semantic_name_size, semantic_name_pool).buffer;

					// Input slot
					char* input_slot = strstr(parenthese, STRING(ECS_REFLECT_INPUT_SLOT));
					if (input_slot != nullptr) {
						input_slot += strlen(STRING(ECS_REFLECT_INPUT_SLOT)) + 2;
						char* starting_input_slot = input_slot;
						while (function::IsNumberCharacter(*input_slot)) {
							input_slot++;
						}
						elements[current_index].InputSlot = function::ConvertCharactersToInt<size_t>(Stream<char>(starting_input_slot, input_slot - starting_input_slot));
					}
					else {
						elements[current_index].InputSlot = 0;
					}

					// Instance
					char* instance = strstr(parenthese, STRING(ECS_REFLECT_INSTANCE));
					if (instance == nullptr) {
						elements[current_index].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
						elements[current_index].InstanceDataStepRate = 0;
					}
					else {
						elements[current_index].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
						instance += strlen(STRING(ECS_REFLECT_INSTANCE)) + 2;
						char* instance_step_rate_start = instance;
						while (function::IsNumberCharacter(*instance)) {
							instance++;
						}
						elements[current_index].InstanceDataStepRate = function::ConvertCharactersToInt<size_t>(Stream<char>(instance_step_rate_start, instance - instance_step_rate_start));
					}

					// Semantic count
					char* semantic_count = strstr(parenthese, STRING(ECS_REFLECT_SEMANTIC_COUNT));
					if (semantic_count == nullptr) {
						elements[current_index].SemanticIndex = 0;
						elements.size++;
						elements.AssertCapacity();
					}
					else {
						semantic_count += strlen(STRING(ECS_REFLECT_SEMANTIC_COUNT)) + 2;
						char* semantic_count_start = semantic_count;
						while (function::IsNumberCharacter(*semantic_count)) {
							semantic_count++;
						}
						size_t count = function::ConvertCharactersToInt<size_t>(Stream<char>(semantic_count_start, semantic_count - semantic_count_start));
						elements.size += count;
						elements.AssertCapacity();
						elements[current_index].SemanticIndex = 0;
						for (size_t index = 1; index < count; index++) {
							memcpy(elements.buffer + current_index + index, elements.buffer + current_index, sizeof(D3D11_INPUT_ELEMENT_DESC));
							elements[current_index + index].SemanticIndex = index;
						}
					}

					last_character = end_line + 1;
				}
				semantic_name_pool.AssertCapacity();

				_freea(allocation);
				return true;
			}

			_freea(allocation);
			return false;
		}
		return false;
	}

	// Returns whether or not it succeded
	bool ShaderReflection::ReflectShaderBuffers(const wchar_t* path, CapacityStream<ShaderReflectedBuffer>& buffers, CapacityStream<char> name_pool) {
		return ReflectShaderBuffers(ToStream(path), buffers, name_pool);
	}

	// Returns whether or not it succeded
	bool ShaderReflection::ReflectShaderBuffers(Stream<wchar_t> path, CapacityStream<ShaderReflectedBuffer>& buffers, CapacityStream<char> name_pool) {
		std::ifstream stream(std::wstring(path.buffer, path.size));

		if (stream.good()) {
			// Read the whole file
			size_t file_size = function::GetFileByteSize(stream);
			char* allocation = (char*)_malloca(file_size);
			stream.read(allocation, file_size);

			// Make sure to make \0 the last read character
			// since read count may differ from byte size
			size_t read_count = stream.gcount();
			allocation[read_count] = '\0';

			// Search for keywords
			// Cbuffer is a special keyword; don't include in the for loop
			char* cbuffer_ptr = strstr(allocation, BUFFER_KEYWORDS[0]);
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

					ResourceIdentifier identifier(start_type_ptr, current_character - start_type_ptr);
					unsigned int hash = Hash::Hash(identifier);
					size_t byte_size;
					if (!type_table.TryGetValue(hash, identifier, byte_size)) {
						_freea(allocation);
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
				char* current_buffer = strstr(allocation, BUFFER_KEYWORDS[index]);
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
					buffers[current_index].register_index = function::PredicateValue<unsigned int>(default_register, current_index - constant_buffer_count, register_index);

					buffers.size++;
					*end_line = '\n';
					current_buffer = strstr(end_line + 1, BUFFER_KEYWORDS[index]);
				}
			}

			name_pool.AssertCapacity();
			_freea(allocation);
			return true;
		}
		return false;
	}

	bool ShaderReflection::ReflectShaderTextures(const wchar_t* path, containers::CapacityStream<ShaderReflectedTexture>& textures, containers::CapacityStream<char> name_pool)
	{
		return ReflectShaderTextures(ToStream(path), textures, name_pool);
	}

	bool ShaderReflection::ReflectShaderTextures(containers::Stream<wchar_t> path, containers::CapacityStream<ShaderReflectedTexture>& textures, containers::CapacityStream<char> name_pool)
	{
		std::ifstream stream(std::wstring(path.buffer, path.size));

		if (stream.good()) {
			// Read the whole file
			size_t file_size = function::GetFileByteSize(stream);
			char* allocation = (char*)_malloca(file_size);
			stream.read(allocation, file_size);

			// Make sure to make \0 the last read character
			// since read count may differ from byte size
			size_t read_count = stream.gcount();
			allocation[read_count] = '\0';

			// Linearly iterate through all keywords and add each texture accordingly
			for (size_t index = 0; index < std::size(TEXTURE_KEYWORDS); index++) {
				char* texture_ptr = strstr(allocation, TEXTURE_KEYWORDS[index]);
				while (texture_ptr != nullptr) {
					char* end_line = strchr(texture_ptr, '\n');
					*end_line = '\0';

					Stream<char> name;
					unsigned short register_index;
					bool default_value = ParseNameAndRegister(texture_ptr, name_pool, name, register_index);
					
					size_t current_index = textures.size;
					textures[current_index].name = name;
					textures[current_index].type = (ShaderTextureType)index;
					textures[current_index].register_index = function::PredicateValue<unsigned short>(default_value, current_index, register_index);

					*end_line = '\n';
					texture_ptr = strstr(end_line + 1, TEXTURE_KEYWORDS[index]);
					textures.size++;
				}
			}

			_freea(allocation);
			return true;
		}
		return false;
	}

	size_t ShaderReflection::MemoryOf()
	{
		return ShaderReflectionFormatTable::MemoryOf(FORMAT_TABLE_CAPACITY) + ShaderReflectionTypeTable::MemoryOf(TYPE_TABLE_CAPACITY);
	}

}