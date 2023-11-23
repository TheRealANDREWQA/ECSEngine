#include "editorpch.h"
#include "ECSEngineAssets.h"
#include "ECSEngineComponentsAll.h"

#include "../Editor/EditorEvent.h"
#include "../Editor/EditorState.h"
#include "../Sandbox/SandboxEntityOperations.h"
#include "../Sandbox/Sandbox.h"
#include "../Project/ProjectFolders.h"
#include "EditorSandboxAssets.h"
#include "AssetManagement.h"

using namespace ECSEngine;

// -----------------------------------------------------------------------------------------------------------------------------

void FromDeallocateAssetDependencyToUpdateAssetToComponentElement(
	CapacityStream<UpdateAssetToComponentElement>* update_assets, 
	Stream<DeallocateAssetDependency> deallocate_dependencies
) {
	for (size_t index = 0; index < deallocate_dependencies.size; index++) {
		UpdateAssetToComponentElement element;
		element.new_asset = deallocate_dependencies[index].new_pointer;
		element.old_asset = deallocate_dependencies[index].previous_pointer;
		element.type = deallocate_dependencies[index].type;
		update_assets->Add(element);
	}
}

struct DeallocateAssetWithRemappingEventData {
	Stream<Stream<unsigned int>> handles;
	bool force_execution;
};

EDITOR_EVENT(DeallocateAssetWithRemappingEvent) {
	DeallocateAssetWithRemappingEventData* data = (DeallocateAssetWithRemappingEventData*)_data;
	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING) || data->force_execution) {
		size_t total_size = 0;
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			total_size += data->handles[index].size;
		}

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB * 4);
		CapacityStream<UpdateAssetToComponentElement> update_assets;
		update_assets.Initialize(&stack_allocator, 0, total_size);

		size_t update_index = 0;
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;

			for (size_t subindex = 0; subindex < data->handles[index].size; subindex++) {
				ECS_STACK_CAPACITY_STREAM(DeallocateAssetDependency, deallocate_dependencies, 512);
				void* metadata = editor_state->asset_database->GetAsset(data->handles[index][subindex], current_type);

				bool success = DeallocateAsset(editor_state, metadata, current_type, true, &deallocate_dependencies);
				if (!success) {
					Stream<wchar_t> target_file = GetAssetFile(metadata, current_type);
					Stream<char> asset_name = GetAssetName(metadata, current_type);
					const char* type_string = ConvertAssetTypeString(current_type);

					ECS_STACK_CAPACITY_STREAM(char, console_message, 512);
					if (target_file.size > 0) {
						ECS_FORMAT_STRING(console_message, "Failed to unload asset {#}, type {#} and target file {#} or assets that depend on it.", asset_name, type_string, target_file);
					}
					else {
						ECS_FORMAT_STRING(console_message, "Failed to unload asset {#}, type {#} or assets that depend on it.", asset_name, type_string);
					}
					EditorSetConsoleError(console_message);
				}

				// Set the new_asset for those assets that have been changed
				update_assets.Expand(GetAllocatorPolymorphic(&stack_allocator), deallocate_dependencies.size);
				FromDeallocateAssetDependencyToUpdateAssetToComponentElement(&update_assets, deallocate_dependencies);
			}
		}

		// Update the link components
		UpdateAssetsToComponents(editor_state, update_assets);
		return false;
	}
	else {
		return true;
	}
}

struct DeallocateAssetWithRemappingWithOldMetadataEventData {
	ECS_ASSET_TYPE type;
	bool force_execution;
private:
	char padding[6];
public:
	// Store the old_metadata directly here
	char old_metadata[AssetMetadataMaxByteSize()];
};

EDITOR_EVENT(DeallocateAssetWithRemappingWithOldMetadataEvent) {
	DeallocateAssetWithRemappingWithOldMetadataEventData* data = (DeallocateAssetWithRemappingWithOldMetadataEventData*)_data;
	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING) || data->force_execution) {
		ECS_STACK_CAPACITY_STREAM(UpdateAssetToComponentElement, update_assets, 256);
		ECS_STACK_CAPACITY_STREAM(DeallocateAssetDependency, deallocate_dependency, 256);

		// Deallocate the asset and then randomize it (already done in the Deallocate function)
		bool success = DeallocateAsset(editor_state, data->old_metadata, data->type, true, &deallocate_dependency);
		if (!success) {
			Stream<wchar_t> target_file = GetAssetFile(data->old_metadata, data->type);
			Stream<char> asset_name = GetAssetName(data->old_metadata, data->type);
			const char* type_string = ConvertAssetTypeString(data->type);

			ECS_STACK_CAPACITY_STREAM(char, console_message, 512);
			if (target_file.size > 0) {
				ECS_FORMAT_STRING(console_message, "Failed to unload asset {#}, type {#} and target file {#} or assets that depend on it.", asset_name, type_string, target_file);
			}
			else {
				ECS_FORMAT_STRING(console_message, "Failed to unload asset {#}, type {#} or assets that depend on it.", asset_name, type_string);
			}
			EditorSetConsoleError(console_message);
		}
		else {
			FromDeallocateAssetDependencyToUpdateAssetToComponentElement(&update_assets, deallocate_dependency);

			// Update the link components
			UpdateAssetsToComponents(editor_state, update_assets);
		}
		return false;
	}
	else {
		return true;
	}
}

struct DeallocateAssetWithRemappingMetadataChangeEventData {
	Stream<Stream<unsigned int>> handles;
	bool force_execution;
};

EDITOR_EVENT(DeallocateAssetWithRemappingMetadataChangeEvent) {
	DeallocateAssetWithRemappingMetadataChangeEventData* data = (DeallocateAssetWithRemappingMetadataChangeEventData*)_data;
	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING) || data->force_execution) {
		size_t total_size = 0;
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			total_size += data->handles[index].size;
		}

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(update_assets_allocator, ECS_KB * 64, ECS_MB);
		CapacityStream<UpdateAssetToComponentElement> update_assets;
		update_assets.Initialize(&update_assets_allocator, 0, total_size);

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_file_asset_allocator, ECS_KB * 64, ECS_MB);
		AllocatorPolymorphic file_asset_allocator = GetAllocatorPolymorphic(&_file_asset_allocator);

		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;

			for (size_t subindex = 0; subindex < data->handles[index].size; subindex++) {
				void* metadata = editor_state->asset_database->GetAsset(data->handles[index][subindex], current_type);

				Stream<char> asset_name = GetAssetName(metadata, current_type);
				Stream<wchar_t> target_file = GetAssetFile(metadata, current_type);
				alignas(alignof(size_t)) char file_metadata[AssetMetadataMaxByteSize()];
				// Temporarly copy the name and the file
				CreateDefaultAsset(file_metadata, StringCopy(file_asset_allocator, asset_name), StringCopy(file_asset_allocator, target_file), current_type);
				bool success = editor_state->asset_database->ReadAssetFile(
					asset_name, 
					target_file, 
					file_metadata, 
					current_type,
					file_asset_allocator
				);

				auto fail = [=](const char* message_with_file, const char* message_without_file) {
					const char* type_string = ConvertAssetTypeString(current_type);

					ECS_STACK_CAPACITY_STREAM(char, console_message, 512);
					if (target_file.size > 0) {
						ECS_FORMAT_STRING(console_message, message_with_file, asset_name, type_string, target_file);
					}
					else {
						ECS_FORMAT_STRING(console_message, message_without_file, asset_name, type_string);
					}
					EditorSetConsoleError(console_message);
				};

				if (success) {
					if (!CompareAssetOptions(metadata, file_metadata, current_type)) {
						ECS_STACK_CAPACITY_STREAM(DeallocateAssetDependency, external_dependencies, 256);

						// Deallocate the asset and then randomize it (already done in the Deallocate function)
						bool success = DeallocateAsset(editor_state, metadata, current_type, true, &external_dependencies);
						if (!success) {
							fail("Failed to unload asset {#}, type {#} and target file {#} or assets that depend on it.", "Failed to unload asset {#}, type {#} or assets that depend on it.");
						}
						else {
							ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, internal_dependencies, 256);
							GetAssetDependencies(metadata, current_type, &internal_dependencies);

							// Update the asset inside the asset database
							Stream<void> randomized_value = GetAssetFromMetadata(metadata, current_type);
							DeallocateAssetBase(metadata, current_type, editor_state->asset_database->Allocator());
							CopyAssetBase(metadata, file_metadata, current_type, editor_state->asset_database->Allocator());

							// Reinstate the old pointer value
							SetAssetToMetadata(metadata, current_type, randomized_value);

							// It is fine to clear the allocator since copying the old elements into the new buffer
							// is the same as copying in the same spot
							update_assets_allocator.Clear();
							update_assets.Expand(GetAllocatorPolymorphic(&update_assets_allocator), external_dependencies.size);
							FromDeallocateAssetDependencyToUpdateAssetToComponentElement(&update_assets, external_dependencies);

							// Remove the assets that were previously dependencies for the metadata
							for (unsigned int dependency = 0; dependency < internal_dependencies.size; dependency++) {
								DecrementAssetReference(editor_state, internal_dependencies[dependency].handle, internal_dependencies[dependency].type);
							}
						}
					}
					else {
						// We don't need to remove the time stamps because they were not added
						editor_state->asset_database->RemoveAssetDependencies(file_metadata, current_type);
					}
				}
				else {
					fail(
						"Failed to read metadata file for asset {#}, type {#} and target file {#} after it changed. The in memory version was not changed.",
						"Failed to read metadata file for asset {#}, type {#} after it changed. The in memory version was not changed."
					);
				}
			}
		}

		// Update the link components
		UpdateAssetsToComponents(editor_state, update_assets);
		return false;
	}
	else {
		return true;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void DeallocateAssetWithRemapping(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, bool commit)
{
	ECS_STACK_CAPACITY_STREAM(Stream<unsigned int>, handles, ECS_ASSET_TYPE_COUNT);
	for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
		handles[index] = { nullptr, 0 };
	}

	handles[type] = { &handle, 1 };
	handles.size = ECS_ASSET_TYPE_COUNT;
	DeallocateAssetsWithRemapping(editor_state, handles, commit);
}

// -----------------------------------------------------------------------------------------------------------------------------

void DeallocateAssetWithRemapping(EditorState* editor_state, const void* old_metadata, ECS_ASSET_TYPE type, bool commit)
{
	DeallocateAssetWithRemappingWithOldMetadataEventData event_data;
	event_data.type = type;
	memcpy(event_data.old_metadata, old_metadata, AssetMetadataByteSize(type));
	if (commit) {
		event_data.force_execution = true;
		DeallocateAssetWithRemappingWithOldMetadataEvent(editor_state, &event_data);
	}
	else {
		event_data.force_execution = false;
		EditorAddEvent(editor_state, DeallocateAssetWithRemappingWithOldMetadataEvent, &event_data, sizeof(event_data));
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void DeallocateAssetsWithRemapping(EditorState* editor_state, Stream<Stream<unsigned int>> handles, bool commit)
{
	DeallocateAssetWithRemappingEventData event_data;
	handles.size = ECS_ASSET_TYPE_COUNT;
	if (commit) {
		event_data.handles = handles;
		event_data.force_execution = true;
		DeallocateAssetWithRemappingEvent(editor_state, &event_data);
	}
	else {
		event_data.force_execution = false;
		event_data.handles = StreamCoalescedDeepCopy(handles, editor_state->EditorAllocator());
		EditorAddEvent(editor_state, DeallocateAssetWithRemappingEvent, &event_data, sizeof(event_data));
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void DeallocateAssetsWithRemappingMetadataChange(EditorState* editor_state, Stream<Stream<unsigned int>> handles, bool commit)
{
	DeallocateAssetWithRemappingMetadataChangeEventData event_data;
	handles.size = ECS_ASSET_TYPE_COUNT;
	if (commit) {
		event_data.handles = handles;
		event_data.force_execution = true;
		DeallocateAssetWithRemappingMetadataChangeEvent(editor_state, &event_data);
	}
	else {
		event_data.force_execution = false;
		event_data.handles = StreamCoalescedDeepCopy(handles, editor_state->EditorAllocator());
		EditorAddEvent(editor_state, DeallocateAssetWithRemappingMetadataChangeEvent, &event_data, sizeof(event_data));
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

unsigned int FindAssetBeingLoaded(const EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type)
{
	for (unsigned int index = 0; index < editor_state->loading_assets.size; index++) {
		if (editor_state->loading_assets[index].handle == handle && editor_state->loading_assets[index].type == type) {
			return index;
		}
	}
	return -1;
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetAssetSandboxesInUse(
	const EditorState* editor_state, 
	const void* metadata, 
	ECS_ASSET_TYPE type, 
	CapacityStream<unsigned int>* sandboxes
)
{
	SandboxAction(editor_state, -1, [=](unsigned int sandbox_index) {
		if (IsAssetReferencedInSandbox(editor_state, metadata, type, sandbox_index)) {
			sandboxes->AddAssert(sandbox_index);
		}
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

// Separate the atomic parts accordingly such that we don't have false sharing
struct LoadSandboxMissingAssetsEventData {
	AtomicStream<LoadAssetFailure>* failures;
private:
	char padding[ECS_CACHE_LINE_SIZE];
public:
	Semaphore semaphore;
private:
	char padding2[ECS_CACHE_LINE_SIZE];
public:
	AssetDatabase database;

	bool success;
	bool has_launched;
	bool insert_time_stamps;

	// This is needed only to unlock a sandbox, if that is desired
	unsigned int sandbox_index;

	// We need this list of pointers for randomized assets - meshes and materials are allocated
	// When registering materials tho it will use the allocation already made in order to copy the contents even when successful.
	// We also need the list of pointers such that we can update the link components
	void** previous_pointers[ECS_ASSET_TYPE_COUNT];
};

// -----------------------------------------------------------------------------------------------------------------------------

LoadAssetInfo LoadInfoFromEventData(LoadSandboxMissingAssetsEventData* event_data, Stream<wchar_t> mount_point) {
	LoadAssetInfo info;

	info.finish_semaphore = &event_data->semaphore;
	info.load_failures = event_data->failures;
	info.mount_point = mount_point;
	info.success = &event_data->success;

	return info;
}


EDITOR_EVENT(LoadSandboxMissingAssetsEvent) {
	LoadSandboxMissingAssetsEventData* data = (LoadSandboxMissingAssetsEventData*)_data;

	if (data->semaphore.count.load(ECS_RELAXED) != 0) {
		// It is not finished yet. Push the event again
		return true;
	}
	else {
		// Check to see if we have launched/need to launch the load
		if (!data->has_launched) {
			// We need to wait graphics initialization as well
			if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING) && EditorStateHasFlag(editor_state, EDITOR_STATE_RUNTIME_GRAPHICS_INITIALIZATION_FINISHED)) {
				// Can launch
				data->has_launched = true;

				EditorStateSetFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);
				EditorStateSetFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH);
				
				// Initialize the previous pointers
				for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
					ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;

					unsigned int count = data->database.GetAssetCount(current_type);
					data->previous_pointers[current_type] = (void**)editor_state->editor_allocator->Allocate(sizeof(void*) * count);
					for (unsigned int subindex = 0; subindex < count; subindex++) {
						data->previous_pointers[current_type][subindex] = GetAssetFromMetadata(
							data->database.GetAssetConst(data->database.GetAssetHandleFromIndex(subindex, current_type), current_type),
							current_type
						).buffer;
					}
				}
					
				ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
				GetProjectAssetsFolder(editor_state, assets_folder);
				LoadAssetInfo load_info = LoadInfoFromEventData(data, assets_folder);
				LoadAssets(&data->database, editor_state->RuntimeResourceManager(), editor_state->task_manager, &load_info);
			}
			return true;
		}
		else {
			ECS_STACK_CAPACITY_STREAM(char, failure_string, 512);
			unsigned int failure_count = data->failures->size.load(ECS_RELAXED);
			for (unsigned int index = 0; index < failure_count; index++) {
				data->failures->buffer[index].ToString(&data->database, failure_string);
				EditorSetConsoleError(failure_string);
				failure_string.size = 0;
			}

			// Deallocate the data and the database and clear the flags
			AllocatorPolymorphic allocator = editor_state->EditorAllocator();

			unsigned int total_asset_count = 0;
			for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
				total_asset_count += data->database.GetAssetCount((ECS_ASSET_TYPE)index);
			}

			// Copy the resources to the editor database that were loaded successfully.
			if (failure_count == 0) {
				editor_state->asset_database->CopyAssetPointers(&data->database);

				if (data->insert_time_stamps) {
					// Insert the time stamps for all assets since they all have loaded successfully
					data->database.ForEachAsset([&](unsigned int handle, ECS_ASSET_TYPE type) {
						InsertAssetTimeStamp(editor_state, data->database.GetAsset(handle, type), type, true);
					});
				}
			}
			else {
				// Create the asset mask and also record the time stamps
				ECS_STACK_CAPACITY_STREAM(Stream<unsigned int>, asset_mask, ECS_ASSET_TYPE_COUNT);

				Stream<LoadAssetFailure> failures = data->failures->ToStream();
				ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, asset_mask_storage, total_asset_count - failure_count);
				for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
					ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
					unsigned int current_count = data->database.GetAssetCount(current_type);
					asset_mask[current_type].buffer = asset_mask_storage.buffer + asset_mask_storage.size;
					for (unsigned int subindex = 0; subindex < current_count; subindex++) {
						unsigned int current_handle = data->database.GetAssetHandleFromIndex(subindex, current_type);
						if (!LoadAssetHasFailed(failures, current_handle, current_type)) {
							asset_mask[current_type].Add(current_handle);

							if (data->insert_time_stamps) {
								InsertAssetTimeStamp(editor_state, current_handle, current_type, true);
							}
						}
					}

					asset_mask_storage.size += asset_mask[current_type].size;
				}

				editor_state->asset_database->CopyAssetPointers(&data->database, asset_mask.buffer);
			}

			ECS_STACK_CAPACITY_STREAM_DYNAMIC(UpdateAssetToComponentElement, update_elements, total_asset_count);

			for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
				ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
				// Deallocate the mesh pointers for randomized assets
				unsigned int count = data->database.GetAssetCount(current_type);

				for (unsigned int subindex = 0; subindex < count; subindex++) {
					const void* metadata = data->database.GetAssetConst(data->database.GetAssetHandleFromIndex(subindex, current_type), current_type);
					Stream<void> current_asset = GetAssetFromMetadata(metadata, current_type);
					if (current_asset.buffer != data->previous_pointers[current_type][subindex]) {
						update_elements.Add({ { data->previous_pointers[current_type][subindex], 0 }, current_asset, current_type });
					}
				}
				Deallocate(allocator, data->previous_pointers[current_type]);
			}

			// Update the link components
			UpdateAssetsToComponents(editor_state, update_elements);

			// Remove the assets from the loading array before deallocating the database streams
			// Since then we won't be able to reference them back
			data->database.ForEachAsset([editor_state](unsigned int handle, ECS_ASSET_TYPE type) {
				RemoveLoadingAsset(editor_state, { handle, type });
			});

			data->database.DeallocateStreams();
			DeallocateIfBelongs(allocator, data->failures->buffer);
			Deallocate(allocator, data);

			EditorStateClearFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH);
			EditorStateClearFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);

			if (data->sandbox_index != -1) {
				// Unlock the sandbox as well
				UnlockSandbox(editor_state, data->sandbox_index);
			}

			return false;
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

const size_t MAX_FAILURE_COUNT = ECS_KB * 16;

LoadSandboxMissingAssetsEventData* InitializeEventDataBase(EditorState* editor_state, AtomicStream<LoadAssetFailure>* failures = nullptr) {
	AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();

	LoadSandboxMissingAssetsEventData* event_data = (LoadSandboxMissingAssetsEventData*)Allocate(editor_allocator, sizeof(LoadSandboxMissingAssetsEventData));

	if (failures != nullptr) {
		event_data->failures = failures;
	}
	else {
		event_data->failures = (AtomicStream<LoadAssetFailure>*)Allocate(editor_allocator, sizeof(AtomicStream<LoadAssetFailure>));
		event_data->failures->Initialize(editor_allocator, MAX_FAILURE_COUNT);
	}
	event_data->success = true;
	event_data->semaphore.ClearCount();
	event_data->has_launched = false;
	event_data->insert_time_stamps = true;
	event_data->sandbox_index = -1;

	return event_data;
}

LoadSandboxMissingAssetsEventData* InitializeEventData(
	EditorState* editor_state,  
	unsigned int sandbox_index, 
	CapacityStream<unsigned int>* missing_assets
) {
	LoadSandboxMissingAssetsEventData* data = InitializeEventDataBase(editor_state);
	data->database = editor_state->asset_database->Copy(missing_assets, editor_state->EditorAllocator());
	data->sandbox_index = sandbox_index;
	return data;
}

LoadSandboxMissingAssetsEventData* InitializeEventData(
	EditorState* editor_state,
	Stream<Stream<unsigned int>> missing_assets,
	AtomicStream<LoadAssetFailure>* failures = nullptr
) {
	LoadSandboxMissingAssetsEventData* data = InitializeEventDataBase(editor_state, failures);
	data->database = editor_state->asset_database->Copy(missing_assets.buffer, editor_state->EditorAllocator());
	data->sandbox_index = -1;
	return data;
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetSandboxMissingAssets(const EditorState* editor_state, unsigned int sandbox_index, CapacityStream<unsigned int>* missing_assets)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	const AssetDatabaseReference* reference = &sandbox->database;
	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);
	GetDatabaseMissingAssets(editor_state->runtime_resource_manager, reference, missing_assets, assets_folder, true);

	// For each handle, verify to see if it is being already being loaded
	for (size_t asset_type = 0; asset_type < ECS_ASSET_TYPE_COUNT; asset_type++) {
		for (unsigned int index = 0; index < missing_assets[asset_type].size; index++) {
			if (IsAssetBeingLoaded(editor_state, missing_assets[asset_type][index], (ECS_ASSET_TYPE)asset_type)) {
				missing_assets[asset_type].RemoveSwapBack(index);
				index--;
			}
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

static LinkComponentWithAssetFields GetLinkComponentWithAssetFieldForComponent(
	const EditorState* editor_state,
	Component component,
	ECS_COMPONENT_TYPE component_type,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search
) {
	LinkComponentWithAssetFields info_type;

	Stream<char> target_type_name = editor_state->editor_components.ComponentFromID(component.value, component_type);
	ECS_ASSERT(target_type_name.size > 0);

	ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 64);

	info_type.type = editor_state->editor_components.GetType(target_type_name);
	GetAssetFieldsFromLinkComponentTarget(info_type.type, asset_fields);

	bool is_material_dependency = false;
	if (deep_search) {
		for (size_t index = 0; index < asset_types.size && !is_material_dependency; index++) {
			is_material_dependency |= asset_types[index] == ECS_ASSET_TEXTURE || asset_types[index] == ECS_ASSET_GPU_SAMPLER || asset_types[index] == ECS_ASSET_SHADER;
		}
	}

	// Go through the asset fields. If the component doesn't have the given asset type, then don't bother with it
	for (unsigned int index = 0; index < asset_fields.size; index++) {
		size_t asset_index = 0;
		for (; asset_index < asset_types.size; asset_index++) {
			if (asset_types[asset_index] == asset_fields[index].type.type) {
				break;
			}
		}

		if (is_material_dependency && asset_fields[index].type.type == ECS_ASSET_MATERIAL) {
			// Make it such that the asset index is different from asset_types.size
			asset_index = -1;
		}

		if (asset_index == asset_types.size) {
			asset_fields.RemoveSwapBack(index);
			index--;
		}
	}

	info_type.asset_fields.InitializeAndCopy(allocator, asset_fields);
	return info_type;
}

static void GetLinkComponentWithAssetFieldsImpl(
	const EditorState* editor_state,
	unsigned int sandbox_index,
	LinkComponentWithAssetFields* link_with_fields,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search,
	ECS_COMPONENT_TYPE component_type
) {
	const EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	if (component_type == ECS_COMPONENT_UNIQUE) {
		entity_manager->ForEachComponent([&](Component component) {
			link_with_fields[component.value] = GetLinkComponentWithAssetFieldForComponent(editor_state, component, component_type, allocator, asset_types, deep_search);
		});
	}
	else if (component_type == ECS_COMPONENT_SHARED) {
		entity_manager->ForEachSharedComponent([&](Component component) {
			link_with_fields[component.value] = GetLinkComponentWithAssetFieldForComponent(editor_state, component, component_type, allocator, asset_types, deep_search);
		});
	}
	else if (component_type == ECS_COMPONENT_GLOBAL) {
		entity_manager->ForAllGlobalComponents([&](const void* global_data, Component component) {
			link_with_fields[component.value] = GetLinkComponentWithAssetFieldForComponent(editor_state, component, component_type, allocator, asset_types, deep_search);
		});
	}
	else {
		ECS_ASSERT(false, "Invalid component type");
	}
}

void GetLinkComponentsWithAssetFieldsUnique(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	LinkComponentWithAssetFields* link_with_fields, 
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search
)
{
	GetLinkComponentWithAssetFieldsImpl(editor_state, sandbox_index, link_with_fields, allocator, asset_types, deep_search, ECS_COMPONENT_UNIQUE);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetLinkComponentsWithAssetFieldsShared(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	LinkComponentWithAssetFields* link_with_fields, 
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search
)
{
	GetLinkComponentWithAssetFieldsImpl(editor_state, sandbox_index, link_with_fields, allocator, asset_types, deep_search, ECS_COMPONENT_SHARED);
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetLinkComponentsWithAssetFieldsGlobal(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	LinkComponentWithAssetFields* link_with_fields, 
	AllocatorPolymorphic allocator, 
	Stream<ECS_ASSET_TYPE> asset_types, 
	bool deep_search
)
{
	GetLinkComponentWithAssetFieldsImpl(editor_state, sandbox_index, link_with_fields, allocator, asset_types, deep_search, ECS_COMPONENT_GLOBAL);
}

// -----------------------------------------------------------------------------------------------------------------------------

void IncrementAssetReferenceInSandbox(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, unsigned int sandbox_index, unsigned int count)
{
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	for (unsigned int index = 0; index < count; index++) {
		sandbox->database.AddAsset(handle, type, true);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsAssetReferencedInSandbox(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type, unsigned int sandbox_index)
{
	bool is_referenced = false;

	if (sandbox_index == -1) {
		// We just need to return the find value from the main database - assets that have
		// dependencies will draw their dependencies into it as well
		is_referenced = editor_state->asset_database->FindAsset(name, file, type) != -1;
	}
	else {
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		AssetTypedHandle typed_handle = sandbox->database.FindDeep(name, file, type);
		is_referenced = typed_handle.handle != -1;
	}

	return is_referenced;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsAssetReferencedInSandbox(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, unsigned int sandbox_index)
{
	return IsAssetReferencedInSandbox(editor_state, GetAssetName(metadata, type), GetAssetFile(metadata, type), type, sandbox_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsAssetReferencedInSandboxEntities(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, unsigned int sandbox_index)
{
	bool return_value = false;
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 128, ECS_MB);
	AllocatorPolymorphic allocator = GetAllocatorPolymorphic(&_stack_allocator);
	Stream<void> asset_pointer = GetAssetFromMetadata(metadata, type);

	SandboxAction<true>(editor_state, sandbox_index, [&](unsigned int sandbox_index) {
		const EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
		unsigned int unique_component_count = entity_manager->GetMaxComponent().value + 1;
		unsigned int shared_component_count = entity_manager->GetMaxSharedComponent().value + 1;
		unsigned int global_component_count = entity_manager->GetGlobalComponentCount() + 1;

		unsigned int max_component_count = std::max(std::max(unique_component_count, shared_component_count), global_component_count);

		ECS_STACK_CAPACITY_STREAM_DYNAMIC(LinkComponentWithAssetFields, link_with_fields, max_component_count);
		GetLinkComponentsWithAssetFieldsUnique(editor_state, sandbox_index, link_with_fields.buffer, allocator, { &type, 1 });

		unsigned char components_to_visit[ECS_ARCHETYPE_MAX_COMPONENTS];
		unsigned char component_to_visit_count = 0;
		const Reflection::ReflectionType* component_reflection_types[ECS_ARCHETYPE_MAX_COMPONENTS];

		SandboxForAllUniqueComponents(editor_state, sandbox_index,
			[&](const Archetype* archetype) {
				component_to_visit_count = 0;
				ComponentSignature signature = archetype->GetUniqueSignature();
				for (unsigned char index = 0; index < signature.count; index++) {
					if (link_with_fields[signature[index]].asset_fields.size > 0) {
						components_to_visit[component_to_visit_count] = index;
						component_reflection_types[component_to_visit_count++] = editor_state->editor_components.GetType(signature[index], false);
						ECS_ASSERT(component_reflection_types[component_to_visit_count - 1] != nullptr);
					}
				}
			},
			[&](const Archetype* archetype, const ArchetypeBase* base, Entity entity, void** unique_components) {
				ComponentSignature signature = archetype->GetUniqueSignature();
				ECS_STACK_CAPACITY_STREAM(Stream<void>, asset_data, 128);
				for (unsigned char index = 0; index < component_to_visit_count; index++) {
					Stream<LinkComponentAssetField> asset_fields = link_with_fields[signature[components_to_visit[index]]].asset_fields;
					GetLinkComponentAssetDataForTargetDeep(
						component_reflection_types[index],
						unique_components[components_to_visit[index]],
						asset_fields,
						editor_state->asset_database,
						type,
						&asset_data
					);

					for (unsigned int subindex = 0; subindex < asset_data.size; subindex++) {
						if (CompareAssetPointers(asset_data[subindex].buffer, asset_pointer.buffer, type)) {
							return_value = true;
							return true;
						}
					}
				}
				return false;
			}
		);

		_stack_allocator.Clear();
		if (!return_value) {
			GetLinkComponentsWithAssetFieldsShared(editor_state, sandbox_index, link_with_fields.buffer, allocator, { &type, 1 });

			// Check the shared components as well
			SandboxForAllSharedComponents<true>(editor_state, sandbox_index,
				[&](Component component) {
					component_reflection_types[0] = editor_state->editor_components.GetType(component, ECS_COMPONENT_SHARED);
					ECS_ASSERT(component_reflection_types[0] != nullptr);
				}, 
				[&](Component component, SharedInstance shared_instance) {
					ECS_STACK_CAPACITY_STREAM(Stream<void>, asset_data, 128);
					Stream<LinkComponentAssetField> asset_fields = link_with_fields[component].asset_fields;
					GetLinkComponentAssetDataForTargetDeep(
						component_reflection_types[0],
						entity_manager->GetSharedData(component, shared_instance),
						asset_fields,
						editor_state->asset_database,
						type,
						&asset_data
					);

					for (unsigned int subindex = 0; subindex < asset_data.size; subindex++) {
						if (CompareAssetPointers(asset_data[subindex].buffer, asset_pointer.buffer, type)) {
							return_value = true;
							return true;
						}
					}

					return return_value;
				}
			);
		}

		_stack_allocator.Clear();
		if (!return_value) {
			GetLinkComponentsWithAssetFieldsGlobal(editor_state, sandbox_index, link_with_fields.buffer, allocator, { &type, 1 });

			// Check the global components as well
			SandboxForAllGlobalComponents<true>(editor_state, sandbox_index,
				[&](const void* data, Component component) {
					component_reflection_types[0] = editor_state->editor_components.GetType(component, ECS_COMPONENT_GLOBAL);
					ECS_ASSERT(component_reflection_types[0] != nullptr);
					ECS_STACK_CAPACITY_STREAM(Stream<void>, asset_data, 128);
					Stream<LinkComponentAssetField> asset_fields = link_with_fields[component].asset_fields;
					GetLinkComponentAssetDataForTargetDeep(
						component_reflection_types[0],
						data,
						asset_fields,
						editor_state->asset_database,
						type,
						&asset_data
					);

					for (unsigned int subindex = 0; subindex < asset_data.size; subindex++) {
						if (CompareAssetPointers(asset_data[subindex].buffer, asset_pointer.buffer, type)) {
							return_value = true;
							return true;
						}
					}

					return return_value;
				}
			);
		}

		return return_value;
	});

	return return_value;
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsAssetBeingLoaded(const EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type)
{
	return FindAssetBeingLoaded(editor_state, handle, type) != -1;
}

// -----------------------------------------------------------------------------------------------------------------------------

void LoadSandboxMissingAssets(EditorState* editor_state, unsigned int sandbox_index, CapacityStream<unsigned int>* missing_assets)
{
	LoadSandboxMissingAssets(editor_state, sandbox_index, missing_assets, nullptr, nullptr, 0);
}

void LoadSandboxMissingAssets(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	CapacityStream<unsigned int>* missing_assets, 
	EditorEventFunction callback,
	void* callback_data, 
	size_t callback_data_size
)
{
	// Check to see if there are actually any assets to be loaded
	unsigned int total_count = 0;
	for (size_t asset_type = 0; asset_type < ECS_ASSET_TYPE_COUNT; asset_type++) {
		total_count += missing_assets[asset_type].size;
	}

	if (total_count > 0) {
		// Add the assets to the loading array
		AddLoadingAssets(editor_state, missing_assets);

		LoadSandboxMissingAssetsEventData* event_data = InitializeEventData(editor_state, sandbox_index, missing_assets);
		if (callback == nullptr) {
			EditorAddEvent(editor_state, LoadSandboxMissingAssetsEvent, event_data, 0);
		}
		else {
			EditorAddEventWithContinuation(editor_state, LoadSandboxMissingAssetsEvent, event_data, 0, callback, callback_data, callback_data_size);
		}
		// Lock the sandbox as well
		LockSandbox(editor_state, sandbox_index);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void LoadSandboxAssets(EditorState* editor_state, unsigned int sandbox_index)
{
	LoadSandboxAssets(editor_state, sandbox_index, nullptr, nullptr, 0);
}

// -----------------------------------------------------------------------------------------------------------------------------

void LoadSandboxAssets(EditorState* editor_state, unsigned int sandbox_index, EditorEventFunction callback, void* callback_data, size_t callback_data_size)
{
	const size_t HANDLES_PER_ASSET_TYPE = ECS_KB;
	ECS_STACK_CAPACITY_STREAM_OF_STREAMS(unsigned int, missing_assets, ECS_ASSET_TYPE_COUNT, HANDLES_PER_ASSET_TYPE);
	GetSandboxMissingAssets(editor_state, sandbox_index, missing_assets.buffer);
	LoadSandboxMissingAssets(editor_state, sandbox_index, missing_assets.buffer, callback, callback_data, callback_data_size);
}

// -----------------------------------------------------------------------------------------------------------------------------

void LoadAssetsWithRemapping(EditorState* editor_state, Stream<Stream<unsigned int>> handles)
{
	// Determine which handles are not already being loaded
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
	bool has_entries = false;
	Stream<Stream<unsigned int>> not_loading_handles = GetNotLoadedAssets(editor_state, GetAllocatorPolymorphic(&stack_allocator), handles, &has_entries);

	if (has_entries) {
		// Add these entries to the loading array
		AddLoadingAssets(editor_state, not_loading_handles);

		LoadSandboxMissingAssetsEventData* event_data = InitializeEventData(editor_state, not_loading_handles);
		EditorAddEvent(editor_state, LoadSandboxMissingAssetsEvent, event_data, 0);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

// The registration needs to be moved into an event in case the resource manager is locked
bool RegisterSandboxAsset(
	EditorState* editor_state, 
	unsigned int sandbox_index, 
	Stream<char> name, 
	Stream<wchar_t> file, 
	ECS_ASSET_TYPE type, 
	unsigned int* handle,
	bool unregister_if_exists,
	UIActionHandler callback
)
{
	return AddRegisterAssetEvent(editor_state, name, file, type, handle, sandbox_index, unregister_if_exists, callback);
}

// -----------------------------------------------------------------------------------------------------------------------------

void FinishReloadAsset(EditorState* editor_state, Stream<UpdateAssetToComponentElement> update_elements, Stream<unsigned int> dirty_sandboxes) {
	if (update_elements.size > 0) {
		UpdateAssetsToComponents(editor_state, update_elements);

		// Now we need to get all sandboxes which use these elements and re-render them
		ECS_STACK_CAPACITY_STREAM(unsigned int, update_dirty_sandboxes, 512);
		for (size_t index = 0; index < update_elements.size; index++) {
			ECS_STACK_CAPACITY_STREAM(unsigned int, current_sandboxes, 512);
			unsigned int handle = editor_state->asset_database->FindAssetEx(update_elements[index].new_asset, update_elements[index].type);
			if (handle == -1) {
				handle = editor_state->asset_database->FindAssetEx(update_elements[index].old_asset, update_elements[index].type);
			}

			// It might happen that the asset is invalid before hand and it still is invalid
			// In that case, don't do anything
			if (handle != -1) {
				const void* metadata = editor_state->asset_database->GetAssetConst(handle, update_elements[index].type);
				GetAssetSandboxesInUse(editor_state, metadata, update_elements[index].type, &current_sandboxes);

				StreamAddUniqueSearchBytes(update_dirty_sandboxes, current_sandboxes);
			}
		}

		// Proceed with the re-rendering of the sandboxes which use these assets
		// Re-render both the scene and the runtime
		for (unsigned int index = 0; index < update_dirty_sandboxes.size; index++) {
			RenderSandboxViewports(editor_state, update_dirty_sandboxes[index]);
		}
	}
	
	// Now re-render the external dirty sandboxes
	for (unsigned int index = 0; index < dirty_sandboxes.size; index++) {
		RenderSandboxViewports(editor_state, dirty_sandboxes[index]);
	}
}

// Returns a component with type ECS_ASSET_TYPE_COUNT when there is nothing to be update
UpdateAssetToComponentElement ReloadAssetTaskIteration(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE asset_type, Stream<wchar_t> assets_folder) {
	void* metadata = editor_state->asset_database->GetAsset(handle, asset_type);
	Stream<void> old_asset = GetAssetFromMetadata(metadata, asset_type);

	ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, assets_to_be_removed, 512);
	bool2 success = ReloadAssetFromMetadata(editor_state->RuntimeResourceManager(), editor_state->asset_database, metadata, asset_type, assets_folder, &assets_to_be_removed);
	
	for (size_t index = 0; index < assets_to_be_removed.size; index++) {
		DecrementAssetReference(editor_state, assets_to_be_removed[index].handle, assets_to_be_removed[index].type);
	}

	if (!success.x || !success.y) {
		Stream<char> asset_name = GetAssetName(metadata, asset_type);
		Stream<wchar_t> asset_file = GetAssetFile(metadata, asset_type);
		Stream<char> asset_type_string = ConvertAssetTypeString(asset_type);
		ECS_STACK_CAPACITY_STREAM(char, console_message, 512);

		Stream<char> detailed_part = !success.x ? "The deallocation part failed." : "The creation part failed.";

		if (asset_file.size > 0) {
			ECS_FORMAT_STRING(console_message, "Failed to reload asset {#}, target file {#}, type {#}. {#}", asset_name, asset_file, asset_type_string, detailed_part);
		}
		else {
			ECS_FORMAT_STRING(console_message, "Failed to reload asset {#}, type {#}. {#}", asset_name, asset_type_string, detailed_part);
		}
		EditorSetConsoleError(console_message);

		if (success.x) {
			// The deallocation went through - randomize the pointer and update the component
			unsigned int randomized_value = editor_state->asset_database->RandomizePointer(handle, asset_type);
			UpdateAssetToComponentElement update_element;
			update_element.old_asset = old_asset;
			update_element.new_asset = { (void*)randomized_value, 0 };
			update_element.type = asset_type;

			return update_element;
		}
	}
	else {
		UpdateAssetToComponentElement update_element;
		update_element.old_asset = old_asset;
		update_element.new_asset = GetAssetFromMetadata(metadata, asset_type);
		update_element.type = asset_type;

		return update_element;
	}

	return { {}, {}, ECS_ASSET_TYPE_COUNT };
}

struct ReloadEventData {
	Stream<Stream<unsigned int>> asset_handles;
};

EDITOR_EVENT(ReloadEvent) {
	ReloadEventData* data = (ReloadEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
		GetProjectAssetsFolder(editor_state, assets_folder);

		ECS_STACK_CAPACITY_STREAM(UpdateAssetToComponentElement, update_elements, 1024);

		// Go through each asset and reload it individually
		for (size_t type = 0; type < ECS_ASSET_TYPE_COUNT; type++) {
			ECS_ASSET_TYPE asset_type = (ECS_ASSET_TYPE)type;
			for (size_t index = 0; index < data->asset_handles[asset_type].size; index++) {
				unsigned int handle = data->asset_handles[asset_type][index];
				UpdateAssetToComponentElement update_element = ReloadAssetTaskIteration(editor_state, handle, asset_type, assets_folder);
				if (update_element.type != ECS_ASSET_TYPE_COUNT && update_element.IsAssetDifferent()) {
					update_elements.AddAssert(update_element);
				}
			}
		}

		FinishReloadAsset(editor_state, update_elements, {});
		// Remove the loading assets entries
		RemoveLoadingAssets(editor_state, data->asset_handles);

		data->asset_handles.Deallocate(editor_state->EditorAllocator());
		return false;
	}
	else {
		return true;
	}
}

void ReloadAssets(EditorState* editor_state, Stream<Stream<unsigned int>> assets_to_reload)
{
	// In order to make sure that there are no conflicts, for reloads take into considerations
	// Loads as well. In this way, we avoid load-reload conflicts (for assets that failed, this
	// is the only case where it can happen) but also reload-reload conflicts
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
	bool has_entries = false;
	Stream<Stream<unsigned int>> not_loaded_assets = GetNotLoadedAssets(editor_state, GetAllocatorPolymorphic(&stack_allocator), assets_to_reload, &has_entries);

	if (has_entries) {
		// Add the entries to the load array
		AddLoadingAssets(editor_state, not_loaded_assets);

		ReloadEventData event_data;
		event_data.asset_handles = StreamCoalescedDeepCopy(not_loaded_assets, editor_state->EditorAllocator());
		EditorAddEvent(editor_state, ReloadEvent, &event_data, sizeof(event_data));
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

struct ReloadAssetsMetadataChangeEventData {
	Stream<Stream<unsigned int>> asset_handles;
};

EDITOR_EVENT(ReloadAssetsMetadataChangeEvent) {
	ReloadAssetsMetadataChangeEventData* data = (ReloadAssetsMetadataChangeEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
		GetProjectAssetsFolder(editor_state, assets_folder);

		ECS_STACK_CAPACITY_STREAM(UpdateAssetToComponentElement, update_elements, 1024);
		ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, old_dependencies, 512);

		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);

		// This is a separate allocator in order to use clear on the stack allocator inside the loop
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(temporary_allocator, ECS_KB * 8, ECS_MB);
		ECS_STACK_CAPACITY_STREAM(CapacityStream<unsigned int>, initial_assets, ECS_ASSET_TYPE_COUNT);
		ECS_STACK_CAPACITY_STREAM(CapacityStream<unsigned int>, new_assets_to_add, ECS_ASSET_TYPE_COUNT);
		for (size_t type = 0; type < ECS_ASSET_TYPE_COUNT; type++) {
			initial_assets[type].Initialize(&temporary_allocator, 0, 64);
			initial_assets[type].CopyOther(data->asset_handles[type]);
			new_assets_to_add[type].Initialize(&temporary_allocator, 0, 64);
		}

		// Keep track of the sandboxes that use these assets and re-render them at the end
		ECS_STACK_CAPACITY_STREAM(unsigned int, dirty_sandboxes, 512);

		auto calculate_remaining_assets = [](Stream<CapacityStream<unsigned int>> asset_handles) {
			unsigned int total = 0;
			for (unsigned int index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
				total += asset_handles[index].size;
			}
			return total;
		};

		auto loop = [&](Stream<CapacityStream<unsigned int>> asset_handles, Stream<CapacityStream<unsigned int>> assets_to_add, auto initial_reload) {
			// Go through each asset and reload it individually
			auto add_external_dependencies = [&](const void* metadata, ECS_ASSET_TYPE asset_type) {
				Stream<Stream<unsigned int>> external_dependencies = editor_state->asset_database->GetDependentAssetsFor(
					metadata,
					asset_type,
					GetAllocatorPolymorphic(&stack_allocator)
				);
				for (size_t subtype = 0; subtype < ECS_ASSET_TYPE_COUNT; subtype++) {
					assets_to_add[subtype].AddStreamSafe(external_dependencies[subtype]);
				}
			};

			// Empty the assets to add
			for (size_t type = 0; type < ECS_ASSET_TYPE_COUNT; type++) {
				assets_to_add[type].size = 0;
			}

			for (size_t type = 0; type < ECS_ASSET_TYPE_COUNT; type++) {
				ECS_ASSET_TYPE asset_type = (ECS_ASSET_TYPE)type;

				for (size_t index = 0; index < asset_handles[asset_type].size; index++) {
					unsigned int handle = asset_handles[asset_type][index];
					void* metadata = editor_state->asset_database->GetAsset(handle, asset_type);

					// Add the referenced sandboxes to the re-render list
					ECS_STACK_CAPACITY_STREAM(unsigned int, current_asset_sandboxes, 512);
					GetAssetSandboxesInUse(editor_state, metadata, asset_type, &current_asset_sandboxes);
					StreamAddUniqueSearchBytes(dirty_sandboxes, current_asset_sandboxes);

					if constexpr (!initial_reload) {
						UpdateAssetToComponentElement update_element = ReloadAssetTaskIteration(editor_state, handle, asset_type, assets_folder);
						if (update_element.type != ECS_ASSET_TYPE_COUNT) {
							update_elements.AddAssert(update_element);

							// If the new pointer is not randomized, then it means that the reload was successful
							// In that case add the external dependencies to the assets_to_add
							if (IsAssetFromMetadataValid(metadata, asset_type)) {
								add_external_dependencies(metadata, asset_type);
							}
						}
					}
					else {
						Stream<void> old_asset = GetAssetFromMetadata(metadata, asset_type);

						old_dependencies.size = 0;
						GetAssetDependencies(metadata, asset_type, &old_dependencies);

						Stream<char> current_name = GetAssetName(metadata, asset_type);
						Stream<wchar_t> current_file = GetAssetFile(metadata, asset_type);

						alignas(alignof(void*)) char file_metadata[AssetMetadataMaxByteSize()];
						CreateDefaultAsset(file_metadata, current_name, current_file, asset_type);

						stack_allocator.Clear();

						// Read the metadata file
						bool success = editor_state->asset_database->ReadAssetFile(
							current_name,
							current_file,
							file_metadata,
							asset_type,
							GetAllocatorPolymorphic(&stack_allocator)
						);
						if (success) {
							// Set the pointer of the file metadata to the one in the database and then update the database
							SetAssetToMetadata(file_metadata, asset_type, old_asset);

							if (ValidateAssetMetadataOptions(file_metadata, asset_type)) {
								ReloadAssetResult reload_result = ReloadAssetFromMetadata(
									editor_state->RuntimeResourceManager(),
									editor_state->asset_database,
									metadata,
									file_metadata,
									asset_type,
									assets_folder
								);
								if (reload_result.is_different) {
									if (!reload_result.success.x || !reload_result.success.y) {
										Stream<char> asset_type_string = ConvertAssetTypeString(asset_type);
										ECS_STACK_CAPACITY_STREAM(char, console_message, 512);

										Stream<char> detailed_part = !reload_result.success.x ? "The deallocation part failed." : "The creation part failed.";

										if (current_file.size > 0) {
											ECS_FORMAT_STRING(console_message, "Failed to reload asset {#}, target file {#}, type {#}. {#}",
												current_name, current_file, asset_type_string, detailed_part);
										}
										else {
											ECS_FORMAT_STRING(console_message, "Failed to reload asset {#}, type {#}. {#}", current_name, asset_type_string, detailed_part);
										}
										EditorSetConsoleError(console_message);

										if (reload_result.success.x) {
											// The assets has been deallocated, we need to randomize the pointer and update the components
											unsigned int randomized_value = editor_state->asset_database->RandomizePointer(handle, asset_type);
											UpdateAssetToComponentElement update_element;
											update_element.old_asset = old_asset;
											update_element.new_asset = { (void*)randomized_value, 0 };
											update_element.type = asset_type;

											update_elements.AddAssert(update_element);
										}

										// We need to remove the dependencies of the file metadata
										// We don't need to remove the time stamps because they were not added
										editor_state->asset_database->RemoveAssetDependencies(file_metadata, asset_type);
									}
									else {
										UpdateAssetToComponentElement update_element;
										update_element.old_asset = old_asset;
										update_element.new_asset = GetAssetFromMetadata(file_metadata, asset_type);
										update_element.type = asset_type;

										// For some assets the pointer might not change
										if (update_element.new_asset.buffer != old_asset.buffer) {
											update_elements.AddAssert(update_element);
										}

										// We need to remove the references to the old dependencies
										// If an asset was removed from the dependency list and is being kept alive
										// only by this reference it will cause an unnecessary unload. So update the metadata
										// in the database first before proceeding

										// Copy the asset into a temporary, copy the new asset fields and then deallocate the old one
										// otherwise the old data can be deallocated and when reallocating the values to be overwritten
										size_t temporary_metadata[AssetMetadataMaxSizetSize()];
										memcpy(temporary_metadata, metadata, AssetMetadataByteSize(asset_type));
										CopyAssetBase(metadata, file_metadata, asset_type, editor_state->asset_database->Allocator());

										DeallocateAssetBase(temporary_metadata, asset_type, editor_state->asset_database->Allocator());

										SetAssetToMetadata(metadata, asset_type, GetAssetFromMetadata(file_metadata, asset_type));

										for (unsigned int dependency = 0; dependency < old_dependencies.size; dependency++) {
											DecrementAssetReference(editor_state, old_dependencies[dependency].handle, old_dependencies[dependency].type);
										}

										// We also need to get the internal dependencies for the file metadata and insert time stamps for
										// assets that have been missing
										ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, file_internal_dependencies, 128);
										GetAssetDependencies(file_metadata, asset_type, &file_internal_dependencies);

										for (unsigned int subindex = 0; subindex < file_internal_dependencies.size; subindex++) {
											InsertAssetTimeStamp(editor_state, file_internal_dependencies[subindex].handle, file_internal_dependencies[subindex].type, true);
										}
									}
									
									// The external dependencies need to be added no matter what if the asset succeeded or not
									add_external_dependencies(metadata, asset_type);
								}
								else {
									// We need to remove the reference count additions made by the file read
									// We don't need to remove the time stamps because they were not added
									editor_state->asset_database->RemoveAssetDependencies(file_metadata, asset_type);
								}
							}
							else {
								// Remove the reference count increments for internal dependencies by the previous metadata
								// And deallocate the previous data. Also we need to create any dependencies that the new asset
								// brought with it and were not loaded before
								ECS_STACK_CAPACITY_STREAM(DeallocateAssetDependency, deallocate_dependencies, 512);
								ECS_STACK_CAPACITY_STREAM(CreateAssetInternalDependenciesElement, create_dependencies, 512);

								bool deallocate_success = DeallocateAsset(editor_state, metadata, asset_type, true, &deallocate_dependencies);
								if (!deallocate_success) {
									ECS_STACK_CAPACITY_STREAM(char, asset_string, 512);
									AssetToString(metadata, asset_type, asset_string);
									ECS_FORMAT_TEMP_STRING(console_message, "Failed to deallocate asset {#}.", asset_string);
									EditorSetConsoleError(console_message);
								}
								FromDeallocateAssetDependencyToUpdateAssetToComponentElement(&update_elements, deallocate_dependencies);

								bool internal_success = CreateAssetInternalDependencies(editor_state, file_metadata, asset_type, &create_dependencies);
								for (unsigned int dependency = 0; dependency < create_dependencies.size; dependency++) {
									if (create_dependencies[dependency].WasChanged()) {
										update_elements.AddAssert({
											create_dependencies[dependency].old_asset,
											create_dependencies[dependency].new_asset,
											create_dependencies[dependency].type
										});
									}
								}

								// This value should be randomized invalid pointer - since the asset was deallocated
								Stream<void> previous_asset_pointer = GetAssetFromMetadata(metadata, asset_type);

								// We don't need to remove the time stamps because they were not added
								editor_state->asset_database->RemoveAssetDependencies(metadata, asset_type);

								// Copy back into the database the new asset
								DeallocateAssetBase(metadata, asset_type, editor_state->asset_database->Allocator());
								CopyAssetBase(metadata, file_metadata, asset_type, editor_state->asset_database->Allocator());

								SetAssetToMetadata(metadata, asset_type, previous_asset_pointer);
							}
						}
					}
				}
			}
		};


		loop(initial_assets, new_assets_to_add, std::true_type{});

		// 0 means use initial assets for the main one, 1 means use new assets_to_add 
		unsigned char asset_handle_index = 1;
		unsigned int initial_assets_count = 0;
		unsigned int new_assets_to_add_count = calculate_remaining_assets(new_assets_to_add);
		while (initial_assets_count > 0 || new_assets_to_add_count > 0) {
			if (asset_handle_index == 0) {
				loop(initial_assets, new_assets_to_add, std::false_type{});

				initial_assets_count = 0;
				new_assets_to_add_count = calculate_remaining_assets(new_assets_to_add);
				asset_handle_index = 1;
			}
			else {
				loop(new_assets_to_add, initial_assets, std::false_type{});

				initial_assets_count = calculate_remaining_assets(initial_assets);
				new_assets_to_add_count = 0;
				asset_handle_index = 0;
			}
		}

		FinishReloadAsset(editor_state, update_elements, dirty_sandboxes);
		// Also remove the entries from the loading array
		RemoveLoadingAssets(editor_state, data->asset_handles);

		return false;
	}
	else {
		return true;
	}
}

void ReloadAssetsMetadataChange(EditorState* editor_state, Stream<Stream<unsigned int>> assets_to_reload)
{
	// The same as the other reload, check the loading assets entries to avoid load-reload or reload-reload conflicts
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
	bool has_entries = false;
	Stream<Stream<unsigned int>> not_loaded_assets = GetNotLoadedAssets(editor_state, GetAllocatorPolymorphic(&stack_allocator), assets_to_reload, &has_entries);

	if (has_entries) {
		AddLoadingAssets(editor_state, not_loaded_assets);

		ReloadAssetsMetadataChangeEventData event_data;
		event_data.asset_handles = StreamCoalescedDeepCopy(not_loaded_assets, editor_state->MultithreadedEditorAllocator());
		EditorAddEvent(editor_state, ReloadAssetsMetadataChangeEvent, &event_data, sizeof(event_data));
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_index, unsigned int handle, ECS_ASSET_TYPE type, UIActionHandler callback)
{
	UnregisterSandboxAssetElement element = { handle, type };
	UnregisterSandboxAsset(editor_state, sandbox_index, Stream<UnregisterSandboxAssetElement>{ &element, 1 }, callback);
}

// -----------------------------------------------------------------------------------------------------------------------------

void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_index, Stream<UnregisterSandboxAssetElement> elements, UIActionHandler callback)
{
	AddUnregisterAssetEvent(editor_state, elements, true, sandbox_index, callback);
}

// -----------------------------------------------------------------------------------------------------------------------------

void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_index, Stream<Stream<unsigned int>> elements, UIActionHandler callback) 
{
	AddUnregisterAssetEventHomogeneous(editor_state, elements, true, sandbox_index, callback);
}

// -----------------------------------------------------------------------------------------------------------------------------

void UnregisterSandboxLinkComponent(EditorState* editor_state, unsigned int sandbox_index, const void* link_component, Stream<char> component_name)
{
	const Reflection::ReflectionType* type = editor_state->editor_components.GetType(component_name);
	ECS_ASSERT(type != nullptr);

	ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 128);
	GetAssetFieldsFromLinkComponent(type, asset_fields);

	ECS_STACK_CAPACITY_STREAM(unsigned int, handles, 128);
	GetLinkComponentHandles(type, link_component, asset_fields, handles);

	ECS_STACK_CAPACITY_STREAM(UnregisterSandboxAssetElement, unload_elements, 128);

	for (unsigned int index = 0; index < handles.size; index++) {
		if (handles[index] != -1) {
			// Unload it
			unload_elements.Add({ handles[index], asset_fields[index].type.type });
		}
	}

	UnregisterSandboxAsset(editor_state, sandbox_index, unload_elements);
}

// -----------------------------------------------------------------------------------------------------------------------------

struct UnloadSandboxAssetsEventData {
	// The asset database reference will be copied here such that the external one on call can still be utilized
	AssetDatabaseReference sandbox_reference_copy;

	unsigned int sandbox_index;
	Stream<unsigned int> asset_mask[ECS_ASSET_TYPE_COUNT];
};

EDITOR_EVENT(UnloadSandboxAssetsEvent) {
	UnloadSandboxAssetsEventData* data = (UnloadSandboxAssetsEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING) && EditorStateHasFlag(editor_state, EDITOR_STATE_RUNTIME_GRAPHICS_INITIALIZATION_FINISHED)) {
		EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_index);
		ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
		GetProjectAssetsFolder(editor_state, assets_folder);

		DeallocateAssetsWithRemappingOptions deallocate_options;
		deallocate_options.asset_mask = data->asset_mask;
		deallocate_options.mount_point = assets_folder;
		DeallocateAssetsWithRemapping(&data->sandbox_reference_copy, editor_state->runtime_resource_manager, &deallocate_options);

		// Deallocate the streams
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			if (data->asset_mask[index].size > 0) {
				editor_state->editor_allocator->Deallocate(data->asset_mask[index].buffer);
			}
		}

		// Frees all the memory used
		data->sandbox_reference_copy.Reset();

		return false;
	}
	else {
		return true;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void UnloadSandboxAssets(EditorState* editor_state, unsigned int sandbox_index, bool clear_database_reference)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	UnloadSandboxAssetsEventData event_data;
	event_data.sandbox_index = sandbox_index;
	event_data.sandbox_reference_copy = sandbox->database.Copy(editor_state->EditorAllocator());
	EditorAddEvent(editor_state, UnloadSandboxAssetsEvent, &event_data, sizeof(event_data));

	// Copy the asset handles into separate allocations
	UnloadSandboxAssetsEventData* event_data_ptr = (UnloadSandboxAssetsEventData*)EditorEventLastData(editor_state);
	for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
		ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
		unsigned int current_count = sandbox->database.GetCount(current_type);
		event_data_ptr->asset_mask[current_type].Initialize(editor_state->editor_allocator, current_count);
		for (unsigned int subindex = 0; subindex < current_count; subindex++) {
			event_data_ptr->asset_mask[current_type][subindex] = sandbox->database.GetHandle(subindex, current_type);
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void UpdateAssetToComponents(EditorState* editor_state, Stream<void> old_asset, Stream<void> new_asset, ECS_ASSET_TYPE asset_type, unsigned int sandbox_index)
{
	UpdateAssetToComponentElement update_element;
	update_element.old_asset = old_asset;
	update_element.new_asset = new_asset;
	update_element.type = asset_type;
	UpdateAssetsToComponents(editor_state, { &update_element, 1 }, sandbox_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

void UpdateAssetsToComponents(EditorState* editor_state, Stream<UpdateAssetToComponentElement> elements, unsigned int sandbox_index)
{
	if (elements.size == 0) {
		return;
	}

	if (sandbox_index == -1) {
		for (unsigned int index = 0; index < editor_state->sandboxes.size; index++) {
			UpdateAssetsToComponents(editor_state, elements, index);
		}
		return;
	}

	ECS_STACK_CAPACITY_STREAM(bool, valid_types, ECS_ASSET_TYPE_COUNT);
	ECS_STACK_CAPACITY_STREAM(ECS_ASSET_TYPE, assets_to_check, ECS_ASSET_TYPE_COUNT);
	memset(valid_types.buffer, 0, sizeof(bool) * ECS_ASSET_TYPE_COUNT);
	for (size_t index = 0; index < elements.size; index++) {
		valid_types[elements[index].type] = true;
	}
	for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
		if (valid_types[index]) {
			assets_to_check.Add((ECS_ASSET_TYPE)index);
		}
	}

	auto perform_update = [&](EntityManager* entity_manager) {
		ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 128, ECS_MB);
		AllocatorPolymorphic stack_allocator = GetAllocatorPolymorphic(&_stack_allocator);

		// Firstly go through all unique components, get their reflection types, and asset fields
		Component max_unique_count = entity_manager->GetMaxComponent();
		max_unique_count.value = max_unique_count.value == -1 ? 0 : max_unique_count.value;

		CapacityStream<LinkComponentWithAssetFields> unique_types;
		unique_types.Initialize(stack_allocator, 0, max_unique_count.value);

		GetLinkComponentsWithAssetFieldsUnique(editor_state, sandbox_index, unique_types.buffer, stack_allocator, assets_to_check);

		Component components_to_check[ECS_ARCHETYPE_MAX_COMPONENTS];
		unsigned char components_indices[ECS_ARCHETYPE_MAX_COMPONENTS];
		unsigned char components_to_check_count = 0;

		// Now iterate through all entities and update the asset fields
		entity_manager->ForEachEntity(
			// The archetype initialize
			[&](Archetype* archetype) {
				components_to_check_count = 0;
				ComponentSignature signature = archetype->GetUniqueSignature();
				for (unsigned int index = 0; index < signature.count; index++) {
					if (unique_types[signature[index]].asset_fields.size > 0) {
						components_indices[components_to_check_count] = index;
						components_to_check[components_to_check_count++] = signature[index];
					}
				}
			},
			// The base archetype initialize - nothing to be done
				[&](Archetype* archetype, ArchetypeBase* base_archetype) {

			},
				[&](Archetype* archetype, ArchetypeBase* base_archetype, Entity entity, void** unique_components) {
				// Go through the components and update the value if it matches
				for (unsigned char index = 0; index < components_to_check_count; index++) {
					Stream<LinkComponentAssetField> asset_fields = unique_types[components_to_check[index]].asset_fields;
					for (size_t subindex = 0; subindex < asset_fields.size; subindex++) {
						for (size_t element_index = 0; element_index < elements.size; element_index++) {
							ECS_SET_ASSET_TARGET_FIELD_RESULT result = SetAssetTargetFieldFromReflectionIfMatches(
								unique_types[components_to_check[index]].type,
								asset_fields[subindex].field_index,
								unique_components[components_indices[index]],
								elements[element_index].new_asset,
								elements[element_index].type,
								elements[element_index].old_asset
							);
							if (result == ECS_SET_ASSET_TARGET_FIELD_MATCHED) {
								break;
							}
							ECS_ASSERT(result == ECS_SET_ASSET_TARGET_FIELD_OK || result == ECS_SET_ASSET_TARGET_FIELD_NONE);
						}
					}
				}
			}
			);

		// Clear the allocator
		_stack_allocator.Clear();

		// Do the same for shared components
		entity_manager->ForEachSharedComponent([&](Component component) {
			LinkComponentWithAssetFields link_with_fields = GetLinkComponentWithAssetFieldForComponent(
				editor_state,
				component,
				ECS_COMPONENT_SHARED,
				stack_allocator,
				assets_to_check,
				false
			);

			if (link_with_fields.asset_fields.size > 0) {
				entity_manager->ForEachSharedInstance(component, [&](SharedInstance instance) {
					void* instance_data = entity_manager->GetSharedData(component, instance);
					for (size_t index = 0; index < link_with_fields.asset_fields.size; index++) {
						for (size_t element_index = 0; element_index < elements.size; element_index++) {
							ECS_SET_ASSET_TARGET_FIELD_RESULT result = SetAssetTargetFieldFromReflectionIfMatches(
								link_with_fields.type,
								link_with_fields.asset_fields[index].field_index,
								instance_data,
								elements[element_index].new_asset,
								elements[element_index].type,
								elements[element_index].old_asset
							);
							if (result == ECS_SET_ASSET_TARGET_FIELD_MATCHED) {
								break;
							}
							ECS_ASSERT(result == ECS_SET_ASSET_TARGET_FIELD_OK || result == ECS_SET_ASSET_TARGET_FIELD_NONE);
						}
					}
					});
			}
		});

		_stack_allocator.Clear();

		// Do the same for global components
		entity_manager->ForAllGlobalComponents([&](void* data, Component component) {
			LinkComponentWithAssetFields link_with_fields = GetLinkComponentWithAssetFieldForComponent(
				editor_state,
				component,
				ECS_COMPONENT_GLOBAL,
				stack_allocator,
				assets_to_check,
				false
			);

			if (link_with_fields.asset_fields.size > 0) {
				for (size_t index = 0; index < link_with_fields.asset_fields.size; index++) {
					for (size_t element_index = 0; element_index < elements.size; element_index++) {
						ECS_SET_ASSET_TARGET_FIELD_RESULT result = SetAssetTargetFieldFromReflectionIfMatches(
							link_with_fields.type,
							link_with_fields.asset_fields[index].field_index,
							data,
							elements[element_index].new_asset,
							elements[element_index].type,
							elements[element_index].old_asset
						);
						if (result == ECS_SET_ASSET_TARGET_FIELD_MATCHED) {
							break;
						}
						ECS_ASSERT(result == ECS_SET_ASSET_TARGET_FIELD_OK || result == ECS_SET_ASSET_TARGET_FIELD_NONE);
					}
				}
			}
		});
	};

	// If we are in run mode, we need to update both entity managers. If in scene mode, only the scene entity manager
	EDITOR_SANDBOX_STATE sandbox_state = GetSandboxState(editor_state, sandbox_index);
	EntityManager* scene_entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
	perform_update(scene_entity_manager);
	if (sandbox_state != EDITOR_SANDBOX_SCENE) {
		EntityManager* runtime_entity_manager = GetSandboxEntityManager(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
		perform_update(runtime_entity_manager);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------
