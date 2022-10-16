// ECS_REFLECT
#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "AssetMetadata.h"
#include "../../Utilities/Reflection/ReflectionMacros.h"

namespace ECSEngine {

	struct AssetDatabase;

	namespace Reflection {
		struct ReflectionManager;
	}

	struct ECSENGINE_API ECS_REFLECT AssetDatabaseReference {
		AssetDatabaseReference();
		AssetDatabaseReference(AssetDatabase* database, AllocatorPolymorphic allocator);

		void AddMesh(unsigned int handle);

		void AddTexture(unsigned int handle);

		void AddGPUSampler(unsigned int handle);

		void AddShader(unsigned int handle);

		void AddMaterial(unsigned int handle);

		void AddMisc(unsigned int handle);

		void AddAsset(unsigned int handle, ECS_ASSET_TYPE type);

		unsigned int AddAsset(Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type, bool* loaded_now = nullptr);

		// Copies the current contents into a new database using the allocator given
		AssetDatabaseReference Copy(AllocatorPolymorphic allocator) const;

		MeshMetadata* GetMesh(unsigned int index);

		TextureMetadata* GetTexture(unsigned int index);

		GPUSamplerMetadata* GetGPUSampler(unsigned int index);

		ShaderMetadata* GetShader(unsigned int index);

		MaterialAsset* GetMaterial(unsigned int index);

		MiscAsset* GetMisc(unsigned int index);

		void* GetAsset(unsigned int index, ECS_ASSET_TYPE type);

		unsigned int GetHandle(unsigned int index, ECS_ASSET_TYPE type) const;

		// It returns -1 if it doesn't find it
		unsigned int GetIndex(unsigned int handle, ECS_ASSET_TYPE type) const;

		AssetDatabase* GetDatabase() const;

		// Returns true if the asset was evicted e.g. its reference count reached 0
		bool RemoveMesh(unsigned int index);

		// Returns true if the asset was evicted e.g. its reference count reached 0
		bool RemoveTexture(unsigned int index);

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

		// It removes it only from this internal storage, not doing it for the main database
		void RemoveAssetThisOnly(unsigned int index, ECS_ASSET_TYPE type);

		// Clears all the assets that are inside. It doesn't decrement the reference count of the assets
		// that are alive inside.
		// All of them should be evicted from memory before calling this function
		void Reset();

		// Increases the reference count of all assets by one
		void IncrementReferenceCounts();

		// Converts a standalone database into a reference to the one being stored.
		void FromStandalone(const AssetDatabase* database);

		// Creates a standalone database from the referenced assets.
		void ToStandalone(AllocatorPolymorphic allocator, AssetDatabase* database) const;

		bool SerializeStandalone(const Reflection::ReflectionManager* reflection_manager, Stream<wchar_t> file) const;

		bool SerializeStandalone(const Reflection::ReflectionManager* reflection_manager, uintptr_t& ptr) const;

		// It will determine the serialization size and then allocate a buffer and write into it and returns it.
		// It returns { nullptr, 0 } if it fails
		Stream<void> SerializeStandalone(const Reflection::ReflectionManager* reflection_manager, AllocatorPolymorphic allocator) const;

		// Returns the amount of bytes needed to write the data. Returns -1 an error occurs
		size_t SerializeStandaloneSize(const Reflection::ReflectionManager* reflection_manager) const;

		bool DeserializeStandalone(const Reflection::ReflectionManager* reflection_manager, Stream<wchar_t> file);

		// Assumes a valid allocator was set before hand on this database
		bool DeserializeStandalone(const Reflection::ReflectionManager* reflection_manager, uintptr_t& ptr);

		// Returns the amount of bytes needed for the buffers. Returns -1 in case an error occurs
		static size_t DeserializeSize(const Reflection::ReflectionManager* reflection_manager, uintptr_t ptr);

		ECS_FIELDS_START_REFLECT;

		ResizableStream<unsigned int> mesh_metadata;
		ResizableStream<unsigned int> texture_metadata;
		ResizableStream<unsigned int> gpu_sampler_metadata;
		ResizableStream<unsigned int> shader_metadata;
		ResizableStream<unsigned int> material_asset;
		ResizableStream<unsigned int> misc_asset;

		AssetDatabase* database; ECS_SKIP_REFLECTION(static_assert(sizeof(AssetDatabase*) == 8))

		ECS_FIELDS_END_REFLECT;
	};

}