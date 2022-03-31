#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../../Containers/HashTable.h"
#include "AssetMetadata.h"

namespace ECSEngine {

	struct MeshMetadataPath {
		MeshMetadata metadata;
		Stream<wchar_t> path;
		Stream<char> name;
	};

	ECS_PACK(
		struct TextureMetadataPath {
			TextureMetadata metadata;
			Stream<wchar_t> path;
			Stream<char> name;
		};
	);

	ECS_PACK(
		struct GPUBufferMetadataPath {
			GPUBufferMetadata metadata;
			Stream<wchar_t> path;
			Stream<char> name;
		};
	);
	
	ECS_PACK(
		struct GPUSamplerMetadataPath {
			GPUSamplerMetadata metadata;
			Stream<wchar_t> path;
			Stream<char> name;
		};
	);

	struct ShaderMetadataPath {
		ShaderMetadata metadata;
		Stream<wchar_t> path;
		Stream<char> name;
	};

	struct MaterialAssetPath {
		MaterialAsset asset;
		Stream<char> name;
	};

	struct ECSENGINE_API AssetDatabase {
		AssetDatabase(AllocatorPolymorphic allocator);

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(AssetDatabase);

		void AddMesh(MeshMetadata metadata, Stream<wchar_t> path, Stream<char> name);

		void AddTexture(TextureMetadata metadata, Stream<wchar_t> path, Stream<char> name);

		void AddGPUBuffer(GPUBufferMetadata metadata, Stream<wchar_t> path, Stream<char> name);

		void AddGPUSampler(GPUSamplerMetadata metadata, Stream<wchar_t> path, Stream<char> name);

		void AddShader(ShaderMetadata metadata, Stream<wchar_t> path, Stream<char> name);

		// Here the path acts more like an identifier or name
		// Because the material is contained in itself - it does not reference
		// an external file unlike the metadata assets
		void AddMaterial(MaterialAsset material, Stream<char> name);

		void AddMisc(Stream<wchar_t> path);

		unsigned int FindMesh(Stream<char> name) const;

		unsigned int FindTexture(Stream<char> name) const;

		unsigned int FindGPUBuffer(Stream<char> name) const;

		unsigned int FindGPUSampler(Stream<char> name) const;

		unsigned int FindShader(Stream<char> name) const;

		unsigned int FindMaterial(Stream<char> name) const;

		unsigned int FindMisc(Stream<wchar_t> path) const;

		// Can operate on it afterwards, like adding, removing or updating macros
		// Returns nullptr if it doesn't exist
		ShaderMetadata* GetShader(Stream<char> name);

		// Retrive the allocator for this database
		AllocatorPolymorphic Allocator() const;

		bool RemoveMesh(Stream<char> name);

		void RemoveMeshIndex(unsigned int index);

		bool RemoveTexture(Stream<char> name);

		void RemoveTextureIndex(unsigned int index);

		bool RemoveGPUBuffer(Stream<char> name);

		void RemoveGPUBUfferIndex(unsigned int index);

		bool RemoveGPUSampler(Stream<char> name);

		void RemoveGPUSamplerIndex(unsigned int index);

		bool RemoveShader(Stream<char> name);

		void RemoveShaderIndex(unsigned int index);

		bool RemoveMaterial(Stream<char> name);

		void RemoveMaterialIndex(unsigned int index);

		bool RemoveMisc(Stream<wchar_t> path);

		void RemoveMiscIndex(unsigned int index);

		ResizableStream<MeshMetadataPath> mesh_asset;
		ResizableStream<TextureMetadataPath> texture_asset;
		ResizableStream<GPUBufferMetadataPath> gpu_buffer_asset;
		ResizableStream<GPUSamplerMetadataPath> gpu_sampler_asset;
		ResizableStream<ShaderMetadataPath> shader_asset;
		ResizableStream<MaterialAssetPath> material_asset;
		ResizableStream<Stream<wchar_t>> misc_asset;
	};

	// ------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool SerializeAssetDatabase(const AssetDatabase* database, Stream<wchar_t> file);

	ECSENGINE_API void SerializeAssetDatabase(const AssetDatabase* database, uintptr_t& buffer);

	ECSENGINE_API size_t SerializeAssetDatabaseSize(const AssetDatabase* database);

	// ------------------------------------------------------------------------------------------------------------

	ECSENGINE_API bool DeserializeAssetDatabase(AssetDatabase* database, Stream<wchar_t> file);

	ECSENGINE_API bool DeserializeAssetDatabase(AssetDatabase* database, uintptr_t& buffer);

	// Returns the total amount needed for the paths and the assets to be loaded into the database
	// Returns -1 if the version is invalid or the data is invalid
	ECSENGINE_API size_t DeserializeAssetDatabaseSize(uintptr_t buffer);

	// ------------------------------------------------------------------------------------------------------------

}