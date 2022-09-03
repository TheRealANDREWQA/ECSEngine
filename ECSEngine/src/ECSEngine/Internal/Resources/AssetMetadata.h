// ECS_REFLECT
#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../../Rendering/RenderingStructures.h"
#include "../../Utilities/FunctionInterfaces.h"
#include "../../Rendering/ShaderReflection.h"
#include "../../Utilities/Reflection/ReflectionMacros.h"
#include "../../Rendering/Compression/TextureCompressionTypes.h"

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

	enum ECS_REFLECT ECS_ASSET_MESH_OPTIMIZE_LEVEL : unsigned char {
		ECS_ASSET_MESH_OPTIMIZE_NONE,
		ECS_ASSET_MESH_OPTIMIZE_BASIC,
		ECS_ASSET_MESH_OPTIMIZE_ADVANCED
	};

	// ------------------------------------------------------------------------------------------------------

	struct ECSENGINE_API ECS_REFLECT MeshMetadata {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		MeshMetadata Copy(AllocatorPolymorphic allocator) const;

		// Sets default values and aliases the name
		void Default(Stream<char> name);

		Stream<char> name;
		float scale_factor;
		bool coallesced_mesh;
		bool invert_z_axis;
		ECS_ASSET_MESH_OPTIMIZE_LEVEL optimize_level;
	};

	struct ECSENGINE_API ECS_REFLECT TextureMetadata {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;
		
		TextureMetadata Copy(AllocatorPolymorphic allocator) const;

		// Sets default values and aliases the name
		void Default(Stream<char> name);

		Stream<char> name;
		bool sRGB;
		bool convert_to_nearest_power_of_two;
		bool generate_mip_maps;
		ECS_TEXTURE_COMPRESSION compression_type;
	};

	struct ECSENGINE_API ECS_REFLECT GPUBufferMetadata {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		GPUBufferMetadata Copy(AllocatorPolymorphic allocator) const;
		
		// Sets default values and aliases the name
		void Default(Stream<char> name);

		Stream<char> name;
		ECS_SHADER_BUFFER_TYPE buffer_type;
	};

	struct ECSENGINE_API ECS_REFLECT GPUSamplerMetadata {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		GPUSamplerMetadata Copy(AllocatorPolymorphic allocator) const;

		void Default(Stream<char> name);

		Stream<char> name;
		ECS_SAMPLER_ADDRESS_TYPE address_mode;
		ECS_SAMPLER_FILTER_TYPE filter_mode;
		unsigned char anisotropic_level;
	};

	// Each macro definition and name is separately allocated
	struct ECSENGINE_API ECS_REFLECT ShaderMetadata {
		ShaderMetadata();
		ShaderMetadata(Stream<char> name, Stream<ShaderMacro> macros, AllocatorPolymorphic allocator);

		void AddMacro(const char* name, const char* definition, AllocatorPolymorphic allocator);

		ShaderMetadata Copy(AllocatorPolymorphic allocator) const;

		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		// Sets default values and aliases the name
		void Default(Stream<char> name);

		void RemoveMacro(size_t index, AllocatorPolymorphic allocator);

		void RemoveMacro(const char* name, AllocatorPolymorphic allocator);

		void UpdateMacro(size_t index, const char* new_definition, AllocatorPolymorphic allocator);

		void UpdateMacro(const char* name, const char* new_definition, AllocatorPolymorphic allocator);

		// Returns -1 if the macro is not found
		size_t SearchMacro(const char* name) const;

		Stream<char> name;
		Stream<ShaderMacro> macros;
		ECS_SHADER_TYPE shader_type;
	};

	struct MaterialAssetResource {
		unsigned int metadata_handle;
		unsigned char slot;
	};

	// The name is separately allocated from the other buffers
	// The MaterialAssetResource buffers are maintained as a single coallesced buffer
	struct ECSENGINE_API MaterialAsset {
		MaterialAsset() = default;
		MaterialAsset(Stream<char> name, AllocatorPolymorphic allocator);

		void AddTexture(MaterialAssetResource texture, AllocatorPolymorphic allocator);

		void AddBuffer(MaterialAssetResource buffer, AllocatorPolymorphic allocator);

		void AddSampler(MaterialAssetResource sampler, AllocatorPolymorphic allocator);

		void AddShader(unsigned int shader, AllocatorPolymorphic allocator);

		MaterialAsset Copy(void* buffer) const;

		MaterialAsset Copy(AllocatorPolymorphic allocator) const;

		size_t CopySize() const;

		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		// Sets default values and aliases the name
		void Default(Stream<char> name);

		void RemoveTexture(unsigned int index, AllocatorPolymorphic allocator);

		void RemoveBuffer(unsigned int index, AllocatorPolymorphic allocator);

		void RemoveSampler(unsigned int index, AllocatorPolymorphic allocator);

		void RemoveShader(unsigned int index, AllocatorPolymorphic allocator);

		Stream<char> name;
		// These are maintained as a coallesced buffer
		Stream<MaterialAssetResource> textures;
		Stream<MaterialAssetResource> buffers;
		Stream<MaterialAssetResource> samplers;
		Stream<unsigned int> shaders;
	};

	struct ECS_REFLECT MiscAsset {
		void DeallocateMemory(AllocatorPolymorphic allocator) const;

		MiscAsset Copy(AllocatorPolymorphic allocator) const;

		// It it will treat it as a Stream<wchar_t>, it exists to allow treatment of the misc asset
		// the same as the other assets which have a name
		void Default(Stream<char> name);

		// It will alias the name
		void Default(Stream<wchar_t> path);

		Stream<wchar_t> path;
	};

}