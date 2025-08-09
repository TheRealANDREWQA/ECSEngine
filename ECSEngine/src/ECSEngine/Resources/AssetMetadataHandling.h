#pragma once
#include "AssetMetadata.h"
#include "../Rendering/TextureOperations.h"
#include "../Utilities/StackScope.h"

namespace ECSEngine {
	
	struct ResourceManager;
	struct AssetDatabase;
	struct AssetDatabaseReference;
	struct GLTFMesh;

	struct CreateAssetFromMetadataExData {
		bool multithreaded = false;
		size_t time_stamp = 0;
		Stream<wchar_t> mount_point = { nullptr, 0 };
		
		// If the reference count is left at default it means that the resource is not reference counted
		unsigned short reference_count = USHORT_MAX;
	};


#pragma region Create and Identifiers

	// SINGLE THREADED
	// Returns true if it managed to create the asset according to the metadata, else false
	// It does not modify the underlying CoalescedMesh* pointer if it fails
	ECSENGINE_API bool CreateMeshFromMetadata(
		ResourceManager* resource_manager, 
		MeshMetadata* metadata, 
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	// A more detailed version. Useful for multithreaded loading
	// If the time stamp is 0, then it will get it from the OS
	// It does not modify the underlying CoalescedMesh* pointer if it fails
	ECSENGINE_API bool CreateMeshFromMetadataEx(
		ResourceManager* resource_manager,
		MeshMetadata* metadata,
		Stream<GLTFMesh> meshes,
		CreateAssetFromMetadataExData* ex_data = {}
	);

	// A more detailed version. Useful for multithreaded loading
	// If the time stamp is 0, then it will get it from the OS
	// It does not modify the underlying CoalescedMesh* pointer if it fails
	// This version cannot fail (unless the GPU memory runs out)
	ECSENGINE_API void CreateMeshFromMetadataEx(
		ResourceManager* resource_manager,
		MeshMetadata* metadata,
		const GLTFMesh* coalesced_mesh,
		Stream<Submesh> submeshes,
		CreateAssetFromMetadataExData* ex_data = {}
	);

	// The combination of settings that form an identifier
	ECSENGINE_API void MeshMetadataIdentifier(const MeshMetadata* metadata, CapacityStream<void>& identifier);

	// SINGLE THREADED
	// Returns true if it managed to create the asset according to the metadata, else false
	// It does not modify the underlying ResourceView if it fails
	ECSENGINE_API bool CreateTextureFromMetadata(
		ResourceManager* resource_manager, 
		TextureMetadata* metadata, 
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	// A more detailed version. Useful for multithreaded loading
	// If the time stamp is 0, then it will get it from the OS
	// It does not modify the underlying ResourceView if it fails
	ECSENGINE_API bool CreateTextureFromMetadataEx(
		ResourceManager* resource_manager,
		TextureMetadata* metadata,
		DecodedTexture texture,
		SpinLock* gpu_lock = nullptr,
		CreateAssetFromMetadataExData* ex_data = {}
	);
	
	ECSENGINE_API void TextureMetadataIdentifier(const TextureMetadata* metadata, CapacityStream<void>& identifier);

	// Receives a resource manager instead of a graphics object in order to keep the API the same for all these types
	ECSENGINE_API void CreateSamplerFromMetadata(ResourceManager* resource_manager, GPUSamplerMetadata* metadata);

	// SINGLE THREADED
	// Returns true if it managed to create the asset according to the metadata, else false
	// It does not modify the underlying Shader Interface if it fails
	ECSENGINE_API bool CreateShaderFromMetadata(
		ResourceManager* resource_manager, 
		ShaderMetadata* metadata,
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	// It does not modify the underlying Shader Interface if it fails
	ECSENGINE_API bool CreateShaderFromMetadataEx(
		ResourceManager* resource_manager,
		ShaderMetadata* metadata,
		Stream<char> source_code,
		CreateAssetFromMetadataExData* ex_data = {}
	);

	ECSENGINE_API void ShaderMetadataIdentifier(const ShaderMetadata* metadata, CapacityStream<void>& identifier);

	// SINGLE THREADED
	// Returns true if it managed to create the asset according to the metadata, else false
	// It does not modify the underlying Material pointer if it fails (if the pointer is nullptr
	// and it succeeds then it will make an allocation from the asset database allocator)
	ECSENGINE_API bool CreateMaterialFromMetadata(
		ResourceManager* resource_manager, 
		AssetDatabase* asset_database,
		MaterialAsset* material,
		Stream<wchar_t> mount_point = { nullptr, 0 },
		bool dont_load_referenced = false
	);

	// The given allocator must be a temporary allocator
	ECSENGINE_API void ConvertMaterialAssetToUserMaterial(
		const AssetDatabase* asset_database,
		const MaterialAsset* material, 
		UserMaterial* user_material, 
		AllocatorPolymorphic allocator,
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	// SINGLE THREADED
	// It does not modify the underlying Stream<void> from the misc asset if it fails.
	ECSENGINE_API bool CreateMiscAssetFromMetadata(
		ResourceManager* resource_manager,
		MiscAsset* misc_asset,
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	ECSENGINE_API void MiscMetadataIdentifier(const MiscAsset* misc, CapacityStream<void>& identifier);
	
	// SINGLE THREADED
	ECSENGINE_API bool CreateAssetFromMetadata(
		ResourceManager* resource_manager,
		AssetDatabase* database,
		void* asset,
		ECS_ASSET_TYPE type,
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	// Creates a unique identifier suffix for an asset such that for the same target file it can be loaded into the resource
	// manager without causing conflicts
	ECSENGINE_API void AssetMetadataIdentifier(const void* metadata, ECS_ASSET_TYPE type, CapacityStream<void>& identifier);

	// The file path + the identifier, if it has one. For materials and gpu samplers, which are not contained in the 
	// resource manager it will not fill in anything
	ECSENGINE_API void AssetMetadataResourceIdentifier(const void* metadata, ECS_ASSET_TYPE type, CapacityStream<void>& path_identifier, Stream<wchar_t> mount_point = { nullptr, 0 });

	// Returns the asset that is being stored in the resource manager. For assets that are not stored, it will return the asset
	// that is being stored in the metadata
	ECSENGINE_API Stream<void> AssetFromResourceManager(const ResourceManager* resource_manager, const void* metadata, ECS_ASSET_TYPE type, Stream<wchar_t> mount_point = { nullptr, 0 });

#pragma endregion

#pragma region Is Loaded

	// For randomized assets it will test differently
	ECSENGINE_API bool IsMeshFromMetadataLoaded(
		const ResourceManager* resource_manager, 
		const MeshMetadata* metadata, 
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	// The same as the other override, but it assigns to the metadata pointer the resource manager value
	// In case the metadata pointer is not set, but it appears in the resource manager. You can be notified
	// If the assignment was performed by the last out parameter
	ECSENGINE_API bool IsMeshFromMetadataLoadedAndAssign(
		const ResourceManager* resource_manager,
		MeshMetadata* metadata,
		Stream<wchar_t> mount_point = { nullptr, 0 },
		bool* has_assigned = nullptr
	);

	// For randomized assets it will test differently
	ECSENGINE_API bool IsTextureFromMetadataLoaded(
		const ResourceManager* resource_manager, 
		const TextureMetadata* metadata, 
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	// The same as the other override, but it assigns to the metadata pointer the resource manager value
	// In case the metadata pointer is not set, but it appears in the resource manager. You can be notified
	// If the assignment was performed by the last out parameter
	ECSENGINE_API bool IsTextureFromMetadataLoadedAndAssign(
		const ResourceManager* resource_manager,
		TextureMetadata* metadata,
		Stream<wchar_t> mount_point = { nullptr, 0 },
		bool* has_assigned = nullptr
	);

	ECSENGINE_API bool IsGPUSamplerFromMetadataLoaded(const GPUSamplerMetadata* metadata, bool randomized_asset = false);

	ECSENGINE_API bool IsShaderFromMetadataLoaded(
		const ResourceManager* resource_manager, 
		const ShaderMetadata* metadata, 
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	// The same as the other override, but it assigns to the metadata pointer the resource manager value
	// In case the metadata pointer is not set, but it appears in the resource manager. You can be notified
	// If the assignment was performed by the last out parameter
	ECSENGINE_API bool IsShaderFromMetadataLoadedAndAssign(
		const ResourceManager* resource_manager,
		ShaderMetadata* metadata,
		Stream<wchar_t> mount_point = { nullptr, 0 },
		bool* has_assigned = nullptr
	);
	
	ECSENGINE_API bool IsMaterialFromMetadataLoaded(const MaterialAsset* metadata, bool randomized_asset = false);
	
	// If the segmented error string is given, then the error string must be specified as well
	// If the missing_handles contains only a single element with asset type of material then it means
	// that the dependencies are ready but the material itself is not loaded
	struct IsMaterialFromMetadataLoadedExDesc {
		Stream<wchar_t> mount_point = { nullptr, 0 };
		CapacityStream<char>* error_string = nullptr;
		CapacityStream<AssetTypedHandle>* missing_handles = nullptr;
		CapacityStream<Stream<char>>* segmented_error_string = nullptr;
		bool dependencies_are_ready = false;
	};

	// A different version which goes the assigned textures and shaders and determines if
	// the dependencies are also loaded. If the segmented error string is specified, the first element
	// will always be the description of the error
	ECSENGINE_API bool IsMaterialFromMetadataLoadedEx(
		const MaterialAsset* metadata, 
		const ResourceManager* resource_manager, 
		const AssetDatabase* database,
		IsMaterialFromMetadataLoadedExDesc* load_desc = nullptr
	);

	// A different version which goes the assigned textures and shaders and determines if
	// the dependencies are also loaded. It verifies the pointer value of the asset only (it doesn't verify in the resource manager).
	// If the segmented error string is specified, the first element will always be the description of the error
	ECSENGINE_API bool IsMaterialFromMetadataLoadedEx(
		const MaterialAsset* metadata,
		const AssetDatabase* database,
		IsMaterialFromMetadataLoadedExDesc* load_desc = nullptr
	);

	ECSENGINE_API bool IsMiscFromMetadataLoaded(
		const ResourceManager* resource_manager, 
		const MiscAsset* metadata, 
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	// The same as the other override, but it assigns to the metadata pointer the resource manager value
	// In case the metadata pointer is not set, but it appears in the resource manager. You can be notified
	// If the assignment was performed by the last out parameter
	ECSENGINE_API bool IsMiscFromMetadataLoadedAndAssign(
		const ResourceManager* resource_manager,
		MiscAsset* metadata,
		Stream<wchar_t> mount_point = { nullptr, 0 },
		bool* has_assigned = nullptr
	);

	ECSENGINE_API bool IsAssetFromMetadataLoaded(
		const ResourceManager* resource_manager, 
		const void* metadata, 
		ECS_ASSET_TYPE type, 
		Stream<wchar_t> mount_point = { nullptr, 0 },
		bool randomized_asset = false
	);

	// The same as the other override, but it assigns to the metadata pointer the resource manager value
	// In case the metadata pointer is not set, but it appears in the resource manager. You can be notified
	// If the assignment was performed by the last out parameter
	ECSENGINE_API bool IsAssetFromMetadataLoadedAndAssign(
		const ResourceManager* resource_manager,
		void* metadata,
		ECS_ASSET_TYPE type,
		Stream<wchar_t> mount_point = { nullptr, 0 },
		bool randomized_asset = false,
		bool* has_assigned = nullptr
	);

	// There must be ECS_ASSET_TYPE_COUNT missing asset streams. Every type will write into its own stream
	ECSENGINE_API void GetDatabaseMissingAssets(
		const ResourceManager* resource_manager,
		const AssetDatabase* database, 
		CapacityStream<unsigned int>* missing_handles, 
		Stream<wchar_t> mount_point = { nullptr, 0 },
		bool randomized_assets = false
	);

	// There must be ECS_ASSET_TYPE_COUNT missing asset streams. Every type will write into its own stream
	ECSENGINE_API void GetDatabaseMissingAssets(
		const ResourceManager* resource_manager,
		const AssetDatabaseReference* database,
		CapacityStream<unsigned int>* missing_handles,
		Stream<wchar_t> mount_point = { nullptr, 0 },
		bool randomized_assets = false
	);

	ECSENGINE_API bool HasMissingAssets(CapacityStream<unsigned int>* missing_handles);

#pragma endregion

#pragma region Deallocate

	// SINGLE THREADED
	// Returns true if the resource reference counted was decremented (by default it checks to see if it exists,
	// if it doesn't it does nothing)
	ECSENGINE_API bool DeallocateMeshFromMetadata(ResourceManager* resource_manager, const MeshMetadata* metadata, Stream<wchar_t> mount_point = { nullptr, 0 });

	// SINGLE THREADED
	// Returns true if the resource reference counted was decremented (by default it checks to see if it exists,
	// if it doesn't it does nothing)
	ECSENGINE_API bool DeallocateTextureFromMetadata(ResourceManager* resource_manager, const TextureMetadata* metadata, Stream<wchar_t> mount_point = { nullptr, 0 });

	// Can be used by from multiple threads
	ECSENGINE_API void DeallocateSamplerFromMetadata(ResourceManager* resource_manager, const GPUSamplerMetadata* metadata);

	// SINGLE THREADED
	// Returns true if the resource reference counted was decremented (by default it checks to see if it exists,
	// if it doesn't it does nothing)
	ECSENGINE_API bool DeallocateShaderFromMetadata(ResourceManager* resource_manager, const ShaderMetadata* metadata, Stream<wchar_t> mount_point = { nullptr, 0 });

	// SINGLE THREADED
	// If the check_resource is set to true, it will check to see that the texture/shader exists in the resource
	// manager and attempt to unload if it does.
	// The last parameter, unload_resources, can be used to disable unloading the other assets that this material references.
	// By default, it will unload them, but you can opt out of it
	ECSENGINE_API void DeallocateMaterialFromMetadata(
		ResourceManager* resource_manager, 
		const MaterialAsset* material, 
		const AssetDatabase* database, 
		Stream<wchar_t> mount_point = { nullptr, 0 },
		bool check_resource = false,
		bool unload_resources = true
	);

	// SINGLE THREADED
	// Returns true if the resource reference counted was decremented (by default it checks to see if it exists,
	// if it doesn't it does nothing)
	ECSENGINE_API bool DeallocateMiscAssetFromMetadata(ResourceManager* resource_manager, const MiscAsset* misc, Stream<wchar_t> mount_point = { nullptr, 0 });
	
	// These are options that apply only to certain asset types
	struct DeallocateAssetFromMetadataOptions {
		bool material_check_resource = false;
		bool material_unload_resources = true;
	};

	// SINGLE THREADED
	// Returns true if the resource reference counted was decremented (by default it checks to see if it exists,
	// if it doesn't it does nothing). For materials and samplers it always returns true
	ECSENGINE_API bool DeallocateAssetFromMetadata(
		ResourceManager* resource_manager, 
		const AssetDatabase* database, 
		const void* metadata,
		ECS_ASSET_TYPE type, 
		Stream<wchar_t> mount_point = { nullptr, 0 },
		DeallocateAssetFromMetadataOptions options = {}
	);

#pragma endregion

#pragma region Reload functions

	// SINGLE THREADED
	// It will reload the entire asset. In the x return true if the deallocation was successful
	// and in the y if the creation was successful
	ECSENGINE_API bool2 ReloadAssetFromMetadata(
		ResourceManager* resource_manager,
		AssetDatabase* database,
		void* metadata,
		ECS_ASSET_TYPE type,
		Stream<wchar_t> mount_point = { nullptr, 0 },
		CapacityStream<AssetTypedHandle>* remove_dependencies = nullptr
	);

	// In the x component of the success is the success state for deallocation
	// and in the y for the creation
	struct ReloadAssetResult {
		bool is_different;
		bool2 success;
	};

	// SINGLE THREADED
	// It will calculate the difference between the assets and then perform a minimal reload
	// (For assets that have dependencies if they have not changed then it will not unload them
	// and then reload them). It returns the compare result and the success status (this is valid
	// only when the assets are different, if they are the same nothing will be performed)
	// At the moment, for materials it won't unload its dependencies from the resource manager/asset database.
	// You must do that manually
	ECSENGINE_API ReloadAssetResult ReloadAssetFromMetadata(
		ResourceManager* resource_manager,
		AssetDatabase* database,
		const void* previous_metadata,
		void* current_metadata,
		ECS_ASSET_TYPE type,
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	enum ECS_RELOAD_ASSET_METADATA_STATUS : unsigned char {
		ECS_RELOAD_ASSET_METADATA_NO_CHANGE,
		ECS_RELOAD_ASSET_METADATA_FAILED_READING, // Failed to read the target file
		ECS_RELOAD_ASSET_METADATA_CHANGED,
		ECS_RELOAD_ASSET_METADATA_STATUS_COUNT
	};

	struct ChangeMaterialDependencies {
		// The database is mandatory
		AssetDatabase* database;
		// If this is specified, instead of removing the dependencies right then,
		// it will fill in this buffer instead
		CapacityStream<AssetTypedHandle>* handles = nullptr;
	};

	// SINGLE THREADED
	// Reloads the buffers, textures and samplers to conform to the new shader file
	// Returns the status of the reload.
	// Remove dependencies can be specified such that when an asset with dependencies has one of
	// its dependencies removed they will be filled in instead of removed directly
	ECSENGINE_API ECS_RELOAD_ASSET_METADATA_STATUS ReloadMaterialMetadataFromShader(
		ResourceManager* resource_manager,
		AssetDatabase* database,
		MaterialAsset* material,
		const ShaderMetadata* shader,
		Stream<wchar_t> mount_point = { nullptr, 0 },
		CapacityStream<AssetTypedHandle>* remove_dependencies = nullptr
	);
		
	// Updates the metadata to conform to the changed_metadata target file content
	// Returns the status of the reload
	// Remove dependencies can be specified such that when an asset with dependencies has one of
	// its dependencies removed they will be filled in instead of removed directly
	ECSENGINE_API ECS_RELOAD_ASSET_METADATA_STATUS ReloadAssetMetadataFromTargetDependency(
		ResourceManager* resource_manager,
		AssetDatabase* database,
		void* metadata,
		ECS_ASSET_TYPE type,
		const void* changed_metadata,
		ECS_ASSET_TYPE changed_metadata_type,
		Stream<wchar_t> mount_point = { nullptr, 0 },
		CapacityStream<AssetTypedHandle>* remove_handles = nullptr
	);

#pragma endregion

	// When force addition is set to false it will set the handle to -1 if it doesn't find it
	// Else it will add the dependency to the database
	struct ConvertAssetFromDatabaseToDatabaseOptions {
		bool handles_only = true;
		bool force_addition = false;

		// This is used for all assets besides materials
		// When set to true it will simply memcpy instead of creating a copy
		bool do_not_copy_with_allocator = false;
	};

	// Options:
	// If the handles_only is set to true, then the material name and the other names are not allocated
	// When handles_only is set to false, for constant buffers it will allocate the data as well, the reflection type
	// is not copied tho
	ECSENGINE_API void ConvertMaterialFromDatabaseToDatabase(
		const MaterialAsset* material,
		MaterialAsset* output_material,
		const AssetDatabase* original_database,
		AssetDatabase* target_database,
		AllocatorPolymorphic allocator,
		ConvertAssetFromDatabaseToDatabaseOptions options = {}
	);

	// For meshes, textures, samplers, shaders and miscs it will simply copy the asset using the allocator
	// For materials it will have to go through a conversion process
	ECSENGINE_API void ConvertAssetFromDatabaseToDatabase(
		const void* source_asset,
		void* target_asset,
		const AssetDatabase* original_database,
		AssetDatabase* target_database,
		AllocatorPolymorphic allocator,
		ECS_ASSET_TYPE asset_type,
		ConvertAssetFromDatabaseToDatabaseOptions options = {}
	);

	// Returns true if there is any difference between the two versions of the buffers, else false
	// If there is no difference, no allocation will take place
	ECSENGINE_API bool ChangeMaterialBuffersFromReflectedParameters(
		MaterialAsset* material,
		const ReflectedShader* reflected_shader,
		ECS_MATERIAL_SHADER shader,
		AllocatorPolymorphic allocator
	);

	// Returns true if there is any difference between the two versions of the textures, else false
	// If there is no difference, no allocation will take place. The database is needed to remove
	// existing textures that are no longer in use 
	ECSENGINE_API bool ChangeMaterialTexturesFromReflectedParameters(
		ChangeMaterialDependencies* change_dependencies,
		MaterialAsset* material,
		const ReflectedShader* reflected_shader,
		ECS_MATERIAL_SHADER shader,
		AllocatorPolymorphic allocator
	);

	// Returns true if there is any difference between the two versions of the samplers, else false
	// If there is no difference, no allocation will take place. The database is needed to remove
	// existing samplers that are no longer in use
	ECSENGINE_API bool ChangeMaterialSamplersFromReflectedParameters(
		ChangeMaterialDependencies* change_dependencies,
		MaterialAsset* material,
		const ReflectedShader* reflected_shader,
		ECS_MATERIAL_SHADER shader,
		AllocatorPolymorphic allocator
	);

	// Returns true if there is any difference between the two versions of the buffers, textures or samplers, else false
	// If there is no difference, no allocation will take place
	ECSENGINE_API bool ChangeMaterialFromReflectedShader(
		ChangeMaterialDependencies* change_dependencies,
		MaterialAsset* material,
		const ReflectedShader* reflected_shader,
		ECS_MATERIAL_SHADER shader,
		AllocatorPolymorphic allocator
	);

	// Returns the status of the reload
	ECSENGINE_API ECS_RELOAD_ASSET_METADATA_STATUS ReflectMaterialShaderParameters(
		const ResourceManager* resource_manager,
		ChangeMaterialDependencies* change_dependencies,
		MaterialAsset* material,
		const ShaderMetadata* shader_metadata,
		ECS_MATERIAL_SHADER shader_type,
		AllocatorPolymorphic allocator,
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	// Returns the status of the reload
	ECSENGINE_API ECS_RELOAD_ASSET_METADATA_STATUS ReloadMaterialShaderParameters(
		const ResourceManager* resource_manager,
		ChangeMaterialDependencies* change_dependencies,
		MaterialAsset* material,
		AllocatorPolymorphic allocator,
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	// Returns the status of the reload
	ECSENGINE_API ECS_RELOAD_ASSET_METADATA_STATUS ReloadMaterialMetadataFromFilesParameters(
		const ResourceManager* resource_manager,
		ChangeMaterialDependencies* change_dependencies,
		MaterialAsset* material,
		AllocatorPolymorphic allocator,
		Stream<wchar_t> mount_point = { nullptr, 0 }
	);

	struct ReloadAssetMetadataFromFilesOptions {
		ChangeMaterialDependencies material_change_dependencies;
		Stream<wchar_t> mount_point = { nullptr, 0 };
		CapacityStream<AssetTypedHandle>* modified_assets = nullptr;
	};

	// Returns the status of the reload
	ECSENGINE_API ECS_RELOAD_ASSET_METADATA_STATUS ReloadAssetMetadataFromFilesParameters(
		const ResourceManager* resource_manager,
		const AssetDatabase* database,
		void* metadata,
		ECS_ASSET_TYPE type,
		AllocatorPolymorphic allocator,
		ReloadAssetMetadataFromFilesOptions* options
	);

	// It will read all assets whose metadata depeneds on target files that are currently loaded and
	// reload them - if they have changed, they will be reflected afterwards
	// Can optionally give a stream of typed handles to be filled in for those assets whose
	// content has changed.
	// Returns true if all file reads were successful, else false (at least one failed)
	ECSENGINE_API bool ReloadAssetMetadataFromFilesParameters(
		const ResourceManager* resource_manager,
		AssetDatabase* database,
		ReloadAssetMetadataFromFilesOptions* options
	);

	// It will read all assets whose metadata depeneds on target files that are currently loaded and
	// reload them - if they have changed, they will be reflected afterwards
	// Can optionally give a stream of typed handles to be filled in for those assets whose
	// content has changed.
	// Returns true if all file reads were successful, else false (at least one failed)
	ECSENGINE_API bool ReloadAssetMetadataFromFilesParameters(
		const ResourceManager* resource_manager,
		AssetDatabaseReference* database_reference,
		ReloadAssetMetadataFromFilesOptions* options
	);

	// The shaders should already be set to the PBR shaders
	// It will modify the macros for the pixel shader such that
	// they match the textures from the pbr material. It will also
	// set the constant buffers to match the pbr_material factors
	// It will modify the metadata for the material and shaders and
	// then write to file these changes. The new allocations will be
	// made from the given allocator
	// Returns true if it succeeded, else false
	ECSENGINE_API bool CreateMaterialAssetFromPBRMaterial(
		AssetDatabase* database,
		MaterialAsset* material_asset,
		const PBRMaterial* pbr_material,
		AllocatorPolymorphic allocator
	);

	//ECS_INLINE void SetShaderMetadataSourceCode(ShaderMetadata* metadata, Stream<char> source_code) {
		//metadata->source_code = source_code;
	//}

	//ECS_INLINE void SetShaderMetadataByteCode(ShaderMetadata* metadata, Stream<void> byte_code) {
		//metadata->byte_code = byte_code;
	//}

	ECS_INLINE void SetMiscData(MiscAsset* misc_asset, Stream<void> data) {
		misc_asset->data = data;
	}

	// Protects all the assets from the database into the resource manager, such that they cannot be unloaded
	ECSENGINE_API void ProtectAssetDatabaseResources(const AssetDatabase* database, ResourceManager* resource_manager);

	// Unprotects all the assets from the database into the resource manager
	ECSENGINE_API void UnprotectAssetDatabaseResources(const AssetDatabase* database, ResourceManager* resource_manager);

	// If you want to protect the GPU from being used by multiple threads at the same time, you can call this
	// Function and it will lock the GPU only if the asset actually requires it, else it won't touch it
	ECSENGINE_API void AssetTypeLockGPU(ECS_ASSET_TYPE type, SpinLock* gpu_lock);

	// If you want to protect the GPU from being used by multiple threads at the same time, you can call this
	// Function and it will unlock the GPU only if the asset actually requires it, else it won't touch it
	ECSENGINE_API void AssetTypeUnlockGPU(ECS_ASSET_TYPE type, SpinLock* gpu_lock);

	// Helper that based upon a boolean will protect/unprotect the resources in the resource manager
	// From the asset database. They will be unprotected upon leaving the scope, using a stack scope.
	// Database and resource manager must be pointers.
#define ECS_PROTECT_UNPROTECT_ASSET_DATABASE_RESOURCES(should_protect, database, resource_manager) \
	bool ___should_protect = should_protect; /* Use a temporary to hold the resource manager in order to capture it in the lambda */ \
	ResourceManager* ___resource_manager = (resource_manager); /* Use a temporary to hold the resource manager in order to capture it in the lambda */ \
	const AssetDatabase* ___asset_database = (database); /* Use a temporary to hold the resource manager in order to capture it in the lambda */ \
	auto __unprotect_database_resources = StackScope([___should_protect, ___asset_database, ___resource_manager]() { \
		if (___should_protect) { \
			UnprotectAssetDatabaseResources(___asset_database, ___resource_manager); \
		} \
	}); \
	if (___should_protect) {\
		ProtectAssetDatabaseResources(___asset_database, ___resource_manager); \
	}

}