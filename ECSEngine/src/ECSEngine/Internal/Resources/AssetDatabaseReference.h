// ECS_REFLECT
#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "AssetMetadata.h"
#include "../../Utilities/Reflection/ReflectionMacros.h"

namespace ECSEngine {

	struct AssetDatabase;

	struct ECSENGINE_API ECS_REFLECT AssetDatabaseReference {
		AssetDatabaseReference();
		AssetDatabaseReference(AssetDatabase* database, AllocatorPolymorphic allocator);

		void AddMesh(unsigned int handle);

		void AddTexture(unsigned int handle);

		void AddGPUBuffer(unsigned int handle);

		void AddGPUSampler(unsigned int handle);

		void AddShader(unsigned int handle);

		void AddMaterial(unsigned int handle);

		void AddMisc(unsigned int handle);

		void AddAsset(unsigned int handle, ECS_ASSET_TYPE type);

		MeshMetadata* GetMesh(unsigned int index);

		TextureMetadata* GetTexture(unsigned int index);

		GPUBufferMetadata* GetGPUBuffer(unsigned int index);

		GPUSamplerMetadata* GetGPUSampler(unsigned int index);

		ShaderMetadata* GetShader(unsigned int index);

		MaterialAsset* GetMaterial(unsigned int index);

		MiscAsset* GetMisc(unsigned int index);

		void* GetAsset(unsigned int index, ECS_ASSET_TYPE type);

		unsigned int GetHandle(unsigned int index, ECS_ASSET_TYPE type);

		AssetDatabase* GetDatabase() const;

		// Returns true if the asset was evicted e.g. its reference count reached 0
		bool RemoveMesh(unsigned int index);

		// Returns true if the asset was evicted e.g. its reference count reached 0
		bool RemoveTexture(unsigned int index);

		// Returns true if the asset was evicted e.g. its reference count reached 0
		bool RemoveGPUBuffer(unsigned int index);

		// Returns true if the asset was evicted e.g. its reference count reached 0
		bool RemoveGPUSampler(unsigned int index);

		// Returns true if the asset was evicted e.g. its reference count reached 0
		bool RemoveShader(unsigned int index);

		// Returns true if the asset was evicted e.g. its reference count reached 0
		bool RemoveMaterial(unsigned int index);

		// Returns true if the asset was evicted e.g. its reference count reached 0
		bool RemoveMisc(unsigned int index);

		// Returns true if the asset was evicted e.g. its reference count reached 0
		bool RemoveAsset(unsigned int index, ECS_ASSET_TYPE type);

		// Increases the reference count of all assets by one
		void IncrementReferenceCounts();

		ResizableStream<unsigned int> mesh_metadata;
		ResizableStream<unsigned int> texture_metadata;
		ResizableStream<unsigned int> gpu_buffer_metadata;
		ResizableStream<unsigned int> gpu_sampler_metadata;
		ResizableStream<unsigned int> shader_metadata;
		ResizableStream<unsigned int> material_asset;
		ResizableStream<unsigned int> misc_asset;
		AssetDatabase* database; ECS_OMIT_FIELD_REFLECT
	};

}