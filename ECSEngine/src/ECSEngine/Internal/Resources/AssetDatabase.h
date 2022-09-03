// ECS_REFLECT
#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../../Containers/SparseSet.h"
#include "AssetMetadata.h"
#include "../../Utilities/Reflection/ReflectionMacros.h"
#include "../../Utilities/Reflection/Reflection.h"

namespace ECSEngine {

	// If the file location is not set, when adding the resources, no file is generated
	struct ECSENGINE_API ECS_REFLECT AssetDatabase {		
		AssetDatabase() = default;
		AssetDatabase(AllocatorPolymorphic allocator, const Reflection::ReflectionManager* reflection_manager);

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(AssetDatabase);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddMesh(Stream<char> name);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddMeshInternal(const MeshMetadata* metadata);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddTexture(Stream<char> name);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddTextureInternal(const TextureMetadata* metadata);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddGPUBuffer(Stream<char> name);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddGPUBufferInternal(const GPUBufferMetadata* metadata);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddGPUSampler(Stream<char> name);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddGPUSamplerInternal(const GPUSamplerMetadata* metadata);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddShader(Stream<char> name);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddShaderInternal(const ShaderMetadata* metadata);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddMaterial(Stream<char> name);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddMaterialInternal(const MaterialAsset* metadata);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddMisc(Stream<wchar_t> path);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddMiscInternal(const MiscAsset* metadata);

		// It returns the handle to that asset. If it fails it returns -1
		// For a misc asset, the path needs to be casted to a char type
		unsigned int AddAsset(Stream<char> name, ECS_ASSET_TYPE type);

		// Used in specific scenarios where a temporary database is needed. It will copy the metadata as is
		// Returns a handle to that asset. Does not verify it it already exists
		unsigned int AddAssetInternal(const void* asset, ECS_ASSET_TYPE type);

		// It increments by one the reference count for that asset.
		void AddAsset(unsigned int handle, ECS_ASSET_TYPE type);

		// Retrive the allocator for this database
		AllocatorPolymorphic Allocator() const;

		bool CreateMeshFile(const MeshMetadata* metadata) const;

		bool CreateTextureFile(const TextureMetadata* metadata) const;

		bool CreateGPUBufferFile(const GPUBufferMetadata* metadata) const;

		bool CreateGPUSamplerFile(const GPUSamplerMetadata* metadata) const;

		bool CreateShaderFile(const ShaderMetadata* metadata) const;

		bool CreateMaterialFile(const MaterialAsset* metadata) const;

		bool CreateMiscFile(const MiscAsset* metadata) const;

		bool CreateAssetFile(const void* asset, ECS_ASSET_TYPE type) const;

		// Returns the handle to that asset. Returns -1 if it doesn't exist
		unsigned int FindMesh(Stream<char> name) const;

		// Returns the handle to that asset. Returns -1 if it doesn't exist
		unsigned int FindTexture(Stream<char> name) const;

		// Returns the handle to that asset. Returns -1 if it doesn't exist
		unsigned int FindGPUBuffer(Stream<char> name) const;

		// Returns the handle to that asset. Returns -1 if it doesn't exist
		unsigned int FindGPUSampler(Stream<char> name) const;

		// Returns the handle to that asset. Returns -1 if it doesn't exist
		unsigned int FindShader(Stream<char> name) const;

		// Returns the handle to that asset. Returns -1 if it doesn't exist
		unsigned int FindMaterial(Stream<char> name) const;

		// Returns the handle to that asset. Returns -1 if it doesn't exist
		unsigned int FindMisc(Stream<wchar_t> path) const;

		// Returns the handle to that asset. Returns -1 if it doesn't exist
		// For misc asset, the path needs to be casted into a char type
		unsigned int FindAsset(Stream<char> name, ECS_ASSET_TYPE type) const;

		void FileLocationMesh(CapacityStream<wchar_t>& path) const;

		void FileLocationTexture(CapacityStream<wchar_t>& path) const;

		void FileLocationGPUBuffer(CapacityStream<wchar_t>& path) const;

		void FileLocationGPUSampler(CapacityStream<wchar_t>& path) const;

		void FileLocationShader(CapacityStream<wchar_t>& path) const;

		void FileLocationMaterial(CapacityStream<wchar_t>& path) const;

		void FileLocationMisc(CapacityStream<wchar_t>& path) const;

		void FileLocationAsset(CapacityStream<wchar_t>& path, ECS_ASSET_TYPE type) const;

		MeshMetadata* GetMesh(unsigned int handle);

		const MeshMetadata* GetMeshConst(unsigned int handle) const;

		TextureMetadata* GetTexture(unsigned int handle);

		const TextureMetadata* GetTextureConst(unsigned int handle) const;

		GPUBufferMetadata* GetGPUBuffer(unsigned int handle);

		const GPUBufferMetadata* GetGPUBufferConst(unsigned int handle) const;
 
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

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveMesh(Stream<char> name);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveMesh(unsigned int handle);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveTexture(Stream<char> name);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveTexture(unsigned int handle);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveGPUBuffer(Stream<char> name);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveGPUBuffer(unsigned int handle);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveGPUSampler(Stream<char> name);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveGPUSampler(unsigned int handle);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveShader(Stream<char> name);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveShader(unsigned int handle);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveMaterial(Stream<char> name);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveMaterial(unsigned int handle);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveMisc(Stream<wchar_t> path);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveMisc(unsigned int handle);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveAsset(Stream<char> name, ECS_ASSET_TYPE type);

		// Returns true if the asset was evicted - e.g. it was the last reference
		// Does not destroy the file
		bool RemoveAsset(unsigned int handle, ECS_ASSET_TYPE type);

		// Only sets the allocator for the streams, it does not copy the already existing data
		// (it should not be used for that purpose)
		void SetAllocator(AllocatorPolymorphic allocator);

		void SetFileLocation(Stream<wchar_t> file_location);

		ECS_FIELDS_START_REFLECT;

		ResizableSparseSet<ReferenceCounted<MeshMetadata>> mesh_metadata;
		ResizableSparseSet<ReferenceCounted<TextureMetadata>> texture_metadata;
		ResizableSparseSet<ReferenceCounted<GPUBufferMetadata>> gpu_buffer_metadata;
		ResizableSparseSet<ReferenceCounted<GPUSamplerMetadata>> gpu_sampler_metadata;
		ResizableSparseSet<ReferenceCounted<ShaderMetadata>> shader_metadata;
		ResizableSparseSet<ReferenceCounted<MaterialAsset>> material_asset;
		ResizableSparseSet<ReferenceCounted<MiscAsset>> misc_asset;
		Stream<wchar_t> file_location;

		ECS_FIELDS_END_REFLECT;
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