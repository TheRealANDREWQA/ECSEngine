// ECS_REFLECT
#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "AssetMetadata.h"
#include "../Utilities/Reflection/ReflectionMacros.h"

namespace ECSEngine {

	struct AssetDatabase;
	struct AssetDatabaseRemoveInfo;

	namespace Reflection {
		struct ReflectionManager;
	}
	
	struct AssetDatabaseReferencePointerRemap {
		unsigned int old_index;
		unsigned int new_index;
		unsigned int handle;
	};

	// A handle_remapping can be specified. When adding the assets from the given database
	// into the master database that this reference is referring to, the handle can change their values
	// The pairs are { original_handle, new_handle_value }.
	// Alternatively, if there are randomized pointers and you want to make sure that they are unique when inserting
	// them into the master database, this will report the handles that needed to be changed.
	// There needs to be specified ECS_ASSET_TYPE_COUNT for each remapping (each asset type has its own stream)
	struct AssetDatabaseReferenceFromStandaloneOptions {
		CapacityStream<uint2>* handle_remapping = nullptr;
		CapacityStream<AssetDatabaseReferencePointerRemap>* pointer_remapping = nullptr;
	};

	struct ECSENGINE_API ECS_REFLECT AssetDatabaseReference {
		AssetDatabaseReference();
		AssetDatabaseReference(AssetDatabase* database, AllocatorPolymorphic allocator);

		// By default it does not increment the count
		void AddMesh(unsigned int handle, bool increment_count = false);

		// By default it does not increment the count
		void AddTexture(unsigned int handle, bool increment_count = false);

		// By default it does not increment the count
		void AddGPUSampler(unsigned int handle, bool increment_count = false);

		// By default it does not increment the count
		void AddShader(unsigned int handle, bool increment_count = false);

		// By default it does not increment the count
		void AddMaterial(unsigned int handle, bool increment_count = false);

		// By default it does not increment the count
		void AddMisc(unsigned int handle, bool increment_count = false);

		// By default it does not increment the count
		void AddAsset(unsigned int handle, ECS_ASSET_TYPE type, bool increment_count = false);

		unsigned int AddAsset(Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type, bool* loaded_now = nullptr);

		// Copies the current contents into a new database using the allocator given
		AssetDatabaseReference Copy(AllocatorPolymorphic allocator) const;

		// Returns the first index that matches the given asset. Returns -1 if it doesn't exist
		unsigned int Find(Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type) const;

		// If it finds the asset in the original type, it returns its index inside the stream.
		// If it doesn't, it will search other types of assets that can reference it and return the handle
		// and the type of the asset that references it (at the moment only materials reference other assets)
		// Returns -1 if it doesn't appear at all in the database
		AssetTypedHandle FindDeep(Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type) const;

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

		unsigned int GetReferenceCountIndex(unsigned int index, ECS_ASSET_TYPE type) const;

		unsigned int GetReferenceCountHandle(unsigned int handle, ECS_ASSET_TYPE type) const;

		ECS_INLINE AssetDatabase* GetDatabase() const {
			return database;
		}

		unsigned int GetCount(ECS_ASSET_TYPE type) const;

		// Returns true if the asset was evicted e.g. its reference count reached 0
		// Can optionally fill in the fields of the evicted asset such that you can use it
		// for some other purpose. The values are valid only until the next remove or addition
		bool RemoveMesh(unsigned int index, MeshMetadata* storage = nullptr);

		// Returns true if the asset was evicted e.g. its reference count reached 0
		// Can optionally fill in the fields of the evicted asset such that you can use it
		// for some other purpose. The values are valid only until the next remove or addition
		bool RemoveTexture(unsigned int index, TextureMetadata* storage = nullptr);

		// Returns true if the asset was evicted e.g. its reference count reached 0
		// Can optionally fill in the fields of the evicted asset such that you can use it
		// for some other purpose. The values are valid only until the next remove or addition
		bool RemoveGPUSampler(unsigned int index, GPUSamplerMetadata* storage = nullptr);

		// Returns true if the asset was evicted e.g. its reference count reached 0
		// Can optionally fill in the fields of the evicted asset such that you can use it
		// for some other purpose. The values are valid only until the next remove or addition
		bool RemoveShader(unsigned int index, ShaderMetadata* storage = nullptr);

		// Returns true if the asset was evicted e.g. its reference count reached 0
		// Can optionally fill in the fields of the evicted asset such that you can use it
		// for some other purpose. The values are valid only until the next remove or addition
		bool RemoveMaterial(unsigned int index, AssetDatabaseRemoveInfo* remove_info = nullptr);

		// Returns true if the asset was evicted e.g. its reference count reached 0
		bool RemoveMisc(unsigned int index, MiscAsset* storage = nullptr);

		// Returns true if the asset was evicted e.g. its reference count reached 0
		// Can optionally fill in the fields of the evicted asset such that you can use it
		// for some other purpose. The values are valid only until the next remove or addition
		bool RemoveAsset(unsigned int index, ECS_ASSET_TYPE type, AssetDatabaseRemoveInfo* remove_info = nullptr);

		// It removes it only from this internal storage, not doing it for the main database
		void RemoveAssetThisOnly(unsigned int index, ECS_ASSET_TYPE type);

		// If the asset is to be evicted - e.g. it was the last reference, then it will call the functor
		// before/after the remove is actually done. Can control the before/after with the template boolean argument
		// The functor receives as arguments (unsigned int handle, ECS_ASSET_TYPE type, AssetType* asset)
		// Returns true if the asset was removed from the main database. If the only_main_asset is set to true
		// then the functor will not be called for its dependencies
		template<bool only_main_asset = false, bool before_removal = true, typename Functor>
		ECS_INLINE bool RemoveAssetWithAction(unsigned int index, ECS_ASSET_TYPE type, Functor&& functor) {
			unsigned int handle = GetHandle(index, type);
			RemoveAssetThisOnly(index, type);
			return database->RemoveAssetWithAction<only_main_asset, before_removal>(handle, type, functor);
		}

		// Clears all the assets that are inside. It doesn't decrement the reference count of the assets
		// that are alive inside.
		// All of them should be evicted from memory before calling this function
		void Reset();

		// Increases the reference count of all assets by one
		void IncrementReferenceCounts();

		// Converts a standalone database into a reference to the one being stored.
		void FromStandalone(const AssetDatabase* database, AssetDatabaseReferenceFromStandaloneOptions options = {});

		// Creates a standalone database from the referenced assets.
		void ToStandalone(AllocatorPolymorphic allocator, AssetDatabase* database) const;

		bool SerializeStandalone(const Reflection::ReflectionManager* reflection_manager, Stream<wchar_t> file) const;

		bool SerializeStandalone(const Reflection::ReflectionManager* reflection_manager, uintptr_t& ptr) const;

		// It will determine the serialization size and then allocate a buffer and write into it and returns it.
		// It returns { nullptr, 0 } if it fails
		Stream<void> SerializeStandalone(const Reflection::ReflectionManager* reflection_manager, AllocatorPolymorphic allocator) const;

		// Returns the amount of bytes needed to write the data. Returns -1 an error occurs
		size_t SerializeStandaloneSize(const Reflection::ReflectionManager* reflection_manager) const;

		// A handle_remapping can be specified.When adding the assets from the given database
		// into the master database that this reference is referring to, the handle can change their values
		// The pairs are { original_handle, new_handle_value }
		bool DeserializeStandalone(const Reflection::ReflectionManager* reflection_manager, Stream<wchar_t> file, AssetDatabaseReferenceFromStandaloneOptions options = {});

		// Assumes a valid allocator was set before hand on this database
		// A handle_remapping can be specified.When adding the assets from the given database
		// into the master database that this reference is referring to, the handle can change their values
		// The pairs are { original_handle, new_handle_value }
		bool DeserializeStandalone(const Reflection::ReflectionManager* reflection_manager, uintptr_t& ptr, AssetDatabaseReferenceFromStandaloneOptions options = {});

		// Returns the amount of bytes needed for the buffers. Returns -1 in case an error occurs
		static size_t DeserializeSize(const Reflection::ReflectionManager* reflection_manager, uintptr_t ptr);

		ECS_FIELDS_START_REFLECT;

		ResizableStream<unsigned int> mesh_metadata;
		ResizableStream<unsigned int> texture_metadata;
		ResizableStream<unsigned int> gpu_sampler_metadata;
		ResizableStream<unsigned int> shader_metadata;
		ResizableStream<unsigned int> material_asset;
		ResizableStream<unsigned int> misc_asset;

		AssetDatabase* database; ECS_SKIP_REFLECTION()

		ECS_FIELDS_END_REFLECT;
	};

}