// ECS_REFLECT
#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Containers/HashTable.h"
#include "../Internal/InternalStructures.h"
#include "RenderingStructures.h"
#include "ecspch.h"
#include "../Utilities/Reflection/ReflectionMacros.h"

namespace ECSEngine {

	enum ShaderReflectionFloatFormatTableType : unsigned char {
		ECS_SHADER_REFLECTION_FLOAT,
		ECS_SHADER_REFLECTION_UNORM_8,
		ECS_SHADER_REFLECTION_SNORM_8,
		ECS_SHADER_REFLECTION_UNORM_16,
		ECS_SHADER_REFLECTION_SNORM_16,
		ECS_SHADER_REFLECTION_FLOAT_TABLE_COUNT
	};

	enum ShaderReflectionSignedIntegerFormatTableType : unsigned char {
		ECS_SHADER_REFLECTION_SINT_8,
		ECS_SHADER_REFLECTION_SINT_16,
		ECS_SHADER_REFLECTION_SINT_32
	};

	enum ShaderReflectionUnsignedIntegerFormatTableType : unsigned char {
		ECS_SHADER_REFLECTION_UINT_8,
		ECS_SHADER_REFLECTION_UINT_16,
		ECS_SHADER_REFLECTION_UINT_32,
		ECS_SHADER_REFLECTION_INTEGER_TABLE_COUNT
	};

	struct ShaderReflectionFloatExtendedFormat {
		DXGI_FORMAT formats[ECS_SHADER_REFLECTION_FLOAT_TABLE_COUNT];
	};

	struct ShaderReflectionIntegerExtendedFormat {
		DXGI_FORMAT formats[ECS_SHADER_REFLECTION_INTEGER_TABLE_COUNT];
	};

	using ShaderReflectionFormatTable = HashTableDefault<DXGI_FORMAT>;;
	using ShaderReflectionFloatFormatTable = HashTableDefault<ShaderReflectionFloatExtendedFormat>;
	using ShaderReflectionIntegerFormatTable = HashTableDefault<ShaderReflectionIntegerExtendedFormat>;

	enum ECS_REFLECT ECS_SHADER_BUFFER_TYPE : unsigned char {
		ECS_SHADER_BUFFER_CONSTANT,
		ECS_SHADER_BUFFER_TEXTURE,
		ECS_SHADER_BUFFER_READ,
		ECS_SHADER_BUFFER_READ_WRITE,
		ECS_SHADER_BUFFER_READ_WRITE_BYTE_ADDRESS,
		ECS_SHADER_BUFFER_STRUCTURED,
		ECS_SHADER_BUFFER_READ_WRITE_STRUCTURED,
		ECS_SHADER_BUFFER_APPEND,
		ECS_SHADER_BUFFER_CONSUME
	};

	// Byte size is useful only for constant buffers in order to create them directly
	// on the Graphics object
	struct ShaderReflectedBuffer {
		Stream<char> name;
		unsigned int byte_size;
		ECS_SHADER_BUFFER_TYPE type;
		unsigned short register_index;
	};

	enum ECS_REFLECT ECS_SHADER_TEXTURE_TYPE : unsigned char {
		ECS_SHADER_TEXTURE_READ_1D,
		ECS_SHADER_TEXTURE_READ_2D,
		ECS_SHADER_TEXTURE_READ_3D,
		ECS_SHADER_TEXTURE_READ_WRITE_1D,
		ECS_SHADER_TEXTURE_READ_WRITE_2D,
		ECS_SHADER_TEXTURE_READ_WRITE_3D,
		ECS_SHADER_TEXTURE_ARRAY_1D,
		ECS_SHADER_TEXTURE_ARRAY_2D,
		ECS_SHADER_TEXTURE_ARRAY_3D,
		ECS_SHADER_TEXTURE_CUBE
	};

	struct ShaderReflectedTexture {
		Stream<char> name;
		ECS_SHADER_TEXTURE_TYPE type;
		unsigned short register_index;
	};

	struct ECSENGINE_API ShaderReflection {
		ShaderReflection();
		ShaderReflection(void* allocation);

		ShaderReflection(const ShaderReflection& other) = default;
		ShaderReflection& operator = (const ShaderReflection& other) = default;

		// Returns whether or not it succeded
		bool ReflectVertexShaderInput(Stream<wchar_t> path, CapacityStream<D3D11_INPUT_ELEMENT_DESC>& elements, CapacityStream<char> semantic_name_pool);

		// Returns whether or not it succeded
		bool ReflectVertexShaderInputSource(Stream<char> source_code, CapacityStream<D3D11_INPUT_ELEMENT_DESC>& elements, CapacityStream<char> semantic_name_pool);;

		// Returns whether or not it succeded
		bool ReflectShaderBuffers(Stream<wchar_t> path, CapacityStream<ShaderReflectedBuffer>& buffers, CapacityStream<char> name_pool);

		// Returns whether or not it succeded
		bool ReflectShaderBuffersSource(Stream<char> source_code, CapacityStream<ShaderReflectedBuffer>& buffers, CapacityStream<char> name_pool);

		// Returns whether or not it succeded
		bool ReflectShaderTextures(Stream<wchar_t> path, CapacityStream<ShaderReflectedTexture>& textures, CapacityStream<char> name_pool);

		// Returns whether or not it succeded
		bool ReflectShaderTexturesSource(Stream<char> source_code, CapacityStream<ShaderReflectedTexture>& textures, CapacityStream<char> name_pool);

		// Returns whether or not it succeded
		bool ReflectVertexBufferMapping(Stream<wchar_t> path, CapacityStream<ECS_MESH_INDEX>& mapping);

		// Returns whether or not it succeded
		bool ReflectVertexBufferMappingSource(Stream<char> source_code, CapacityStream<ECS_MESH_INDEX>& mapping);

		// Returns the amount of bytes necessary to create an instance of this class
		static size_t MemoryOf();

		ShaderReflectionFormatTable string_table;
		ShaderReflectionFloatFormatTable float_table;
		ShaderReflectionIntegerFormatTable unsigned_table;
		ShaderReflectionIntegerFormatTable signed_table;
	};

}