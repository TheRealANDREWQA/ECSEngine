#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Containers/HashTable.h"
#include "../Internal/InternalStructures.h"
#include "RenderingStructures.h"
#include "ecspch.h"
#include "../Utilities/Reflection/ReflectionTypes.h"

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

	struct ShaderConstantBufferReflectionTypeMapping {
		Reflection::ReflectionBasicFieldType basic_type;
		unsigned char component_count;
	};

	typedef HashTableDefault<DXGI_FORMAT> ShaderReflectionFormatTable;
	typedef HashTableDefault<ShaderReflectionFloatExtendedFormat> ShaderReflectionFloatFormatTable;
	typedef HashTableDefault<ShaderReflectionIntegerExtendedFormat> ShaderReflectionIntegerFormatTable;
	typedef HashTableDefault<ShaderConstantBufferReflectionTypeMapping> ShaderReflectionCBTypeMapping;

	enum ECS_SHADER_BUFFER_TYPE : unsigned char {
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

	enum ECS_SHADER_TEXTURE_TYPE : unsigned char {
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

	struct ShaderReflectedSampler {
		Stream<char> name;
		unsigned short register_index;
	};

	struct ECSENGINE_API ShaderReflection {
		ShaderReflection();
		ShaderReflection(void* allocation);

		ShaderReflection(const ShaderReflection& other) = default;
		ShaderReflection& operator = (const ShaderReflection& other) = default;

		// Returns whether or not it succeded
		bool ReflectVertexShaderInput(Stream<wchar_t> path, CapacityStream<D3D11_INPUT_ELEMENT_DESC>& elements, AllocatorPolymorphic allocator) const;

		// Returns whether or not it succeded
		bool ReflectVertexShaderInputSource(Stream<char> source_code, CapacityStream<D3D11_INPUT_ELEMENT_DESC>& elements, AllocatorPolymorphic allocator) const;

		// Returns whether or not it succeded
		// If the reflection_types is specified, it will create a reflection equivalent for constant buffers
		// There must be at least buffers.capacity these reflection_types available in that pointer
		bool ReflectShaderBuffers(
			Stream<wchar_t> path, 
			CapacityStream<ShaderReflectedBuffer>& buffers, 
			AllocatorPolymorphic allocator,
			bool only_constant_buffers = false,
			Reflection::ReflectionType* constant_buffer_reflection = nullptr
		) const;

		// Returns whether or not it succeded
		// If the reflection_types is specified, it will create a reflection equivalent for constant buffers
		// There must be at least buffers.capacity these reflection_types available in that pointer
		bool ReflectShaderBuffersSource(
			Stream<char> source_code, 
			CapacityStream<ShaderReflectedBuffer>& buffers, 
			AllocatorPolymorphic allocator, 
			bool only_constant_buffers = false,
			Reflection::ReflectionType* constant_buffer_reflection = nullptr
		) const;

		// Returns whether or not it succeded
		bool ReflectShaderTextures(Stream<wchar_t> path, CapacityStream<ShaderReflectedTexture>& textures, AllocatorPolymorphic allocator) const;

		// Returns whether or not it succeded
		bool ReflectShaderTexturesSource(Stream<char> source_code, CapacityStream<ShaderReflectedTexture>& textures, AllocatorPolymorphic allocator) const;

		// Returns whether or not it succeeded
		bool ReflectShaderSamplers(Stream<wchar_t> path, CapacityStream<ShaderReflectedSampler>& samplers, AllocatorPolymorphic allocator) const;

		// Returns whether or not it succeeded
		bool ReflectShaderSamplersSource(Stream<char> source_code, CapacityStream<ShaderReflectedSampler>& samplers, AllocatorPolymorphic allocator) const;

		// Returns whether or not it succeeded. It only fills in the name of the macros defined in file. Set the pointer to
		// nullptr if you are not interested in that type of macro.
		// If the allocator is nullptr, then it will only reference the source code, it will not make an allocation
		bool ReflectShaderMacros(
			Stream<char> source_code, 
			CapacityStream<Stream<char>>* defined_macros, 
			CapacityStream<Stream<char>>* conditional_macros,
			AllocatorPolymorphic allocator
		) const;

		// Returns whether or not it succeded
		bool ReflectVertexBufferMapping(Stream<wchar_t> path, CapacityStream<ECS_MESH_INDEX>& mapping) const;

		// Returns whether or not it succeded
		bool ReflectVertexBufferMappingSource(Stream<char> source_code, CapacityStream<ECS_MESH_INDEX>& mapping) const;

		// It will try to determine the type of the shader from source code
		ECS_SHADER_TYPE DetermineShaderType(Stream<char> source_code) const;

		void InitializeCBTypeMappingTable();

		// Returns the amount of bytes necessary to create an instance of this class
		static size_t MemoryOf();

		ShaderReflectionFormatTable string_table;
		ShaderReflectionFloatFormatTable float_table;
		ShaderReflectionIntegerFormatTable unsigned_table;
		ShaderReflectionIntegerFormatTable signed_table;
		ShaderReflectionCBTypeMapping cb_mapping_table;
	};

	// Parses a single constant buffer out of the given source code and can optionally
	// give a reflection type to construct. The allocator is needed only when the reflection
	// type is specified, it needs to be specified such that the allocations needed for the
	// reflection type are performed from it. The default slot position is used to assign in
	// the slot value when there is no register keyword specified. If it fails it returns
	// buffer with byte size -1 (it can fail if it cannot reflect the byte size or the type)
	ECSENGINE_API ShaderReflectedBuffer ReflectShaderConstantBuffer(
		const ShaderReflection* shader_reflection, 
		Stream<char> source,
		unsigned int default_slot_position,
		Reflection::ReflectionType* reflection_type = nullptr,
		AllocatorPolymorphic allocator = { nullptr }
	);

	ECSENGINE_API void ShaderConstantBufferReflectionTypeCopy(
		const Reflection::ReflectionType* reflection_type,
		Reflection::ReflectionType* copy,
		AllocatorPolymorphic allocator
	);

	ECSENGINE_API void ShaderConstantBufferReflectionTypeDeallocate(
		const Reflection::ReflectionType* reflection_type,
		AllocatorPolymorphic allocator
	);

	ECSENGINE_API void ShaderReflectedTexturesDeallocate(
		Stream<ShaderReflectedTexture> textures,
		AllocatorPolymorphic allocator
	);

	ECSENGINE_API void ShaderReflectedSamplersDeallocate(
		Stream<ShaderReflectedSampler> samplers,
		AllocatorPolymorphic allocator
	);

}