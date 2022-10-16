// ECS_REFLECT
#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../../Containers/SparseSet.h"
#include "AssetMetadata.h"
#include "../../Utilities/Reflection/ReflectionMacros.h"
#include "../../Utilities/Reflection/Reflection.h"

namespace ECSEngine {

	// Other_handles contains the handle as the first element
	struct AssetDatabaseSameTargetAsset {
		unsigned int handle;
		Stream<unsigned int> other_handles;
	};

	struct AssetDatabaseSameTargetAll {
		Stream<AssetDatabaseSameTargetAsset> meshes;
		Stream<AssetDatabaseSameTargetAsset> textures;
		Stream<AssetDatabaseSameTargetAsset> shaders;
		Stream<AssetDatabaseSameTargetAsset> miscs;
	};

	// The last character is a path separator
	ECSENGINE_API void AssetDatabaseFileDirectory(Stream<wchar_t> file_location, CapacityStream<wchar_t>& path, ECS_ASSET_TYPE type);

	// If the file location is not set, when adding the resources, no file is generated
	struct ECSENGINE_API ECS_REFLECT AssetDatabase {		
		AssetDatabase() = default;
		AssetDatabase(AllocatorPolymorphic allocator, const Reflection::ReflectionManager* reflection_manager);

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(AssetDatabase);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddMesh(Stream<char> name, Stream<wchar_t> file, bool* loaded_now = nullptr);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddMeshInternal(const MeshMetadata* metadata);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddTexture(Stream<char> name, Stream<wchar_t> file, bool* loaded_now = nullptr);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddTextureInternal(const TextureMetadata* metadata);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddGPUSampler(Stream<char> name, bool* loaded_now = nullptr);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddGPUSamplerInternal(const GPUSamplerMetadata* metadata);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddShader(Stream<char> name, Stream<wchar_t> file, bool* loaded_now = nullptr);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddShaderInternal(const ShaderMetadata* metadata);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddMaterial(Stream<char> name, bool* loaded_now = nullptr);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddMaterialInternal(const MaterialAsset* metadata);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddMisc(Stream<char> name, Stream<wchar_t> file, bool* loaded_now = nullptr);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddMiscInternal(const MiscAsset* metadata);

		// It returns the handle to that asset. If it fails it returns -1
		// The file is ignored for materials and samplers
		unsigned int AddAsset(Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type, bool* loaded_now = nullptr);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddAssetInternal(const void* asset, ECS_ASSET_TYPE type);

		// It increments by one the reference count for that asset.
		void AddAsset(unsigned int handle, ECS_ASSET_TYPE type);

		// Retrive the allocator for this database
		AllocatorPolymorphic Allocator() const;

		// Creates a new standalone database with the given allocator and the given handles
		AssetDatabase Copy(Stream<unsigned int>* handle_mask, AllocatorPolymorphic allocator) const;

		// Creates a new standalone database with the given allocator and the given handles
		AssetDatabase Copy(CapacityStream<unsigned int>* handle_mask, AllocatorPolymorphic allocator) const;

		// Creates all the necessary folders for the metadata
		// Returns true if all the folders exist, else false (a failure happened during the creation of the folders)
		bool CreateMetadataFolders() const;

		// In case the database is not yet initialized
		static bool CreateMetadataFolders(Stream<wchar_t> file_location);

		// Deallocates all the streams (but it does not deallocate the individual assets)
		void DeallocateStreams();

		// Returns the handle to that asset. Returns -1 if it doesn't exist
		unsigned int FindMesh(Stream<char> name, Stream<wchar_t> file) const;

		// Returns the handle to that asset. Returns -1 if it doesn't exist
		unsigned int FindTexture(Stream<char> name, Stream<wchar_t> file) const;

		// Returns the handle to that asset. Returns -1 if it doesn't exist
		unsigned int FindGPUSampler(Stream<char> name) const;

		// Returns the handle to that asset. Returns -1 if it doesn't exist
		unsigned int FindShader(Stream<char> name, Stream<wchar_t> file) const;

		// Returns the handle to that asset. Returns -1 if it doesn't exist
		unsigned int FindMaterial(Stream<char> name) const;

		// Returns the handle to that asset. Returns -1 if it doesn't exist
		unsigned int FindMisc(Stream<char> name, Stream<wchar_t> file) const;

		// Returns the handle to that asset. Returns -1 if it doesn't exist
		// For misc asset, the path needs to be casted into a char type
		unsigned int FindAsset(Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type) const;

		// Searches an asset based upon its actual data. Returns -1 if it doesn't find it.
		unsigned int FindMeshEx(const CoallescedMesh* mesh) const;

		// Searches an asset based upon its actual data. Returns -1 if it doesn't find it.
		unsigned int FindTextureEx(ResourceView resource_view) const;

		// Searches an asset based upon its actual data. Returns -1 if it doesn't find it.
		unsigned int FindGPUSamplerEx(SamplerState sampler_state) const;

		// Searches an asset based upon its actual data. Returns -1 if it doesn't find it.
		unsigned int FindShaderEx(const void* shader_interface) const;

		// Searches an asset based upon its actual data. Returns -1 if it doesn't find it.
		unsigned int FindMaterialEx(const Material* material) const;

		// Searches an asset based upon its actual data. Returns -1 if it doesn't find it.
		unsigned int FindMiscEx(Stream<void> data) const;

		// Searches an asset based upon its actual data. Returns -1 if it doesn't find it.
		// For shaders, textures and gpu samplers the data.buffer needs to be the interface
		unsigned int FindAssetEx(Stream<void> data, ECS_ASSET_TYPE type) const;

		void FileLocationMesh(Stream<char> name, Stream<wchar_t> file, CapacityStream<wchar_t>& path) const;

		void FileLocationTexture(Stream<char> name, Stream<wchar_t> file, CapacityStream<wchar_t>& path) const;

		// Here the target path is ignored. It is kept in order to have the same API for all type of assets
		void FileLocationGPUSampler(Stream<char> name, CapacityStream<wchar_t>& path) const;

		void FileLocationShader(Stream<char> name, Stream<wchar_t> file, CapacityStream<wchar_t>& path) const;

		// Here the target path is ignored. It is kept in order to have the same API for all type of assets
		void FileLocationMaterial(Stream<char> name, CapacityStream<wchar_t>& path) const;

		void FileLocationMisc(Stream<char> name, Stream<wchar_t> file, CapacityStream<wchar_t>& path) const;

		void FileLocationAsset(Stream<char> name, Stream<wchar_t> file, CapacityStream<wchar_t>& path, ECS_ASSET_TYPE type) const;

		// Retrieves all the asset names for a certain type. They will reference the names stored inside
		void FillAssetNames(ECS_ASSET_TYPE type, CapacityStream<Stream<char>>& names) const;

		// Retrieves all the asset paths for a certain type (The type must have paths). They will reference the paths stored inside
		void FillAssetPaths(ECS_ASSET_TYPE type, CapacityStream<Stream<wchar_t>>& paths) const;

		MeshMetadata* GetMesh(unsigned int handle);

		const MeshMetadata* GetMeshConst(unsigned int handle) const;

		TextureMetadata* GetTexture(unsigned int handle);

		const TextureMetadata* GetTextureConst(unsigned int handle) const;
 
		GPUSamplerMetadata* GetGPUSampler(unsigned int handle);

		const GPUSamplerMetadata* GetGPUSamplerConst(unsigned int handle) const;

		// Can operate on it afterwards, like adding, removing or updating macros
		ShaderMetadata* GetShader(unsigned int handle);

		const ShaderMetadata* GetShaderConst(unsigned int handle) const;

		// Can operate on it afterwards, like adding or removing resources
		MaterialAsset* GetMaterial(unsigned int handle);

		const MaterialAsset* GetMaterialConst(unsigned int handle) const;

		MiscAsset* GetMisc(unsigned int handle);

		const MiscAsset* GetMiscConst(unsigned int handle) const;

		void* GetAsset(unsigned int handle, ECS_ASSET_TYPE type);
		
		const void* GetAssetConst(unsigned int handle, ECS_ASSET_TYPE type) const;

		Stream<char> GetAssetName(unsigned int handle, ECS_ASSET_TYPE type) const;

		// For samplers and materials which don't have a file it returns { nullptr, 0 }
		Stream<wchar_t> GetAssetPath(unsigned int handle, ECS_ASSET_TYPE type) const;

		// This returns the handles that have the given file as target. (it does not check the disk files)
		void GetMetadatasForFile(Stream<wchar_t> file, ECS_ASSET_TYPE type, CapacityStream<unsigned int>& handles) const;

		// To deallocate you must deallocate every index and then the main buffer
		// This uses the disk to determine the available metadatas.
		Stream<Stream<char>> GetMetadatasForFile(Stream<wchar_t> file, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator) const;

		// Retrieves all the metadata files for a certain type. You need to extract the name and the file manually
		Stream<Stream<wchar_t>> GetMetadatasForType(ECS_ASSET_TYPE type, AllocatorPolymorphic allocator) const;

		// It does not set the name or the file
		bool ReadMeshFile(Stream<char> name, Stream<wchar_t> file, MeshMetadata* metadata) const;

		// It does not set the name or the file
		bool ReadTextureFile(Stream<char> name, Stream<wchar_t> file, TextureMetadata* metadata) const;

		// It does not set the name or the file
		bool ReadGPUSamplerFile(Stream<char> name, GPUSamplerMetadata* metadata) const;

		// It does not set the name or the file
		bool ReadShaderFile(Stream<char> name, Stream<wchar_t> file, ShaderMetadata* metadata) const;

		// It does not set the name or the file
		bool ReadMaterialFile(Stream<char> name, MaterialAsset* asset) const;

		// It does not set the name or the file
		bool ReadMiscFile(Stream<char> name, Stream<wchar_t> file, MiscAsset* asset) const;

		// It does not set the name or the file
		bool ReadAssetFile(Stream<char> name, Stream<wchar_t> file, void* metadata, ECS_ASSET_TYPE asset_type) const;

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveMesh(unsigned int handle);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveTexture(unsigned int handle);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveGPUSampler(unsigned int handle);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveShader(unsigned int handle);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveMaterial(unsigned int handle);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveMisc(unsigned int handle);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveAsset(unsigned int handle, ECS_ASSET_TYPE type);

		// Does not destroy the file. It will remove the asset no matter what the reference count is
		void RemoveAssetForced(unsigned int handle, ECS_ASSET_TYPE type);

		// Only sets the allocator for the streams, it does not copy the already existing data
		// (it should not be used for that purpose)
		void SetAllocator(AllocatorPolymorphic allocator);

		void SetFileLocation(Stream<wchar_t> file_location);

		// Fills in the handles of the "unique" meshes - only the meshes with different settings
		// are selected. This is in order to avoid a different mesh but with the same target for processing
		void UniqueMeshes(CapacityStream<unsigned int>& handles) const;

		// Fills in the handles of the "unique" textures - only the textures with different settings
		// are selected. This is in order to avoid a different textures but with the same target for processing
		void UniqueTextures(CapacityStream<unsigned int>& handles) const;

		// Fills in the handles of the "unique" shaders - only the meshes with different settings
		// are selected. This is in order to avoid a different shaders but with the same target for processing
		void UniqueShaders(CapacityStream<unsigned int>& handles) const;

		// Fills in the handles of the "unique" miscs - only the miscs with different settings
		// are selected. This is in order to avoid a different miscs but with the same target for processing
		void UniqueMiscs(CapacityStream<unsigned int>& handles) const;

		// Changes the settings of an asset with the newly given.
		// It will adjust the buffers accordingly. It returns true when update files is false
		// and when the latter is set to true, it returns true when the files were updated successfully.
		// In case it fails, the in memory version will be the update one. If changing the name,
		// this does not verify that the new name doesn't exist previously
		bool UpdateMesh(unsigned int handle, const MeshMetadata* metadata, bool update_files = true);

		// Changes the settings of an asset with the newly given.
		// It will adjust the buffers accordingly. It returns true when update files is false
		// and when the latter is set to true, it returns true when the files were updated successfully.
		// In case it fails, the in memory version will be the update one. If changing the name,
		// this does not verify that the new name doesn't exist previously
		bool UpdateTexture(unsigned int handle, const TextureMetadata* metadata, bool update_files = true);

		// Changes the settings of an asset with the newly given.
		// It will adjust the buffers accordingly. It returns true when update files is false
		// and when the latter is set to true, it returns true when the files were updated successfully.
		// In case it fails, the in memory version will be the update one. If changing the name,
		// this does not verify that the new name doesn't exist previously
		bool UpdateGPUSampler(unsigned int handle, const GPUSamplerMetadata* metadata, bool update_files = true);

		// Changes the settings of an asset with the newly given.
		// It will adjust the buffers accordingly. It returns true when update files is false
		// and when the latter is set to true, it returns true when the files were updated successfully.
		// In case it fails, the in memory version will be the update one. If changing the name,
		// this does not verify that the new name doesn't exist previously
		bool UpdateShader(unsigned int handle, const ShaderMetadata* metadata, bool update_files = true);

		// Changes the settings of an asset with the newly given.
		// It will adjust the buffers accordingly. It returns true when update files is false
		// and when the latter is set to true, it returns true when the files were updated successfully.
		// In case it fails, the in memory version will be the update one. If changing the name,
		// this does not verify that the new name doesn't exist previously
		bool UpdateMaterial(unsigned int handle, const MaterialAsset* material, bool update_files = true);

		// Changes the settings of an asset with the newly given.
		// It will adjust the buffers accordingly. It returns true when update files is false
		// and when the latter is set to true, it returns true when the files were updated successfully.
		// In case it fails, the in memory version will be the update one
		bool UpdateMisc(unsigned int handle, const MiscAsset* misc, bool update_files = true);

		// Changes the settings of an asset with the newly given.
		// It will adjust the buffers accordingly. It returns true when update files is false
		// and when the latter is set to true, it returns true when the files were updated successfully.
		// In case it fails, the in memory version will be the update one. If changing the name,
		// this does not verify that the new name doesn't exist previously
		bool UpdateAsset(unsigned int handle, const void* asset, ECS_ASSET_TYPE type, bool update_files = true);

		// Writes the file for that asset. Returns true if it suceedded, else false
		// In case it fails, the previous version of that file (if it exists) will be restored
		bool WriteMeshFile(const MeshMetadata* metadata) const;

		// Writes the file for that asset. Returns true if it suceedded, else false
		// In case it fails, the previous version of that file (if it exists) will be restored
		bool WriteTextureFile(const TextureMetadata* metadata) const;

		// Writes the file for that asset. Returns true if it suceedded, else false
		// In case it fails, the previous version of that file (if it exists) will be restored
		bool WriteGPUSamplerFile(const GPUSamplerMetadata* metadata) const;

		// Writes the file for that asset. Returns true if it suceedded, else false
		// In case it fails, the previous version of that file (if it exists) will be restored
		bool WriteShaderFile(const ShaderMetadata* metadata) const;

		// Writes the file for that asset. Returns true if it suceedded, else false
		// In case it fails, the previous version of that file (if it exists) will be restored
		bool WriteMaterialFile(const MaterialAsset* metadata) const;

		// Writes the file for that asset. Returns true if it suceedded, else false
		// In case it fails, the previous version of that file (if it exists) will be restored
		bool WriteMiscFile(const MiscAsset* metadata) const;

		// Writes the file for that asset. Returns true if it suceedded, else false
		// In case it fails, the previous version of that file (if it exists) will be restored
		bool WriteAssetFile(const void* asset, ECS_ASSET_TYPE type) const;

		// Fills in the unique assets alongside the handles that target the same asset multiple times
		Stream<AssetDatabaseSameTargetAsset> SameTargetMeshes(AllocatorPolymorphic allocator) const;

		// Fills in the unique assets alongside the handles that target the same asset multiple times
		Stream<AssetDatabaseSameTargetAsset> SameTargetTextures(AllocatorPolymorphic allocator) const;

		// Fills in the unique assets alongside the handles that target the same asset multiple times
		Stream<AssetDatabaseSameTargetAsset> SameTargetShaders(AllocatorPolymorphic allocator) const;

		// Fills in the unique assets alongside the handles that target the same asset multiple times
		Stream<AssetDatabaseSameTargetAsset> SameTargetMiscs(AllocatorPolymorphic allocator) const;

		AssetDatabaseSameTargetAll SameTargetAll(AllocatorPolymorphic allocator) const;

		// Returns the name from the given metadata file
		static void ExtractNameFromFile(Stream<wchar_t> path, CapacityStream<char>& name);
		
		static void ExtractFileFromFile(Stream<wchar_t> path, CapacityStream<wchar_t>& file);

		ECS_FIELDS_START_REFLECT;

		ResizableSparseSet<ReferenceCounted<MeshMetadata>> mesh_metadata;
		ResizableSparseSet<ReferenceCounted<TextureMetadata>> texture_metadata;
		ResizableSparseSet<ReferenceCounted<GPUSamplerMetadata>> gpu_sampler_metadata;
		ResizableSparseSet<ReferenceCounted<ShaderMetadata>> shader_metadata;
		ResizableSparseSet<ReferenceCounted<MaterialAsset>> material_asset;
		ResizableSparseSet<ReferenceCounted<MiscAsset>> misc_asset;

		ECS_FIELDS_END_REFLECT;

		// In that directory the metadata folders will be created
		Stream<wchar_t> metadata_file_location;
		const Reflection::ReflectionManager* reflection_manager;
	};

	// ------------------------------------------------------------------------------------------------------------
	
	enum ECS_SERIALIZE_CODE : unsigned char;

	enum ECS_DESERIALIZE_CODE : unsigned char;

	// It writes only the names and the paths of the resources that need to be written
	ECSENGINE_API ECS_SERIALIZE_CODE SerializeAssetDatabase(const AssetDatabase* database, Stream<wchar_t> file);

	ECSENGINE_API ECS_SERIALIZE_CODE SerializeAssetDatabase(const AssetDatabase* database, uintptr_t& ptr);

	ECSENGINE_API size_t SerializeAssetDatabaseSize(const AssetDatabase* database);

	// The corresponding metadata files are not loaded
	ECSENGINE_API ECS_DESERIALIZE_CODE DeserializeAssetDatabase(AssetDatabase* database, Stream<wchar_t> file, bool reference_count_zero = false);

	ECSENGINE_API ECS_DESERIALIZE_CODE DeserializeAssetDatabase(AssetDatabase* database, uintptr_t& ptr, bool reference_count_zero = false);

	ECSENGINE_API size_t DeserializeAssetDatabaseSize(const Reflection::ReflectionManager* reflection_manager, uintptr_t ptr);

	// ------------------------------------------------------------------------------------------------------------

}