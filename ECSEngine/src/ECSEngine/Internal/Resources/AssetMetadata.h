#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../../Rendering/RenderingStructures.h"

namespace ECSEngine {

	enum ECS_ASSET_TYPE : unsigned char {
		ECS_ASSET_MESH,
		ECS_ASSET_TEXTURE,
		ECS_ASSET_GPU_BUFFER,
		ECS_ASSET_GPU_SAMPLER,
		ECS_ASSET_SHADER,
		ECS_ASSET_MATERIAL,
		ECS_ASSET_MISC,
		ECS_ASSET_TYPE_COUNT
	};

	enum ECS_ASSET_METADATA_CODE : unsigned char {
		ECS_ASSET_METADATA_OK,
		ECS_ASSET_METADATA_FAILED_TO_READ,
		ECS_ASSET_METADATA_VERSION_MISMATCH,
		ECS_ASSET_METADATA_VERSION_OLD,
		ECS_ASSET_METADATA_VERSION_INVALID_DATA
	};

	struct MeshMetadata {
		bool coallesced_mesh;
		bool invert_z_axis;
		unsigned char optimize_level;
		float scale_factor;
	};

	struct TextureMetadata {
		bool srgb : 1;
		bool convert_to_nearest_power_of_two : 1;
		bool generate_mip_maps : 1;
		bool compression_type : 4;
	};

	struct GPUBufferMetadata {
		char buffer_type : 4;
		char cpu_access : 2;
	};

	struct GPUSamplerMetadata {
		char wrap_mode : 3;
		char filter_mode : 1;
		char anisotropic_level : 4;
	};

	struct ShaderMetadata {
		ShaderMetadata();
		ShaderMetadata(Stream<ShaderMacro> macros, AllocatorPolymorphic allocator);

		void AddMacro(const char* name, const char* definition, AllocatorPolymorphic allocator);

		void RemoveMacro(size_t index, AllocatorPolymorphic allocator);

		void RemoveMacro(const char* name, AllocatorPolymorphic allocator);

		void UpdateMacro(size_t index, const char* new_definition, AllocatorPolymorphic allocator);

		void UpdateMacro(const char* name, const char* new_definition, AllocatorPolymorphic allocator);

		// Returns -1 if the macro is not found
		size_t SearchMacro(const char* name) const;

		Stream<ShaderMacro> macros;
	};

	struct MaterialAssetResource {
		unsigned int shader_index : 3;
		unsigned int slot : 8;
		unsigned int metadata_index;
	};

	struct MaterialAsset {
		void AddTexture(MaterialAssetResource texture, AllocatorPolymorphic allocator);

		void AddBuffer(MaterialAssetResource buffer, AllocatorPolymorphic allocator);

		void AddSampler(MaterialAssetResource sampler, AllocatorPolymorphic allocator);

		MaterialAsset Copy(void* buffer);

		size_t CopySize() const;

		void RemoveTexture(unsigned int index, AllocatorPolymorphic allocator);

		void RemoveBuffer(unsigned int index, AllocatorPolymorphic allocator);

		void RemoveSampler(unsigned int index, AllocatorPolymorphic allocator);

		// These are indices into a shader metadata database
		// Ignore the compute shader
		unsigned int shaders[ECS_SHADER_TYPE_COUNT - 1];

		// These are maintained as a coallesced buffer
		Stream<MaterialAssetResource> textures;
		Stream<MaterialAssetResource> buffers;
		Stream<MaterialAssetResource> samplers;
	};

	// ------------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeMeshMetadata(MeshMetadata metadata, uintptr_t& buffer);

	ECSENGINE_API size_t SerializeMeshMetadataSize();

	// ------------------------------------------------------------------------------------------------------

	ECSENGINE_API ECS_ASSET_METADATA_CODE DeserializeMeshMetadata(MeshMetadata* metadata, uintptr_t& buffer);

	// ------------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeTextureMetadata(TextureMetadata metadata, uintptr_t& buffer);

	ECSENGINE_API size_t SerializeTextureMetadataSize();

	// ------------------------------------------------------------------------------------------------------

	ECSENGINE_API ECS_ASSET_METADATA_CODE DeserializeTextureMetadata(TextureMetadata* metadata, uintptr_t& buffer);

	// ------------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeGPUBufferMetadata(GPUBufferMetadata metadata, uintptr_t& buffer);

	ECSENGINE_API size_t SerializeGPUBufferMetadataSize();

	// ------------------------------------------------------------------------------------------------------

	ECSENGINE_API ECS_ASSET_METADATA_CODE DeserializeGPUBufferMetadata(GPUBufferMetadata* metadata, uintptr_t& buffer);
	// ------------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeGPUSamplerMetadata(GPUSamplerMetadata metadata, uintptr_t& buffer);

	ECSENGINE_API size_t SerializeGPUSamplerMetadataSize();

	// ------------------------------------------------------------------------------------------------------

	ECSENGINE_API ECS_ASSET_METADATA_CODE DeserializeGPUSamplerMetadata(GPUSamplerMetadata* metadata, uintptr_t& buffer);

	// ------------------------------------------------------------------------------------------------------

	ECSENGINE_API void SerializeShaderMetadata(ShaderMetadata metadata, uintptr_t& buffer);

	ECSENGINE_API size_t SerializeShaderMetadataSize(ShaderMetadata metadata);

	// ------------------------------------------------------------------------------------------------------

	// Each macro string will be separately allocated
	ECSENGINE_API ECS_ASSET_METADATA_CODE DeserializeShaderMetadata(ShaderMetadata* metadata, uintptr_t& buffer, AllocatorPolymorphic allocator = { nullptr });

	// Returns -1 if the version is not valid
	// Returns how many bytes of the current buffer are used for the shader metadata
	ECSENGINE_API size_t DeserializeShaderMetadataSize(uintptr_t buffer);

	// ------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool SerializeMaterialAsset(MaterialAsset material, Stream<wchar_t> file);

	ECSENGINE_API void SerializeMaterialAsset(MaterialAsset material, uintptr_t& buffer);

	ECSENGINE_API size_t SerializeMaterialAssetSize(MaterialAsset material);

	// ------------------------------------------------------------------------------------------------------

	// It will copy the streams into a separate buffer
	ECSENGINE_API ECS_ASSET_METADATA_CODE DeserializeMaterialAsset(MaterialAsset* material, Stream<wchar_t> file, AllocatorPolymorphic allocator);

	// It will copy the streams into a separate buffer
	ECSENGINE_API ECS_ASSET_METADATA_CODE DeserializeMaterialAsset(MaterialAsset* material, uintptr_t& buffer, AllocatorPolymorphic allocator);

	// The streams will point into the buffer memory
	ECSENGINE_API ECS_ASSET_METADATA_CODE DeserializeMaterialAsset(MaterialAsset* material, uintptr_t& buffer);

	// Returns -1 if the version is not valid
	// Returns how many bytes belong to the material asset
	ECSENGINE_API size_t DeserializeMaterialAssetSize(uintptr_t buffer);

	// ------------------------------------------------------------------------------------------------------
}