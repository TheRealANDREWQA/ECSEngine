#pragma once
#include "../../Core.h"
#include "../../Containers/Stream.h"
#include "AssetMetadata.h"
#include "../../Utilities/FilePackaging.h"
#include "../../Containers/AtomicStream.h"

namespace ECSEngine {

	/*
		In order to use these calls for the AssetDatabaseReference you must convert it into
		a normal AssetDatabase first using the ToStandalone() call and then forward to the
		appropriate calls. The asset database needs to be persistent so the allocator with
		which is allocated needs to be stored by the caller and when the Semaphore is signaled
		for the finish then the caller can proceed to deallocate the allocator/database
	*/

	struct ResourceManager;
	struct AssetDatabase;
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

	// The mount path will be copied internally
	struct LoadAssetInfo {
		Semaphore* finish_semaphore;
		bool* success = nullptr;
		AtomicStream<LoadAssetFailure>* load_failures = nullptr;
		Stream<wchar_t> mount_point = { nullptr, 0 };
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

	// Releases all assets from the database. If an asset is not found, it is placed into the missing assets list if specified, else ignored
	// This is single threaded
	ECSENGINE_API void UnloadAssets(
		AssetDatabase* database, 
		ResourceManager* resource_manager,
		Stream<wchar_t> mount_point = { nullptr, 0 }, 
		CapacityStream<MissingAsset>* missing_assets = nullptr
	);


}