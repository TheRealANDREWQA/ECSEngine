// ECS_REFLECT
#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../../Rendering/RenderingStructures.h"
#include "../../Utilities/FunctionInterfaces.h"
#include "../../Rendering/ShaderReflection.h"
#include "../../Utilities/Reflection/ReflectionMacros.h"
#include "../../Rendering/Compression/TextureCompressionTypes.h"
#include "../../Utilities/Serialization/Binary/SerializationMacro.h"
#include "../../Internal/Resources/ResourceTypes.h"

namespace ECSEngine {

	enum ECS_ASSET_TYPE : unsigned char {
		ECS_ASSET_MESH,
		ECS_ASSET_TEXTURE,
		ECS_ASSET_GPU_SAMPLER,
		ECS_ASSET_SHADER,
		ECS_ASSET_MATERIAL,
		ECS_ASSET_MISC,
		ECS_ASSET_TYPE_COUNT
	};

	ECSENGINE_API bool AssetHasFile(ECS_ASSET_TYPE type);

	ECSENGINE_API ResourceType AssetTypeToResourceType(ECS_ASSET_TYPE type);

	struct AssetFieldTarget {
		Stream<char> name;
		ECS_ASSET_TYPE asset_type;
	};

	ECSENGINE_API extern AssetFieldTarget ECS_ASSET_METADATA_MACROS[];

	ECSENGINE_API size_t ECS_ASSET_METADATA_MACROS_SIZE();

	ECSENGINE_API extern AssetFieldTarget ECS_ASSET_TARGET_FIELD_NAMES[];

	ECSENGINE_API size_t ECS_ASSET_TARGET_FIELD_NAMES_SIZE();

	ECSENGINE_API ECS_ASSET_TYPE FindAssetMetadataMacro(Stream<char> string);

	ECSENGINE_API ECS_ASSET_TYPE FindAssetTargetField(Stream<char> string);

	// The string is read-only from the global memory (it is a constant)
	ECSENGINE_API const char* ConvertAssetTypeString(ECS_ASSET_TYPE type);

	ECSENGINE_API size_t AssetMetadataByteSize(ECS_ASSET_TYPE type);

	enum ECS_REFLECT ECS_ASSET_MESH_OPTIMIZE_LEVEL : unsigned char {
		ECS_ASSET_MESH_OPTIMIZE_NONE,
		ECS_ASSET_MESH_OPTIMIZE_BASIC,
		ECS_ASSET_MESH_OPTIMIZE_ADVANCED
	};

	// This is the limit of assets which can be randomized on asset database load
#define ECS_ASSET_RANDOMIZED_ASSET_LIMIT 10'000

	// ------------------------------------------------------------------------------------------------------

	struct ECSENGINE_API ECS_REFLECT MeshMetadata {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		MeshMetadata Copy(AllocatorPolymorphic allocator) const;

		// Sets default values and aliases the name
		void Default(Stream<char> name, Stream<wchar_t> file);

		bool SameTarget(const MeshMetadata* other) const;

		ECS_INLINE void* Pointer() const {
			return mesh_pointer;
		}

		ECS_INLINE void** PtrToPointer() {
			return (void**)&mesh_pointer;
		}

		Stream<char> name;
		Stream<wchar_t> file;
		float scale_factor;
		bool invert_z_axis;
		ECS_ASSET_MESH_OPTIMIZE_LEVEL optimize_level;

		CoallescedMesh* mesh_pointer; ECS_SKIP_REFLECTION()
	};

	struct ECSENGINE_API ECS_REFLECT TextureMetadata {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;
		
		TextureMetadata Copy(AllocatorPolymorphic allocator) const;

		// Sets default values and aliases the name
		void Default(Stream<char> name, Stream<wchar_t> file);

		bool SameTarget(const TextureMetadata* other) const;

		ECS_INLINE void* Pointer() const {
			return texture.view;
		}

		ECS_INLINE void** PtrToPointer() {
			return (void**)&texture;
		}

		Stream<char> name;
		Stream<wchar_t> file;
		bool sRGB;
		bool generate_mip_maps;
		ECS_TEXTURE_COMPRESSION_EX compression_type;

		ResourceView texture; ECS_SKIP_REFLECTION(static_assert(sizeof(ResourceView) == 8))
	};

	struct ECSENGINE_API ECS_REFLECT GPUSamplerMetadata {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		GPUSamplerMetadata Copy(AllocatorPolymorphic allocator) const;

		void Default(Stream<char> name, Stream<wchar_t> file);

		ECS_INLINE void* Pointer() const {
			return sampler.sampler;
		}

		ECS_INLINE void** PtrToPointer() {
			return (void**)&sampler;
		}

		Stream<char> name;
		ECS_SAMPLER_ADDRESS_TYPE address_mode;
		ECS_SAMPLER_FILTER_TYPE filter_mode;
		unsigned char anisotropic_level;

		SamplerState sampler; ECS_SKIP_REFLECTION(static_assert(sizeof(SamplerState) == 8))
	};

	// Each macro definition and name is separately allocated
	struct ECSENGINE_API ECS_REFLECT ShaderMetadata {
		ShaderMetadata();
		ShaderMetadata(Stream<char> name, Stream<ShaderMacro> macros, AllocatorPolymorphic allocator);

		void AddMacro(Stream<char> name, Stream<char> definition, AllocatorPolymorphic allocator);

		ShaderMetadata Copy(AllocatorPolymorphic allocator) const;

		bool SameTarget(const ShaderMetadata* other) const;

		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		// Sets default values and aliases the name
		void Default(Stream<char> name, Stream<wchar_t> file);

		// Returns -1 if the macro is not found
		unsigned int FindMacro(Stream<char> name) const;

		void RemoveMacro(unsigned int index, AllocatorPolymorphic allocator);

		void RemoveMacro(Stream<char> name, AllocatorPolymorphic allocator);

		void UpdateMacro(unsigned int index, Stream<char> new_definition, AllocatorPolymorphic allocator);

		void UpdateMacro(Stream<char> name, Stream<char> new_definition, AllocatorPolymorphic allocator);

		ECS_INLINE void* Pointer() const {
			return shader_interface;
		}

		ECS_INLINE void** PtrToPointer() {
			return &shader_interface;
		}

		Stream<char> name;
		Stream<wchar_t> file;
		Stream<ShaderMacro> macros;
		ECS_SHADER_TYPE shader_type;
		ECS_SHADER_COMPILE_FLAGS compile_flag;

		// The Graphics object interface
		void* shader_interface; ECS_SKIP_REFLECTION()
		Stream<char> source_code; ECS_SKIP_REFLECTION()
		Stream<void> byte_code; ECS_SKIP_REFLECTION()
	};

	struct MaterialAssetResource {
		unsigned int metadata_handle;
		unsigned char slot;
	};

	// Constant buffer description
	// If it is dynamic, the data can be missing (the size must still be specified,
	// the pointer can be nullptr then). When the data is static
	struct MaterialAssetBuffer {
		Stream<void> data;
		bool dynamic;
		ECS_SHADER_TYPE shader_type;
		unsigned char slot;
	};

	// The name is separately allocated from the other buffers
	// The MaterialAssetResource buffers are maintained as a single coallesced buffer
	struct ECSENGINE_API MaterialAsset {
		MaterialAsset() = default;
		MaterialAsset(Stream<char> name, AllocatorPolymorphic allocator);

		void AddTexture(MaterialAssetResource texture, AllocatorPolymorphic allocator);

		void AddSampler(MaterialAssetResource sampler, AllocatorPolymorphic allocator);

		void AddBuffer(MaterialAssetBuffer buffer, AllocatorPolymorphic allocator);

		// This asset can only be copied with an allocator because the buffers can be independently allocated
		MaterialAsset Copy(AllocatorPolymorphic allocator) const;

		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		// Sets default values and aliases the name and the file
		void Default(Stream<char> name, Stream<wchar_t> file);

		void RemoveTexture(unsigned int index, AllocatorPolymorphic allocator);

		void RemoveSampler(unsigned int index, AllocatorPolymorphic allocator);

		void RemoveBuffer(unsigned int index, AllocatorPolymorphic allocator);

		void Resize(unsigned int texture_count, unsigned int sampler_count, unsigned int buffer_count, AllocatorPolymorphic allocator);

		ECS_INLINE void* Pointer() const {
			return material_pointer;
		}

		ECS_INLINE void** PtrToPointer() {
			return (void**)&material_pointer;
		}

		Stream<char> name;
		// These are maintained as a coallesced buffer
		Stream<MaterialAssetResource> textures;
		Stream<MaterialAssetResource> samplers;
		Stream<MaterialAssetBuffer> buffers;
		unsigned int vertex_shader_handle;
		unsigned int pixel_shader_handle;

		// A pointer to the graphics object
		Material* material_pointer; ECS_SKIP_REFLECTION()
	};

	struct ECS_REFLECT MiscAsset {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		MiscAsset Copy(AllocatorPolymorphic allocator) const;

		// Returns true if they have the same target
		bool SameTarget(const MiscAsset* other) const;

		void Default(Stream<char> name, Stream<wchar_t> file);

		ECS_INLINE void* Pointer() const {
			return data.buffer;
		}

		ECS_INLINE void** PtrToPointer() {
			return (void**)&data;
		}

		Stream<char> name;
		Stream<wchar_t> file;

		Stream<void> data; ECS_SKIP_REFLECTION()
	};

	ECSENGINE_API void DeallocateAssetBase(const void* asset, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator);

	// Returns { nullptr, 0 } for materials and samplers
	ECSENGINE_API Stream<wchar_t> GetAssetFile(const void* asset, ECS_ASSET_TYPE type);

	ECSENGINE_API Stream<char> GetAssetName(const void* asset, ECS_ASSET_TYPE type);

	ECSENGINE_API void CreateDefaultAsset(void* asset, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type);

	inline VertexShader GetVertexShaderFromMetadata(const ShaderMetadata* metadata) {
		return (ID3D11VertexShader*)metadata->shader_interface;
	}

	inline PixelShader GetPixelShaderFromMetadata(const ShaderMetadata* metadata) {
		return (ID3D11PixelShader*)metadata->shader_interface;
	}

	inline ComputeShader GetComputeShaderFromMetadata(const ShaderMetadata* metadata) {
		return (ID3D11ComputeShader*)metadata->shader_interface;
	}

	ECSENGINE_API Stream<void> GetAssetFromMetadata(const void* metadata, ECS_ASSET_TYPE type);

	ECSENGINE_API void SetAssetToMetadata(void* metadata, ECS_ASSET_TYPE type, Stream<void> asset);

	ECSENGINE_API void SetRandomizedAssetToMetadata(void* metadata, ECS_ASSET_TYPE type, unsigned int index);

	ECSENGINE_API bool IsAssetFromMetadataValid(const void* metadata, ECS_ASSET_TYPE type);

	ECSENGINE_API bool IsAssetFromMetadataValid(Stream<void> asset_pointer);

	// Meshes and materials are treated differently
	ECSENGINE_API unsigned int ExtractRandomizedAssetValue(const void* asset_pointer, ECS_ASSET_TYPE type);

	ECSENGINE_API bool CompareAssetPointers(const void* first, const void* second, ECS_ASSET_TYPE type);

	ECSENGINE_API bool CompareAssetPointers(const void* first, const void* second, size_t compare_size);

	// If long format is specified then it will print the full file path for absolute paths instead of only the filename
	ECSENGINE_API void AssetToString(const void* metadata, ECS_ASSET_TYPE type, CapacityStream<char>& string, bool long_format = false);

}