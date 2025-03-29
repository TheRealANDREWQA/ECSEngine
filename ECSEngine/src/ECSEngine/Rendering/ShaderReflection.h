#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "../Containers/HashTable.h"
#include "../ECS/InternalStructures.h"
#include "RenderingStructures.h"
#include "../Utilities/Reflection/ReflectionTypes.h"

namespace ECSEngine {

#define ECS_SHADER_MAX_CONSTANT_BUFFER_SLOT 16
#define ECS_SHADER_MAX_SAMPLER_SLOT 16
#define ECS_SHADER_MAX_TEXTURE_SLOT 128

#define ECS_SHADER_REFLECTION_CONSTANT_BUFFER_TAG_DELIMITER "___"

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
		ECS_SHADER_BUFFER_CONSUME,
		ECS_SHADER_BUFFER_COUNT
	};

	// Byte size is useful only for constant buffers in order to create them directly on the Graphics object
	// Also tags can be added to buffer descriptions - they can be parameterized tags as well
	struct ECSENGINE_API ShaderReflectedBuffer {
		// Returns { nullptr, 0 } if it doesn't exist
		Stream<char> GetTag(Stream<char> tag) const;

		ECS_INLINE bool HasTag(Stream<char> tag) const {
			if (tag.size > 0) {
				return FindFirstToken(tags, tag).size > 0;
			}
			return false;
		}

		Stream<char> name;
		Stream<char> tags;
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

	// The x position indicates the offset inside the reflection type where it starts and 
	// the y component how many rows there are
	struct ShaderReflectionBufferMatrixField {
		uint2 position;
		Stream<char> name;
	};

	struct ShaderReflectionBuffersOptions {
		bool only_constant_buffers = false;
		CapacityStream<Reflection::ReflectionType>* constant_buffer_reflection = nullptr;

		CapacityStream<ShaderReflectionBufferMatrixField> matrix_types_storage = { nullptr, 0, 0 };
		CapacityStream<char> matrix_type_name_storage = { nullptr, 0, 0 };
		// If this is specified, then it will fill in the positions and the row counts
		// of the matrix types. There will be a stream for each type. The matrix_types_storage and matrix_type_name_storage
		// must be specified in order to fill in the values (they will not be allocated from the allocator)
		Stream<ShaderReflectionBufferMatrixField>* matrix_types = nullptr;
	};

	struct ReflectedShader {
		// Vertex input layout descriptor
		CapacityStream<D3D11_INPUT_ELEMENT_DESC>* input_layout_descriptor = nullptr;

		// Vertex buffer binding types
		CapacityStream<ECS_MESH_INDEX>* input_layout_mapping = nullptr;
	
		// Buffers
		CapacityStream<ShaderReflectedBuffer>* buffers = nullptr;
		ShaderReflectionBuffersOptions buffer_options = {};

		// Textures
		CapacityStream<ShaderReflectedTexture>* textures = nullptr;

		// Samplers
		CapacityStream<ShaderReflectedSampler>* samplers = nullptr;

		// Used for all allocations except macros
		AllocatorPolymorphic allocator;

		// Used to extract macros from the file
		CapacityStream<Stream<char>>* defined_macros = nullptr;
		CapacityStream<Stream<char>>* conditional_macros = nullptr;
		AllocatorPolymorphic macro_allocator = ECS_MALLOC_ALLOCATOR;
	};

	struct ECSENGINE_API ShaderReflection {
		ShaderReflection();
		ShaderReflection(void* allocation);

		ShaderReflection(const ShaderReflection& other) = default;
		ShaderReflection& operator = (const ShaderReflection& other) = default;

		// Returns whether or not it succeded. Only the name is allocated
		// The macros are used to preprocess the file
		bool ReflectVertexShaderInput(
			Stream<wchar_t> path, 
			CapacityStream<D3D11_INPUT_ELEMENT_DESC>& elements, 
			AllocatorPolymorphic allocator,
			Stream<Stream<char>> external_macros = {}
		) const;

		// Returns whether or not it succeded. Only the name is allocated
		bool ReflectVertexShaderInputSource(Stream<char> source_code, CapacityStream<D3D11_INPUT_ELEMENT_DESC>& elements, AllocatorPolymorphic allocator) const;

		// Returns whether or not it succeded
		// If the reflection_types is specified, it will create a reflection equivalent for constant buffers
		// There must be at least buffers.capacity these reflection_types available in that pointer. The macros
		// are used to preprocess the file
		bool ReflectShaderBuffers(
			Stream<wchar_t> path,
			CapacityStream<ShaderReflectedBuffer>& buffers, 
			AllocatorPolymorphic allocator,
			ShaderReflectionBuffersOptions options = {},
			Stream<Stream<char>> external_macros = {}
		) const;

		// Returns whether or not it succeded
		// If the reflection_types is specified, it will create a reflection equivalent for constant buffers
		// There must be at least buffers.capacity these reflection_types available in that pointer
		bool ReflectShaderBuffersSource(
			Stream<char> source_code, 
			CapacityStream<ShaderReflectedBuffer>& buffers, 
			AllocatorPolymorphic allocator, 
			ShaderReflectionBuffersOptions options = {}
		) const;

		// Returns whether or not it succeded. The macros are used to preprocess the file
		bool ReflectShaderTextures(
			Stream<wchar_t> path, 
			CapacityStream<ShaderReflectedTexture>& textures, 
			AllocatorPolymorphic allocator, 
			Stream<Stream<char>> external_macros = {}
		) const;

		// Returns whether or not it succeded
		bool ReflectShaderTexturesSource(Stream<char> source_code, CapacityStream<ShaderReflectedTexture>& textures, AllocatorPolymorphic allocator) const;

		// Returns whether or not it succeeded. The macros are used to preprocess the file
		bool ReflectShaderSamplers(
			Stream<wchar_t> path, 
			CapacityStream<ShaderReflectedSampler>& samplers, 
			AllocatorPolymorphic allocator,
			Stream<Stream<char>> external_macros = {}
		) const;

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

		// Returns whether or not it succeeded. The macros are used to preprocess the file
		bool ReflectVertexBufferMapping(Stream<wchar_t> path, CapacityStream<ECS_MESH_INDEX>& mapping, Stream<Stream<char>> external_macros = {}) const;

		// Returns whether or not it succeeded
		bool ReflectVertexBufferMappingSource(Stream<char> source_code, CapacityStream<ECS_MESH_INDEX>& mapping) const;

		// Returns whether or not it succeeded
		// The macros are used to preprocess the file
		bool ReflectShader(Stream<char> source_code, const ReflectedShader* reflected_shader) const;

		// Returns whether or not it succeeded
		bool ReflectComputeShaderDispatchSize(Stream<char> source_code, uint3* dispatch_size) const;

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

	// Parses a single struct (that can be a constant buffer) out of the given source code and can optionally
	// give a reflection type to construct. The allocator is needed only when the reflection
	// type is specified, it needs to be specified such that the allocations needed for the
	// reflection type are performed from it. The default slot position is used to assign in
	// the slot value when there is no register keyword specified. If it fails it returns
	// buffer with byte size -1 (it can fail if it cannot reflect the byte size or the type)
	// When the type is a struct it will fill in the type field to ECS_SHADER_BUFFER_COUNT
	ECSENGINE_API ShaderReflectedBuffer ReflectShaderStruct(
		const ShaderReflection* shader_reflection, 
		Stream<char> source,
		unsigned int default_slot_position,
		Reflection::ReflectionType* reflection_type = nullptr,
		AllocatorPolymorphic allocator = ECS_MALLOC_ALLOCATOR,
		Stream<ShaderReflectionBufferMatrixField>* matrix_types = nullptr,
		CapacityStream<char>* matrix_type_name_storage = nullptr
	);

	ECSENGINE_API void ShaderInputLayoutDeallocate(
		Stream<D3D11_INPUT_ELEMENT_DESC> elements,
		AllocatorPolymorphic allocator
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

	ECSENGINE_API void ShaderReflectedBuffersDeallocate(
		Stream<ShaderReflectedBuffer> buffers,
		AllocatorPolymorphic allocator,
		Stream<Reflection::ReflectionType> reflection_types = { nullptr, 0 }
	);

}