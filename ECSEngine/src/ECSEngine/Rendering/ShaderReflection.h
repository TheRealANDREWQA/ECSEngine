#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Containers/HashTable.h"
#include "../Internal/InternalStructures.h"
#include "RenderingStructures.h"
#include "ecspch.h"

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

	enum class ShaderBufferType {
		Constant,
		Texture,
		Read,
		ReadWrite,
		ByteAddress,
		ReadWriteByteAddress,
		Structured,
		ReadWriteStructured,
		Append,
		Consume
	};

	// Byte size is useful only for constant buffers in order to create them directly
	// on the Graphics object
	struct ECSENGINE_API ShaderReflectedBuffer {
		Stream<char> name;
		unsigned int byte_size;
		ShaderBufferType type;
		unsigned short register_index;
	};

	enum class ShaderTextureType {
		Read1D,
		Read2D,
		Read3D,
		ReadWrite1D,
		ReadWrite2D,
		ReadWrite3D,
		Array1D,
		Array2D,
		Array3D,
		Cube
	};

	struct ECSENGINE_API ShaderReflectedTexture {
		Stream<char> name;
		ShaderTextureType type;
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