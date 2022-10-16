#pragma once
#include "AssetMetadata.h"
#include "../../Rendering/GraphicsHelpers.h"

namespace ECSEngine {
	
	struct ResourceManager;
	struct AssetDatabase;
	struct GLTFMesh;

	struct CreateAssetFromMetadataExData {
		SpinLock* manager_lock = nullptr;
		size_t time_stamp = 0;
		Stream<wchar_t> mount_point = { nullptr, 0 };
	};


#pragma region Create and Identifiers

	// Returns true if it managed to create the asset according to the metadata, else false
	ECSENGINE_API bool CreateMeshFromMetadata(
		ResourceManager* resource_manager, 
		const MeshMetadata* metadata, 
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	// A more detailed version. Useful for multithreaded loading
	// If the time stamp is 0, then it will get it from the OS
	ECSENGINE_API bool CreateMeshFromMetadataEx(
		ResourceManager* resource_manager,
		const MeshMetadata* metadata,
		Stream<GLTFMesh> meshes,
		CreateAssetFromMetadataExData* ex_data = {}
	);

	// The combination of settings that form an identifier
	ECSENGINE_API void MeshMetadataIdentifier(const MeshMetadata* metadata, CapacityStream<void>& identifier);

	// Returns true if it managed to create the asset according to the metadata, else false
	ECSENGINE_API bool CreateTextureFromMetadata(
		ResourceManager* resource_manager, 
		const TextureMetadata* metadata, 
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	// A more detailed version. Useful for multithreaded loading
	// If the time stamp is 0, then it will get it from the OS
	ECSENGINE_API bool CreateTextureFromMetadataEx(
		ResourceManager* resource_manager,
		const TextureMetadata* metadata,
		DecodedTexture texture,
		SpinLock* gpu_spin_lock = nullptr,
		CreateAssetFromMetadataExData* ex_data = {}
	);
	
	ECSENGINE_API void TextureMetadataIdentifier(const TextureMetadata* metadata, CapacityStream<void>& identifier);

	// Receives a resource manager instead of a graphics object in order to keep the API the same for all these types
	ECSENGINE_API void CreateSamplerFromMetadata(ResourceManager* resource_manager, GPUSamplerMetadata* metadata);

	// Returns true if it managed to create the asset according to the metadata, else false
	ECSENGINE_API bool CreateShaderFromMetadata(
		ResourceManager* resource_manager, 
		ShaderMetadata* metadata,
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	ECSENGINE_API bool CreateShaderFromMetadataEx(
		ResourceManager* resource_manager,
		ShaderMetadata* metadata,
		Stream<char> source_code,
		CreateAssetFromMetadataExData* ex_data = {}
	);

	ECSENGINE_API void ShaderMetadataIdentifier(const ShaderMetadata* metadata, CapacityStream<void>& identifier);

	// Returns true if it managed to create the asset according to the metadata, else false
	ECSENGINE_API bool CreateMaterialFromMetadata(
		ResourceManager* resource_manager, 
		AssetDatabase* asset_database,
		MaterialAsset* material, 
		Stream<wchar_t> mount_point = { nullptr, 0 },
		bool dont_load_referenced = false
	);

	ECSENGINE_API void ConvertMaterialAssetToUserMaterial(
		const AssetDatabase* asset_database,
		const MaterialAsset* material, 
		UserMaterial* user_material, 
		AllocatorPolymorphic allocator,
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	ECSENGINE_API bool CreateMiscAssetFromMetadata(
		ResourceManager* resource_manager,
		MiscAsset* misc_asset,
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	ECSENGINE_API bool CreateAssetFromMetadata(
		ResourceManager* resource_manager,
		AssetDatabase* database,
		void* asset,
		ECS_ASSET_TYPE type,
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

#pragma endregion

#pragma region Is Loaded

	ECSENGINE_API bool IsMeshFromMetadataLoaded(const ResourceManager* resource_manager, const MeshMetadata* metadata, Stream<wchar_t> mount_point = { nullptr, 0 });

	ECSENGINE_API bool IsTextureFromMetadataLoaded(const ResourceManager* resource_manager, const TextureMetadata* metadata, Stream<wchar_t> mount_point = { nullptr, 0 });

	ECSENGINE_API bool IsGPUSamplerFromMetadataLoaded(const GPUSamplerMetadata* metadata);

	ECSENGINE_API bool IsShaderFromMetadataLoaded(const ResourceManager* resource_manager, const ShaderMetadata* metadata, Stream<wchar_t> mount_point = { nullptr, 0 });
	
	ECSENGINE_API bool IsMaterialFromMetadataLoaded(const MaterialAsset* metadata);

	ECSENGINE_API bool IsMiscFromMetadataLoaded(const ResourceManager* resource_manager, const MiscAsset* metadata, Stream<wchar_t> mount_point = { nullptr, 0 });

	// There must be ECS_ASSET_TYPE_COUNT missing asset streams. Every type will write into its own stream
	// Use this version for the asset database but with the difference being that you must convert it to a standalone version first
	ECSENGINE_API void GetDatabaseMissingAssets(
		const ResourceManager* resource_manager,
		const AssetDatabase* database, 
		CapacityStream<unsigned int>* missing_handles, 
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	ECSENGINE_API bool HasMissingAssets(CapacityStream<unsigned int>* missing_handles);

#pragma endregion

#pragma region Deallocate

	// Returns true if the resource reference counted was decremented (by default it checks to see if it exists,
	// if it doesn't it does nothing)
	ECSENGINE_API bool DeallocateMeshFromMetadata(ResourceManager* resource_manager, MeshMetadata* metadata, Stream<wchar_t> mount_point = { nullptr, 0 });

	// Returns true if the resource reference counted was decremented (by default it checks to see if it exists,
	// if it doesn't it does nothing)
	ECSENGINE_API bool DeallocateTextureFromMetadata(ResourceManager* resource_manager, TextureMetadata* metadata, Stream<wchar_t> mount_point = { nullptr, 0 });

	ECSENGINE_API void DeallocateSamplerFromMetadata(ResourceManager* resource_manager, GPUSamplerMetadata* metadata);

	// Returns true if the resource reference counted was decremented (by default it checks to see if it exists,
	// if it doesn't it does nothing)
	ECSENGINE_API bool DeallocateShaderFromMetadata(ResourceManager* resource_manager, ShaderMetadata* metadata, Stream<wchar_t> mount_point = { nullptr, 0 });

	ECSENGINE_API void DeallocateMaterialFromMetadata(ResourceManager* resource_manager, MaterialAsset* material, const AssetDatabase* database, Stream<wchar_t> mount_point = { nullptr, 0 });

	// Returns true if the resource reference counted was decremented (by default it checks to see if it exists,
	// if it doesn't it does nothing)
	ECSENGINE_API bool DeallocateMiscAssetFromMetadata(ResourceManager* resource_manager, MiscAsset* misc, Stream<wchar_t> mount_point = { nullptr, 0 });
	
	// Returns true if the resource reference counted was decremented (by default it checks to see if it exists,
	// if it doesn't it does nothing). For materials and samplers it always returns true
	ECSENGINE_API bool DeallocateAssetFromMetadata(
		ResourceManager* resource_manager, 
		AssetDatabase* database, 
		unsigned int handle,
		ECS_ASSET_TYPE type, 
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

#pragma endregion

	ECSENGINE_API void SetShaderMetadataSourceCode(ShaderMetadata* metadata, Stream<char> source_code);

	ECSENGINE_API void SetShaderMetadataByteCode(ShaderMetadata* metadata, Stream<void> byte_code);

	ECSENGINE_API void SetMiscData(MiscAsset* misc_asset, Stream<void> data);

}