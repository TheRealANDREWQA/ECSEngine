#include "ecspch.h"
#include "AssetDatabaseReference.h"
#include "AssetDatabase.h"

namespace ECSEngine {

	// ------------------------------------------------------------------------------------------------

	AssetDatabaseReference::AssetDatabaseReference() : database(nullptr) {}

	// ------------------------------------------------------------------------------------------------

	AssetDatabaseReference::AssetDatabaseReference(AssetDatabase* database, AllocatorPolymorphic allocator) : database(database)
	{
		mesh_metadata.Initialize(allocator, 0);
		texture_metadata.Initialize(allocator, 0);
		gpu_buffer_metadata.Initialize(allocator, 0);
		gpu_sampler_metadata.Initialize(allocator, 0);
		shader_metadata.Initialize(allocator, 0);
		material_asset.Initialize(allocator, 0);
		misc_asset.Initialize(allocator, 0);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::AddMesh(unsigned int handle)
	{
		AddAsset(handle, ECS_ASSET_MESH);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::AddTexture(unsigned int handle)
	{
		AddAsset(handle, ECS_ASSET_TEXTURE);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::AddGPUBuffer(unsigned int handle)
	{
		AddAsset(handle, ECS_ASSET_GPU_BUFFER);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::AddGPUSampler(unsigned int handle)
	{
		AddAsset(handle, ECS_ASSET_GPU_SAMPLER);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::AddShader(unsigned int handle)
	{
		AddAsset(handle, ECS_ASSET_SHADER);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::AddMaterial(unsigned int handle)
	{
		AddAsset(handle, ECS_ASSET_MATERIAL);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::AddMisc(unsigned int handle)
	{
		AddAsset(handle, ECS_ASSET_MISC);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::AddAsset(unsigned int handle, ECS_ASSET_TYPE type)
	{
		ResizableStream<unsigned int>* streams = (ResizableStream<unsigned int>*)this;
		streams[type].Add(handle);
	}

	// ------------------------------------------------------------------------------------------------

	MeshMetadata* AssetDatabaseReference::GetMesh(unsigned int index)
	{
		return (MeshMetadata*)GetAsset(index, ECS_ASSET_MESH);
	}

	// ------------------------------------------------------------------------------------------------

	TextureMetadata* AssetDatabaseReference::GetTexture(unsigned int index)
	{
		return (TextureMetadata*)GetAsset(index, ECS_ASSET_TEXTURE);
	}

	// ------------------------------------------------------------------------------------------------

	GPUBufferMetadata* AssetDatabaseReference::GetGPUBuffer(unsigned int index)
	{
		return (GPUBufferMetadata*)GetAsset(index, ECS_ASSET_GPU_BUFFER);
	}

	// ------------------------------------------------------------------------------------------------

	GPUSamplerMetadata* AssetDatabaseReference::GetGPUSampler(unsigned int index)
	{
		return (GPUSamplerMetadata*)GetAsset(index, ECS_ASSET_GPU_SAMPLER);
	}

	// ------------------------------------------------------------------------------------------------

	ShaderMetadata* AssetDatabaseReference::GetShader(unsigned int index)
	{
		return (ShaderMetadata*)GetAsset(index, ECS_ASSET_SHADER);
	}

	// ------------------------------------------------------------------------------------------------

	MaterialAsset* AssetDatabaseReference::GetMaterial(unsigned int index)
	{
		return (MaterialAsset*)GetAsset(index, ECS_ASSET_MATERIAL);
	}

	// ------------------------------------------------------------------------------------------------

	MiscAsset* AssetDatabaseReference::GetMisc(unsigned int index)
	{
		return (MiscAsset*)GetAsset(index, ECS_ASSET_MISC);
	}

	// ------------------------------------------------------------------------------------------------

	void* AssetDatabaseReference::GetAsset(unsigned int index, ECS_ASSET_TYPE type)
	{
		return database->GetAsset(GetHandle(index, type), type);
	}

	// ------------------------------------------------------------------------------------------------

	unsigned int AssetDatabaseReference::GetHandle(unsigned int index, ECS_ASSET_TYPE type)
	{
		ResizableStream<unsigned int>* streams = (ResizableStream<unsigned int>*)this;
		return streams[type][index];
	}

	// ------------------------------------------------------------------------------------------------

	AssetDatabase* AssetDatabaseReference::GetDatabase() const
	{
		return database;
	}

	// ------------------------------------------------------------------------------------------------

	bool AssetDatabaseReference::RemoveMesh(unsigned int index)
	{
		return RemoveAsset(index, ECS_ASSET_MESH);
	}

	// ------------------------------------------------------------------------------------------------

	bool AssetDatabaseReference::RemoveTexture(unsigned int index)
	{
		return RemoveAsset(index, ECS_ASSET_TEXTURE);
	}

	// ------------------------------------------------------------------------------------------------

	bool AssetDatabaseReference::RemoveGPUBuffer(unsigned int index)
	{
		return RemoveAsset(index, ECS_ASSET_GPU_BUFFER);
	}

	// ------------------------------------------------------------------------------------------------

	bool AssetDatabaseReference::RemoveGPUSampler(unsigned int index)
	{
		return RemoveAsset(index, ECS_ASSET_GPU_SAMPLER);
	}

	// ------------------------------------------------------------------------------------------------

	bool AssetDatabaseReference::RemoveShader(unsigned int index)
	{
		return RemoveAsset(index, ECS_ASSET_SHADER);
	}

	// ------------------------------------------------------------------------------------------------

	bool AssetDatabaseReference::RemoveMaterial(unsigned int index)
	{
		return RemoveAsset(index, ECS_ASSET_MATERIAL);
	}

	// ------------------------------------------------------------------------------------------------

	bool AssetDatabaseReference::RemoveMisc(unsigned int index)
	{
		return RemoveAsset(index, ECS_ASSET_MISC);
	}

	// ------------------------------------------------------------------------------------------------

	bool AssetDatabaseReference::RemoveAsset(unsigned int index, ECS_ASSET_TYPE type)
	{
		unsigned int handle = GetHandle(index, type);
		return database->RemoveAsset(handle, type);
	}

	// ------------------------------------------------------------------------------------------------

	void AssetDatabaseReference::IncrementReferenceCounts()
	{
		ResizableStream<unsigned int>* streams = (ResizableStream<unsigned int>*)this;

		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE asset_type = (ECS_ASSET_TYPE)index;

			for (unsigned int stream_index = 0; stream_index < streams[index].size; stream_index++) {
				unsigned int handle = GetHandle(stream_index, asset_type);
				database->AddAsset(handle, asset_type);
			}
		}
	}

	// ------------------------------------------------------------------------------------------------

	// ------------------------------------------------------------------------------------------------

	// ------------------------------------------------------------------------------------------------

}
