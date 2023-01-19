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

	struct AssetDatabaseSnapshot {
		unsigned int stream_sizes[ECS_ASSET_TYPE_COUNT];
	};

	struct AssetDatabaseRemoveInfo {
		// If the asset is removed it will copy the metadata into this pointer
		void* storage = nullptr;
		// When set to true it will remove the dependencies as well
		bool remove_dependencies = true;
		// When set it will fill in the dependencies for this asset
		CapacityStream<AssetTypedHandle>* dependencies = nullptr;

		// The remove dependencies flag must be set for this to be considered
		// When set it will copy the dependency data into the given buffer from storage_dependencies_allocation
		// and write back the pointer here. The storage_dependencies_allocation must be specified as well.
		// It will write the metadata only for those assets that have been evicted.
		CapacityStream<AssetTypedMetadata>* storage_dependencies = nullptr;
		CapacityStream<void> storage_dependencies_allocation = { nullptr, 0, 0 };
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
		unsigned int AddMeshInternal(const MeshMetadata* metadata, unsigned int reference_count = 1);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddTexture(Stream<char> name, Stream<wchar_t> file, bool* loaded_now = nullptr);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddTextureInternal(const TextureMetadata* metadata, unsigned int reference_count = 1);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddGPUSampler(Stream<char> name, bool* loaded_now = nullptr);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddGPUSamplerInternal(const GPUSamplerMetadata* metadata, unsigned int reference_count = 1);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddShader(Stream<char> name, Stream<wchar_t> file, bool* loaded_now = nullptr);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddShaderInternal(const ShaderMetadata* metadata, unsigned int reference_count = 1);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddMaterial(Stream<char> name, bool* loaded_now = nullptr);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddMaterialInternal(const MaterialAsset* metadata, unsigned int reference_count = 1);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddMisc(Stream<char> name, Stream<wchar_t> file, bool* loaded_now = nullptr);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddMiscInternal(const MiscAsset* metadata, unsigned int reference_count = 1);

		// It returns the handle to that asset. If it fails it returns -1
		// The file is ignored for materials and samplers
		unsigned int AddAsset(Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type, bool* loaded_now = nullptr);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddAssetInternal(const void* asset, ECS_ASSET_TYPE type, unsigned int reference_count = 1);

		// It will search for the asset using the name and file. If it finds it, it will return a handle to it
		// and increment the reference count. Else it will add it and return the handle
		unsigned int AddFindAssetInternal(const void* asset, ECS_ASSET_TYPE type, unsigned int reference_count = 1);

		// It increments by one the reference count for that asset.
		void AddAsset(unsigned int handle, ECS_ASSET_TYPE type, unsigned int reference_count = 1);

		// Retrive the allocator for this database
		ECS_INLINE AllocatorPolymorphic Allocator() const {
			return mesh_metadata.allocator;
		}

		// Creates a new standalone database with the given allocator and the given handles
		AssetDatabase Copy(Stream<unsigned int>* handle_mask, AllocatorPolymorphic allocator) const;

		// Creates a new standalone database with the given allocator and the given handles
		AssetDatabase Copy(CapacityStream<unsigned int>* handle_mask, AllocatorPolymorphic allocator) const;

		// Copies the pointers for the assets from another database. If an asset mask is given, it must have ECS_ASSET_TYPE_COUNT entries
		// And the ints to be handles, not indices
		// If an asset doesn't exist, then it will not add it (since it can mess up the reference count).
		void CopyAssetPointers(const AssetDatabase* other, Stream<unsigned int>* asset_mask = nullptr);

		// Creates all the necessary folders for the metadata
		// Returns true if all the folders exist, else false (a failure happened during the creation of the folders)
		bool CreateMetadataFolders() const;

		// In case the database is not yet initialized
		static bool CreateMetadataFolders(Stream<wchar_t> file_location);

		// Deallocates all the streams (but it does not deallocate the individual assets)
		void DeallocateStreams();

		// The main asset should be an asset that can have dependencies (like materials) and referenced_asset
		// a referenceable asset (like textures, samplers, shaders). It uses the handle values in order to determine
		// if the asset is referenced or not
		bool DependsUpon(const void* main_asset, ECS_ASSET_TYPE type, const void* referenced_asset, ECS_ASSET_TYPE referenced_type) const;

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

		// The functor will be called with the handle as a parameter
		template<typename Functor>
		void ForEachAsset(ECS_ASSET_TYPE type, Functor&& functor) {
			unsigned int count = GetAssetCount(type);
			for (unsigned int index = 0; index < count; index++) {
				unsigned int handle = GetAssetHandleFromIndex(index, type);
				functor(handle);
			}
		}

		// The functor will be called with the handle as a parameter and the asset type
		template<typename Functor>
		void ForEachAsset(Functor&& functor) {
			for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
				ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
				unsigned int count = GetAssetCount(current_type);
				for (unsigned int subindex = 0; subindex < count; subindex++) {
					unsigned int handle = GetAssetHandleFromIndex(subindex, current_type);
					functor(handle, current_type);
				}
			}
		}

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

		unsigned int GetAssetCount(ECS_ASSET_TYPE type) const;

		unsigned int GetAssetHandleFromIndex(unsigned int index, ECS_ASSET_TYPE type) const;

		unsigned int GetIndexFromAssetHandle(unsigned int handle, ECS_ASSET_TYPE type) const;

		// Returns the reference count stored in the actual stream. This is the total reference count
		// of direct (or standalone) references + references from assets that depend on it
		unsigned int GetReferenceCount(unsigned int handle, ECS_ASSET_TYPE type) const;

		// Returns the count of direct references to that asset
		unsigned int GetReferenceCountStandalone(unsigned int handle, ECS_ASSET_TYPE type) const;

		// The counts stream is a pair with the x component being the handle and the y being the reference count
		// It will find the standalone count for each handle in the asset type
		void GetReferenceCountsStandalone(ECS_ASSET_TYPE type, CapacityStream<uint2>* counts) const;

		// Determines assets that use the given asset (at the moment only the material references other assets)
		// For example if a texture is being used and a material references it, then it will fill in the material that references it
		Stream<Stream<unsigned int>> GetDependentAssetsFor(const void* metadata, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator) const;

		// Fills in the dependencies that the given asset has
		void GetDependencies(unsigned int handle, ECS_ASSET_TYPE type, CapacityStream<AssetTypedHandle>* dependencies) const;

		AssetDatabaseSnapshot GetSnapshot() const;

		unsigned int GetRandomizedPointer(ECS_ASSET_TYPE type) const;

		// Returns true if the asset is being referenced by another asset
		bool IsAssetReferencedByOtherAsset(unsigned int handle, ECS_ASSET_TYPE type) const;

		// Generates the list of randomized values that are still valid
		void RandomizedPointerList(unsigned int maximum_count, ECS_ASSET_TYPE type, CapacityStream<unsigned int>& values) const;

		// Makes the pointer unique for each asset such that it can be uniquely identified by its pointer
		// An allocator can be given to allocate the assets from or, if left as default, it will allocate
		// from the database allocator.
		void RandomizePointer(unsigned int handle, ECS_ASSET_TYPE type);

		// Makes the pointer unique for each asset such that it can be uniquely identified by its pointer
		// An allocator can be given to allocate the assets from or, if left as default, it will allocate
		// from the database allocator.
		void RandomizePointers(AssetDatabaseSnapshot snapshot);

		// It does not set the name or the file
		bool ReadMeshFile(Stream<char> name, Stream<wchar_t> file, MeshMetadata* metadata) const;

		// It does not set the name or the file
		bool ReadTextureFile(Stream<char> name, Stream<wchar_t> file, TextureMetadata* metadata) const;

		// It does not set the name or the file
		bool ReadGPUSamplerFile(Stream<char> name, GPUSamplerMetadata* metadata) const;

		// It does not set the name or the file. Can optionally provide an allocator for the buffers that are used
		// otherwise the allocator from this database will be used.
		bool ReadShaderFile(Stream<char> name, Stream<wchar_t> file, ShaderMetadata* metadata, AllocatorPolymorphic allocator = { nullptr }) const;

		// It does not set the name or the file. Can optionally provide an allocator for the buffers that are used
		// otherwise the allocator from this database will be used.
		bool ReadMaterialFile(Stream<char> name, MaterialAsset* asset, AllocatorPolymorphic allocator = { nullptr }) const;

		// It does not set the name or the file
		bool ReadMiscFile(Stream<char> name, Stream<wchar_t> file, MiscAsset* asset) const;

		// It does not set the name or the file.  Can optionally provide an allocator for the buffers that are used
		// otherwise the allocator from this database will be used. (this is used only for shaders and materials)
		bool ReadAssetFile(Stream<char> name, Stream<wchar_t> file, void* metadata, ECS_ASSET_TYPE asset_type, AllocatorPolymorphic allocator = { nullptr }) const;

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveMesh(unsigned int handle, MeshMetadata* storage = nullptr);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveTexture(unsigned int handle, TextureMetadata* storage = nullptr);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveGPUSampler(unsigned int handle, GPUSamplerMetadata* storage = nullptr);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveShader(unsigned int handle, ShaderMetadata* storage = nullptr);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveMaterial(unsigned int handle, AssetDatabaseRemoveInfo* remove_info = nullptr);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveMisc(unsigned int handle, MiscAsset* storage = nullptr);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveAsset(unsigned int handle, ECS_ASSET_TYPE type, AssetDatabaseRemoveInfo* remove_info = nullptr);

		// Does not destroy the file. It will remove the asset no matter what the reference count is
		// If the decrement_dependencies is set to true, then it will decrement the reference count of its dependencies
		// (it will not forcefully remove them)
		void RemoveAssetForced(unsigned int handle, ECS_ASSET_TYPE type, AssetDatabaseRemoveInfo* remove_info = nullptr);

		// If the asset is to be evicted - e.g. it was the last reference, then it will call the functor
		// before/after the remove is actually done. Can control the before/after with the template boolean argument
		// The functor receives as arguments (unsigned int handle, ECS_ASSET_TYPE type, AssetType* asset)
		// Returns true if the asset was removed. If the only_main_asset is set to true, the functor will not be called
		// for dependencies
		template<bool only_main_asset = false, bool before_removal = true, typename Functor>
		bool RemoveAssetWithAction(unsigned int handle, ECS_ASSET_TYPE type, Functor&& functor) {
			// Returns true if the asset was removed
			auto remove_lambda = [&](auto& sparse_set) {
				auto& metadata = sparse_set[handle];
				metadata.reference_count--;

				if (metadata.reference_count == 0) {
					if constexpr (before_removal) {
						functor(handle, type, &metadata.value);
					}

					// Deallocate the memory
					metadata.value.DeallocateMemory(sparse_set.allocator);
					sparse_set.RemoveSwapBack(handle);

					if constexpr (!before_removal) {
						functor(handle, type, &metadata.value);
					}

					return true;
				}
				return false;
			};

			switch (type) {
			case ECS_ASSET_MESH:
			{
				return remove_lambda(mesh_metadata);
			}
			break;
			case ECS_ASSET_TEXTURE:
			{
				return remove_lambda(texture_metadata);
			}
			break;
			case ECS_ASSET_GPU_SAMPLER:
			{
				return remove_lambda(gpu_sampler_metadata);
			}
			break;
			case ECS_ASSET_SHADER:
			{
				return remove_lambda(shader_metadata);
			}
			break;
			case ECS_ASSET_MATERIAL:
			{
				// Need to take the metadata now, and then deallocate the dependencies since the functor
				// might use the handles that are dependencies.
				MaterialAsset old_asset;
				old_asset = *GetMaterialConst(handle);
				bool removed = remove_lambda(material_asset);
				if constexpr (!only_main_asset) {
					if (removed) {
						RemoveAssetDependencies(&old_asset, ECS_ASSET_MATERIAL);
					}
				}
				return removed;
			}
			break;
			case ECS_ASSET_MISC:
			{
				return remove_lambda(misc_asset);
			}
			break;
			default:
				ECS_ASSERT(false);
			}
			return false;
		}

		void RemoveAssetDependencies(const void* asset, ECS_ASSET_TYPE type);

		// If the asset has dependencies, it will remove them
		void RemoveAssetDependencies(unsigned int handle, ECS_ASSET_TYPE type);

		// The restore works only when elements were added, if removals have happened
		// these will produce undefined behaviour
		void RestoreSnapshot(AssetDatabaseSnapshot snapshot);

		// Only sets the allocator for the streams, it does not copy the already existing data
		// (it should not be used for that purpose)
		void SetAllocator(AllocatorPolymorphic allocator);

		void SetFileLocation(Stream<wchar_t> file_location);

		// The reference count needs to be larger than 1
		void SetAssetReferenceCount(unsigned int handle, ECS_ASSET_TYPE asset_type, unsigned int new_reference_count);

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