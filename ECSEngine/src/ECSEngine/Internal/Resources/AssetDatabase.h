// ECS_REFLECT
#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "../../Containers/SparseSet.h"
#include "AssetMetadata.h"
#include "../../Utilities/Reflection/ReflectionMacros.h"
#include "../../Utilities/Reflection/Reflection.h"

namespace ECSEngine {

	template<typename Asset>
	struct ReferenceCountedAsset {
		Asset asset;
		unsigned int reference_count;
	};

	struct ECSENGINE_API ECS_REFLECT AssetDatabase {		
		AssetDatabase() = default;
		AssetDatabase(Stream<wchar_t> file_location, AllocatorPolymorphic allocator, Reflection::ReflectionManager* reflection_manager);

		ECS_CLASS_DEFAULT_CONSTRUCTOR_AND_ASSIGNMENT(AssetDatabase);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddMesh(Stream<char> name);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddTexture(Stream<char> name);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddGPUBuffer(Stream<char> name);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddGPUSampler(Stream<char> name);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddShader(Stream<char> name);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddMaterial(Stream<char> name);

		// It returns the handle to that asset. If it fails it returns -1
		unsigned int AddMisc(Stream<wchar_t> path);

		// It returns the handle to that asset. If it fails it returns -1
		// For a misc asset, the path needs to be casted to a char type
		unsigned int AddAsset(Stream<char> name, ECS_ASSET_TYPE type);

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

		TextureMetadata* GetTexture(unsigned int handle);

		GPUBufferMetadata* GetGPUBuffer(unsigned int handle);

		GPUSamplerMetadata* GetGPUSampler(unsigned int handle);

		// Can operate on it afterwards, like adding, removing or updating macros
		ShaderMetadata* GetShader(unsigned int handle);

		// Can operate on it afterwards, like adding or removing resources
		MaterialAsset* GetMaterial(unsigned int handle);

		MiscAsset* GetMisc(unsigned int handle);

		void* GetAsset(unsigned int handle, ECS_ASSET_TYPE type);

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

		void SetAllocator(AllocatorPolymorphic allocator);

		void SetFileLocation(Stream<wchar_t> file_location);

		ECS_FIELDS_START_REFLECT;

		ResizableSparseSet<ReferenceCountedAsset<MeshMetadata>> mesh_metadata;
		ResizableSparseSet<ReferenceCountedAsset<TextureMetadata>> texture_metadata;
		ResizableSparseSet<ReferenceCountedAsset<GPUBufferMetadata>> gpu_buffer_metadata;
		ResizableSparseSet<ReferenceCountedAsset<GPUSamplerMetadata>> gpu_sampler_metadata;
		ResizableSparseSet<ReferenceCountedAsset<ShaderMetadata>> shader_metadata;
		ResizableSparseSet<ReferenceCountedAsset<MaterialAsset>> material_asset;
		ResizableSparseSet<ReferenceCountedAsset<MiscAsset>> misc_asset;
		Stream<wchar_t> file_location;

		ECS_FIELDS_END_REFLECT;
		Reflection::ReflectionManager* reflection_manager;
	};

	// ------------------------------------------------------------------------------------------------------------

	// The serialize functions are not needed since they can be called directly upon the database
	// For the deserialization, some buffer management needs to be made
	
	enum ECS_DESERIALIZE_CODE : unsigned char;

	ECSENGINE_API ECS_DESERIALIZE_CODE DeserializeAssetDatabase(AssetDatabase* database, Stream<wchar_t> file);

	// ------------------------------------------------------------------------------------------------------------

}