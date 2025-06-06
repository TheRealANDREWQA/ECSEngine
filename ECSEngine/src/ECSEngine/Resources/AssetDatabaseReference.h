// ECS_REFLECT
#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "AssetMetadata.h"
#include "../Utilities/Reflection/ReflectionMacros.h"
#include "AssetDatabase.h"

namespace ECSEngine {

	struct AssetDatabaseRemoveInfo;

	struct WriteInstrument;
	struct ReadInstrument;

	namespace Reflection {
		struct ReflectionManager;
	}
	
	struct AssetDatabaseReferencePointerRemap {
		void* old_pointer;
		void* new_pointer;
		unsigned int handle;
	};

	// A handle_remapping can be specified. When adding the assets from the given database
	// into the master database that this reference is referring to, the handle can change its value
	// The pairs are { original_handle, new_handle_value }.
	// The pointer remap gives the values for the assets whose pointer values are different between the 2 asset databases
	// There needs to be specified ECS_ASSET_TYPE_COUNT for each remapping (each asset type has its own stream)
	struct AssetDatabaseReferenceFromStandaloneOptions {
		CapacityStream<uint2>* handle_remapping = nullptr;
		Stream<CapacityStream<AssetDatabaseReferencePointerRemap>> pointer_remapping = { nullptr, 0 };
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

		// Adds all the assets from the other database reference. If you set the increment to true,
		// The the counts inside the main database are incremented as well
		void AddOther(const AssetDatabaseReference* other, bool increment_main_database_counts = false);

		// Copies the current contents into a new database using the allocator given
		AssetDatabaseReference Copy(AllocatorPolymorphic allocator) const;

		// Returns the first index that matches the given asset. Returns -1 if it doesn't exist
		unsigned int Find(Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type) const;

		// If it finds the asset in the original type, it returns its index inside the stream.
		// If it doesn't, it will search other types of assets that can reference it and return the handle
		// and the type of the asset that references it (at the moment only materials reference other assets)
		// Returns -1 if it doesn't appear at all in the database
		AssetTypedHandle FindDeep(Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type) const;

		// If it finds the asset in the original type, it returns its index inside the stream.
		// If it doesn't, it will search other types of assets that can reference it and return the handle
		// and the type of the asset that references it (at the moment only materials reference other assets)
		// Returns -1 if it doesn't appear at all in the database
		AssetTypedHandle FindDeep(const void* metadata, ECS_ASSET_TYPE type) const;

		// The functor is called as (unsigned int handle)
		// Return true in the functor to early exit
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEachAsset(ECS_ASSET_TYPE type, Functor&& functor) const {
			unsigned int count = GetCount(type);
			// We need to prune the entries - if there are multiple reference counts they will appear
			// multiple times and we need to call the functor only once for each handle
			
			ECS_ASSERT(count <= ECS_KB * 16, "Too many asset database reference entries for a particular type");

			ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, temporary_values, count);
			ResizableStream<unsigned int>* streams = (ResizableStream<unsigned int>*)this;
			temporary_values.size = count;
			temporary_values.CopyOther(streams[type].ToStream());

			for (unsigned int index = 0; index < temporary_values.size; index++) {
				unsigned int handle = GetHandle(index, type);
				if constexpr (early_exit) {
					if (functor(handle)) {
						return true;
					}
				}
				else {
					functor(handle);
				}

				// Now we need to eliminate all identical handles as this one
				size_t next_value_index = SearchBytes(temporary_values.buffer + index + 1, temporary_values.size - index - 1, handle);
				while (next_value_index != -1) {
					temporary_values.RemoveSwapBack(index + 1);
					next_value_index = SearchBytes(temporary_values.buffer + index + 1, temporary_values.size - index - 1, handle);
				}
			}
			return false;
		}

		// The functor will be called for each appearance of a handle - so if a handle
		// has a reference count of 2 here, it means it will be called 2 times with that handle value
		// The functor is called as (unsigned int handle)
		// Return true in the functor to early exit
		// Returns true if it early exited, else false
		template<bool early_exit = false, typename Functor>
		bool ForEachAssetDuplicates(ECS_ASSET_TYPE type, Functor&& functor) const {
			unsigned int count = GetCount(type);
			for (unsigned int index = 0; index < count; index++) {
				unsigned int handle = GetHandle(index, type);
				if constexpr (early_exit) {
					if (functor(handle)) {
						return true;
					}
				}
				else {
					functor(handle);
				}
			}
			return false;
		}

		ECS_INLINE MeshMetadata* GetMesh(unsigned int index) {
			return (MeshMetadata*)GetAssetByIndex(index, ECS_ASSET_MESH);
		}

		ECS_INLINE TextureMetadata* GetTexture(unsigned int index) {
			return (TextureMetadata*)GetAssetByIndex(index, ECS_ASSET_TEXTURE);
		}

		ECS_INLINE GPUSamplerMetadata* GetGPUSampler(unsigned int index) {
			return (GPUSamplerMetadata*)GetAssetByIndex(index, ECS_ASSET_GPU_SAMPLER);
		}

		ECS_INLINE ShaderMetadata* GetShader(unsigned int index) {
			return (ShaderMetadata*)GetAssetByIndex(index, ECS_ASSET_SHADER);
		}

		ECS_INLINE MaterialAsset* GetMaterial(unsigned int index) {
			return (MaterialAsset*)GetAssetByIndex(index, ECS_ASSET_MATERIAL);
		}

		ECS_INLINE MiscAsset* GetMisc(unsigned int index) {
			return (MiscAsset*)GetAssetByIndex(index, ECS_ASSET_MISC);
		}

		// Returns a database snapshot for this reference instance, not for the overall database
		AssetDatabaseFullSnapshot GetFullSnapshot(AllocatorPolymorphic allocator) const;

		void* GetAssetByIndex(unsigned int index, ECS_ASSET_TYPE type);

		void* GetAsset(unsigned int handle, ECS_ASSET_TYPE type);

		ECS_INLINE unsigned int GetHandle(unsigned int index, ECS_ASSET_TYPE type) const {
			ResizableStream<unsigned int>* streams = (ResizableStream<unsigned int>*)this;
			return streams[type][index];
		}

		// It returns -1 if it doesn't find it
		ECS_INLINE unsigned int GetIndex(unsigned int handle, ECS_ASSET_TYPE type) const {
			ResizableStream<unsigned int>* streams = (ResizableStream<unsigned int>*)this;
			return SearchBytes(streams[type].buffer, streams[type].size, handle);
		}

		ECS_INLINE unsigned int GetReferenceCountIndex(unsigned int index, ECS_ASSET_TYPE type) const {
			return GetReferenceCountHandle(GetHandle(index, type), type);
		}

		// Returns the reference count in the overall database - not how many encounters there are in this reference
		unsigned int GetReferenceCountHandle(unsigned int handle, ECS_ASSET_TYPE type) const;

		// Returns the reference count of the handle inside this database reference - not in the overall database
		unsigned int GetReferenceCountInInstance(unsigned int handle, ECS_ASSET_TYPE type) const;

		ECS_INLINE AssetDatabase* GetDatabase() const {
			return database;
		}

		// Returns the count of all assets (includes duplicates)
		unsigned int GetCount() const;

		ECS_INLINE unsigned int GetCount(ECS_ASSET_TYPE type) const {
			ResizableStream<unsigned int>* streams = (ResizableStream<unsigned int>*)this;
			return streams[type].size;
		}

		ECS_INLINE Stream<unsigned int> GetHandlesForType(ECS_ASSET_TYPE type) const {
			ResizableStream<unsigned int>* streams = (ResizableStream<unsigned int>*)this;
			return streams[type].ToStream();
		}

		Stream<Stream<unsigned int>> GetUniqueHandles(AllocatorPolymorphic allocator) const;

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

		// Clears all the assets that are inside
		// If the remove dependencies is set to true, if an asset is removed the main
		// database, its dependencies will get decremented
		void Reset(bool decrement_reference_counts = false, bool remove_dependencies = true);

		// Increases the reference count of all assets by one
		// Can choose whether or not the reflected reference count increase to be reflected in this reference or not
		void IncrementReferenceCounts(bool add_here = false);

		// Converts a standalone database into a reference to the one being stored.
		void FromStandalone(const AssetDatabase* database, AssetDatabaseReferenceFromStandaloneOptions options = {});

		// Creates a standalone database from the referenced assets.
		void ToStandalone(AllocatorPolymorphic allocator, AssetDatabase* database) const;

		// Returns true if it succeeded, else false
		bool SerializeStandalone(const Reflection::ReflectionManager* reflection_manager, WriteInstrument* write_instrument) const;

		// A handle_remapping can be specified. When adding the assets from the given database
		// into the master database that this reference is referring to, the handles can change their values
		// The pairs are { original_handle, new_handle_value }
		bool DeserializeStandalone(const Reflection::ReflectionManager* reflection_manager, ReadInstrument* read_instrument, AssetDatabaseReferenceFromStandaloneOptions options = {});

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