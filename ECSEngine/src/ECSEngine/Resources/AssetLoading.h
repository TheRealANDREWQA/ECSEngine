#pragma once
#include "../Core.h"
#include "../Containers/Stream.h"
#include "AssetMetadata.h"
#include "../Utilities/FilePackaging.h"
#include "../Containers/AtomicStream.h"
#include "../Multithreading/ThreadTask.h"

namespace ECSEngine {

	/*
		In order to use these calls for the AssetDatabaseReference you must convert it into
		a normal AssetDatabase first using the ToStandalone() call and then forward to the
		appropriate calls. The asset database needs to be persistent so the allocator with
		which is allocated needs to be stored by the caller and when the Semaphore is signaled
		then the caller can proceed to deallocate the allocator/database
	*/

	struct ResourceManager;
	struct AssetDatabase;
	struct AssetDatabaseReference;
	struct TaskManager;

	// This is used to report the exact failure for the load
	// If processing_failure is set to true then the data from disk was read but the processing failed
	// If set to false then reading from disk is the culprit. If the dependency failure is set to true, then it failed
	// because a dependent resource failed to load (this can only be true for materials, in case a texture/shader fails loading
	// then this material cannot continue and fails as well). Use the function ToString() for an easier
	// conversion to characters
	struct ECSENGINE_API LoadAssetFailure {
		void ToString(const AssetDatabase* database, CapacityStream<char>& characters) const;

		unsigned int handle;
		ECS_ASSET_TYPE asset_type;
		bool processing_failure;
		bool dependency_failure;
	};

	struct LoadAssetOnSuccessData {
		SpinLock* spin_lock;
		unsigned int handle;
		ECS_ASSET_TYPE type;
		void* metadata;
		AssetDatabase* database;
		ResourceManager* resource_manager;
		void* data;
	};

	// The mount path will be copied internally
	struct LoadAssetInfo {
		Semaphore* finish_semaphore;
		bool* success = nullptr;
		AtomicStream<LoadAssetFailure>* load_failures = nullptr;
		Stream<wchar_t> mount_point = { nullptr, 0 };

		// With this mask can specify which actual handles should be loaded
		// There must be ECS_ASSET_TYPE_COUNT streams
		//Stream<Stream<unsigned int>> include_assets = { nullptr, 0 };
		// With this mask can specify which handles should be omitted when
		// generating tasks. There must be ECS_ASSET_TYPE_COUNT streams
		//Stream<Stream<unsigned int>> exclude_assets = { nullptr, 0 };

		// For assets which can have dependencies, if the dependency has a pointer that is valid,
		// it will assume that it is already loaded
		//bool assets_with_dependencies_assume_valid_as_loaded = true;

		// The function must cast the data into a LoadAssetOnSuccessData* and then cast to its own data
		// The spin lock can be used to get exclusive access to the resource manager and reference the
		// asset that was successfully preloaded/processed. Also the thread task data will also be copied

		// These are callbacks that will be called when an asset was successfully preloaded.
		ThreadTask preload_on_success[ECS_ASSET_TYPE_COUNT] = { {} };

		// These are callbacks that will be called when an asset was successfully processed.
		ThreadTask process_on_success[ECS_ASSET_TYPE_COUNT] = { {} };
	};

	// Loads all the corresponding assets from disk with the appropriate settings applied.
	// The database must not have the assets already loaded. When the count of the semaphore
	// Reaches 0 then all tasks have finished (if one task fails the other will continue their processing,
	// unless a dependent resource failed to load as well). It uses the database allocator internally
	ECSENGINE_API void LoadAssets(
		AssetDatabase* database,
		ResourceManager* resource_manager,
		TaskManager* task_manager,
		const LoadAssetInfo* load_info
	);
	
	// Loads all the corresponding assets from disk with the appropriate settings applied.
	// The database must not have the assets already loaded. When the count of the semaphore
	// Reaches 0 then all tasks have finished (if one task fails the other will continue their processing,
	// unless a dependent resource failed to load as well). The packed file is copied internally
	// so it can be a temporary packed file. It will close the packed file at the end.
	// It uses the database allocator internally
	ECSENGINE_API void LoadAssetsPacked(
		AssetDatabase* database,
		ResourceManager* resource_manager,
		TaskManager* task_manager,
		const PackedFile* packed_file,
		const LoadAssetInfo* load_info
	);
	
	// Loads all the corresponding assets from disk with the appropriate settings applied.
	// The database must not have the assets already loaded. When the count of the semaphore
	// Reaches 0 then all tasks have finished (if one task fails the other will continue their processing,
	// unless a dependent resource failed to load as well). The multi packed file and its inputs are
	// copied internally so these can be temporary. It will close the multi packed files at the end.
	// It uses the database allocator internally
	ECSENGINE_API void LoadAssetsPacked(
		AssetDatabase* database,
		ResourceManager* resource_manager, 
		TaskManager* task_manager,
		Stream<PackedFile> packed_files,
		const MultiPackedFile* multi_packed_file,
		const LoadAssetInfo* load_info
	);

	struct MissingAsset {
		ECS_ASSET_TYPE asset_type;
		unsigned int handle;
	};

	struct DeallocateAssetsWithRemappingOptions {
		Stream<wchar_t> mount_point = {};
		// There must be ECS_ASSET_TYPE_COUNT entries in this
		Stream<unsigned int>* asset_mask = nullptr;
		// If this is set to true, then it will decrement the reference count
		// Of the dependencies for assets that have them even when the asset itself
		// is not deallocated
		bool decrement_dependencies = false;
	};

	// Releases all assets from the database. If an asset is not found, it is placed into the missing assets list if specified, else ignored
	// If the asset mask is specified, then there must be ECS_ASSET_TYPE_COUNT streams to indicate which handles to unload
	// This is single threaded.
	ECSENGINE_API void DeallocateAssetsWithRemapping(
		AssetDatabase* database, 
		ResourceManager* resource_manager,
		const DeallocateAssetsWithRemappingOptions* options,
		CapacityStream<MissingAsset>* missing_assets = nullptr
	);

	// Releases all assets from the given asset database reference. It will unload these assets that have the reference count to 0 after
	// their elimination from the main database (the reference count is decremented in all cases).
	// If the asset mask is specified, then there must be ECS_ASSET_TYPE_COUNT streams to indicate which handles to unload
	ECSENGINE_API void DeallocateAssetsWithRemapping(
		AssetDatabaseReference* database_reference,
		ResourceManager* resource_manager,
		const DeallocateAssetsWithRemappingOptions* options
	);

	ECSENGINE_API bool LoadAssetHasFailed(Stream<LoadAssetFailure> failures, unsigned int handle, ECS_ASSET_TYPE type);

}