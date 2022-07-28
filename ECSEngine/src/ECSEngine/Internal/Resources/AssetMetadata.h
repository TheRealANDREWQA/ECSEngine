// ECS_REFLECT
#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../../Rendering/RenderingStructures.h"
#include "../../Utilities/FunctionInterfaces.h"
#include "../../Utilities/Reflection/ReflectionMacros.h"

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

	// ------------------------------------------------------------------------------------------------------

	struct ECSENGINE_API ECS_REFLECT MeshMetadata {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		MeshMetadata Copy(AllocatorPolymorphic allocator) const;

		Stream<char> name;
		float scale_factor;
		bool coallesced_mesh;
		bool invert_z_axis;
		unsigned char optimize_level;
	};

	struct ECSENGINE_API ECS_REFLECT TextureMetadata {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;
		
		TextureMetadata Copy(AllocatorPolymorphic allocator) const;

		Stream<char> name;
		bool srgb;
		bool convert_to_nearest_power_of_two;
		bool generate_mip_maps;
		unsigned char compression_type;
	};

	struct ECSENGINE_API ECS_REFLECT GPUBufferMetadata {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		GPUBufferMetadata Copy(AllocatorPolymorphic allocator) const;

		Stream<char> name;
		unsigned char buffer_type;
	};

	struct ECSENGINE_API ECS_REFLECT GPUSamplerMetadata {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		GPUSamplerMetadata Copy(AllocatorPolymorphic allocator) const;

		Stream<char> name;
		unsigned char wrap_mode;
		unsigned char filter_mode;
		unsigned char anisotropic_level;
	};

	// Each macro definition and name is separately allocated
	struct ECSENGINE_API ECS_REFLECT ShaderMetadata {
		ShaderMetadata();
		ShaderMetadata(Stream<char> name, Stream<ShaderMacro> macros, AllocatorPolymorphic allocator);

		void AddMacro(const char* name, const char* definition, AllocatorPolymorphic allocator);

		ShaderMetadata Copy(AllocatorPolymorphic allocator) const;

		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		void RemoveMacro(size_t index, AllocatorPolymorphic allocator);

		void RemoveMacro(const char* name, AllocatorPolymorphic allocator);

		void UpdateMacro(size_t index, const char* new_definition, AllocatorPolymorphic allocator);

		void UpdateMacro(const char* name, const char* new_definition, AllocatorPolymorphic allocator);

		// Returns -1 if the macro is not found
		size_t SearchMacro(const char* name) const;

		Stream<char> name;
		Stream<ShaderMacro> macros;
	};

	struct ECS_REFLECT MaterialAssetResource {
		unsigned char shader_type;
		unsigned char slot;
		unsigned int metadata_handle;
	};

	// The name is separately allocated from the other buffers
	// The MaterialAssetResource buffers are maintained as a single coallesced buffer
	struct ECSENGINE_API ECS_REFLECT MaterialAsset {
		MaterialAsset() = default;
		MaterialAsset(Stream<char> name, AllocatorPolymorphic allocator);

		void AddTexture(MaterialAssetResource texture, AllocatorPolymorphic allocator);

		void AddBuffer(MaterialAssetResource buffer, AllocatorPolymorphic allocator);

		void AddSampler(MaterialAssetResource sampler, AllocatorPolymorphic allocator);

		void AddShader(MaterialAssetResource shader, AllocatorPolymorphic allocator);

		MaterialAsset Copy(void* buffer) const;

		MaterialAsset Copy(AllocatorPolymorphic allocator) const;

		size_t CopySize() const;

		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		void RemoveTexture(unsigned int index, AllocatorPolymorphic allocator);

		void RemoveBuffer(unsigned int index, AllocatorPolymorphic allocator);

		void RemoveSampler(unsigned int index, AllocatorPolymorphic allocator);

		void RemoveShader(unsigned int index, AllocatorPolymorphic allocator);

		Stream<char> name;
		// These are maintained as a coallesced buffer
		Stream<MaterialAssetResource> textures;
		Stream<MaterialAssetResource> buffers;
		Stream<MaterialAssetResource> samplers;
		Stream<MaterialAssetResource> shaders;
	};

	struct ECS_REFLECT MiscAsset {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		MiscAsset Copy(AllocatorPolymorphic allocator) const;

		Stream<wchar_t> path;
	};

}