#include "editorpch.h"
#include "ECSEngineAssets.h"
#include "ECSEngineComponents.h"

#include "../Editor/EditorEvent.h"
#include "../Editor/EditorState.h"
#include "../Editor/EditorSandboxEntityOperations.h"
#include "../Editor/EditorSandbox.h"
#include "../Project/ProjectFolders.h"
#include "EditorSandboxAssets.h"
#include "AssetManagement.h"

using namespace ECSEngine;

// -----------------------------------------------------------------------------------------------------------------------------

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
		Stream<UpdateAssetToComponentElement> update_assets;
		update_assets.Initialize(&stack_allocator, total_size);

		CapacityStream<unsigned int> randomized_indices;

		size_t update_index = 0;
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;

			for (size_t subindex = 0; subindex < data->handles[index].size; subindex++) {
				void* metadata = editor_state->asset_database->GetAsset(data->handles[index][subindex], current_type);
				update_assets[update_index].old_asset = GetAssetFromMetadata(metadata, current_type);
				update_assets[update_index].type = current_type;

				// Deallocate the asset and then randomize it (already done in the Deallocate function)
				bool success = DeallocateAsset(editor_state, metadata, current_type);
				if (!success) {
					Stream<wchar_t> target_file = GetAssetFile(metadata, current_type);
					Stream<char> asset_name = GetAssetName(metadata, current_type);
					const char* type_string = ConvertAssetTypeString(current_type);

					ECS_STACK_CAPACITY_STREAM(char, console_message, 512);
					if (target_file.size > 0) {
						ECS_FORMAT_STRING(console_message, "Failed to unload asset {#}, type {#} and target file {#}.", asset_name, type_string, target_file);
					}
					else {
						ECS_FORMAT_STRING(console_message, "Failed to unload asset {#}, type {#}.", asset_name, type_string);
					}
					EditorSetConsoleError(console_message);
				}

				Stream<void> new_asset_value = GetAssetFromMetadata(metadata, current_type);
				update_assets[update_index].new_asset = new_asset_value;

				update_index++;
			}
		}

		// Update the link components
		UpdateAssetsToComponents(editor_state, update_assets);

		stack_allocator.ClearBackup();
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
	DeallocateAssetsWithRemapping(editor_state, handles, commit);
}

// -----------------------------------------------------------------------------------------------------------------------------

void DeallocateAssetsWithRemapping(EditorState* editor_state, Stream<Stream<unsigned int>> handles, bool commit)
{
	DeallocateAssetWithRemappingEventData event_data;
	if (commit) {
		event_data.handles = handles;
		event_data.force_execution = true;
		DeallocateAssetWithRemappingEvent(editor_state, &event_data);
	}
	else {
		event_data.force_execution = false;
		event_data.handles = StreamDeepCopy(handles, editor_state->EditorAllocator());
		EditorAddEvent(editor_state, DeallocateAssetWithRemappingEvent, &event_data, sizeof(event_data));
	}
}

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

			// Copy the resources to the editor database that were loaded successfully.
			if (failure_count == 0) {
				editor_state->asset_database->CopyAssetPointers(&data->database);

				if (data->insert_time_stamps) {
					// Insert the time stamps for all assets since they all have loaded successfully
					data->database.ForEachAsset([&](unsigned int handle, ECS_ASSET_TYPE type) {
						InsertAssetTimeStamp(editor_state, data->database.GetAsset(handle, type), type);
					});
				}
			}
			else {
				// Create the asset mask and also record the time stamps
				ECS_STACK_CAPACITY_STREAM(Stream<unsigned int>, asset_mask, ECS_ASSET_TYPE_COUNT);
				unsigned int total_asset_count = 0;
				for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
					total_asset_count += data->database.GetAssetCount((ECS_ASSET_TYPE)index);
				}

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
								InsertAssetTimeStamp(editor_state, current_handle, current_type);
							}
						}
					}

					asset_mask_storage.size += asset_mask[current_type].size;
				}

				editor_state->asset_database->CopyAssetPointers(&data->database, asset_mask.buffer);
			}

			for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
				ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
				// Deallocate the mesh pointers for randomized assets
				unsigned int count = data->database.GetAssetCount(current_type);
				for (unsigned int subindex = 0; subindex < count; subindex++) {
					const void* metadata = data->database.GetAssetConst(data->database.GetAssetHandleFromIndex(subindex, current_type), current_type);
					Stream<void> current_asset = GetAssetFromMetadata(metadata, current_type);
					if (current_asset.buffer != data->previous_pointers[current_type][subindex]) {
						// Update the link components
						UpdateAssetToComponents(editor_state, { data->previous_pointers[current_type][subindex], 0 }, current_asset, current_type);
					}
				}
				Deallocate(allocator, data->previous_pointers[current_type]);
			}

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
}

// -----------------------------------------------------------------------------------------------------------------------------

LinkComponentWithAssetFields GetLinkComponentWithAssetFieldForComponent(
	const EditorState* editor_state,
	Component component,
	bool shared,
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search
) {
	LinkComponentWithAssetFields info_type;

	Stream<char> target_type_name = editor_state->editor_components.TypeFromID(component.value, shared);
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
			if (asset_types[asset_index] == asset_fields[index].type) {
				break;
			}
		}

		if (is_material_dependency && asset_fields[index].type == ECS_ASSET_MATERIAL) {
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

void GetLinkComponentsWithAssetFieldsUnique(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	LinkComponentWithAssetFields* link_with_fields, 
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search
)
{
	const EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	entity_manager->ForEachComponent([&](Component component) {
		link_with_fields[component.value] = GetLinkComponentWithAssetFieldForComponent(editor_state, component, false, allocator, asset_types, deep_search);
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetLinkComponentWithAssetFieldsShared(
	const EditorState* editor_state, 
	unsigned int sandbox_index, 
	LinkComponentWithAssetFields* link_with_fields, 
	AllocatorPolymorphic allocator,
	Stream<ECS_ASSET_TYPE> asset_types,
	bool deep_search
)
{
	const EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	entity_manager->ForEachSharedComponent([&](Component component) {
		link_with_fields[component.value] = GetLinkComponentWithAssetFieldForComponent(editor_state, component, true, allocator, asset_types, deep_search);
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

bool IsAssetReferencedInSandbox(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type, unsigned int sandbox_index)
{
	if (sandbox_index == -1) {
		// We just need to return the find value from the main database - assets that have
		// dependencies will draw their dependencies into it as well
		return editor_state->asset_database->FindAsset(name, file, type) != -1;
	}
	else {
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		AssetTypedHandle typed_handle = sandbox->database.FindDeep(name, file, type);
		if (typed_handle.handle != -1) {
			return true;
		}
		return false;
	}
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

		ECS_STACK_CAPACITY_STREAM_DYNAMIC(LinkComponentWithAssetFields, link_with_fields, std::max(unique_component_count, shared_component_count));
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
						component_reflection_types[component_to_visit_count++] = editor_state->editor_components.GetType(editor_state->editor_components.TypeFromID(signature[index], false));
						ECS_ASSERT(component_reflection_types[component_to_visit_count - 1] != nullptr);
					}
				}
			},
			[&](const Archetype* archetype, const ArchetypeBase* base, Entity entity, void** unique_components) {
				ComponentSignature signature = archetype->GetUniqueSignature();
				ECS_STACK_CAPACITY_STREAM(Stream<void>, asset_data, ECS_ARCHETYPE_MAX_COMPONENTS);
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

		_stack_allocator.ClearBackup();
		if (!return_value) {
			GetLinkComponentWithAssetFieldsShared(editor_state, sandbox_index, link_with_fields.buffer, allocator, { &type,1 });

			// Check the shared components as well
			SandboxForAllSharedComponents<true>(editor_state, sandbox_index,
				[&](Component component) {
					component_reflection_types[0] = editor_state->editor_components.GetType(editor_state->editor_components.TypeFromID(component, true));
					ECS_ASSERT(component_reflection_types[0] != nullptr);
				}, 
				[&](Component component, SharedInstance shared_instance) {
					ECS_STACK_CAPACITY_STREAM(Stream<void>, asset_data, ECS_ARCHETYPE_MAX_COMPONENTS);
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
			_stack_allocator.ClearBackup();
		}

		return return_value;
	});

	return return_value;
}

// -----------------------------------------------------------------------------------------------------------------------------

void LoadSandboxMissingAssets(EditorState* editor_state, unsigned int sandbox_index, CapacityStream<unsigned int>* missing_assets)
{
	LoadSandboxMissingAssetsEventData* event_data = InitializeEventData(editor_state, sandbox_index, missing_assets);
	EditorAddEvent(editor_state, LoadSandboxMissingAssetsEvent, event_data, 0);
	// Lock the sandbox as well
	LockSandbox(editor_state, sandbox_index);
}

// -----------------------------------------------------------------------------------------------------------------------------

void LoadSandboxAssets(EditorState* editor_state, unsigned int sandbox_index)
{
	const size_t HANDLES_PER_ASSET_TYPE = ECS_KB;
	ECS_STACK_CAPACITY_STREAM_OF_STREAMS(unsigned int, missing_assets, ECS_ASSET_TYPE_COUNT, HANDLES_PER_ASSET_TYPE);

	GetSandboxMissingAssets(editor_state, sandbox_index, missing_assets.buffer);
	LoadSandboxMissingAssets(editor_state, sandbox_index, missing_assets.buffer);
}

// -----------------------------------------------------------------------------------------------------------------------------

void LoadAssetsWithRemapping(EditorState* editor_state, Stream<Stream<unsigned int>> handles)
{
	LoadSandboxMissingAssetsEventData* event_data = InitializeEventData(editor_state, handles);
	EditorAddEvent(editor_state, LoadSandboxMissingAssetsEvent, event_data, 0);
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

struct ReloadEventData {
	LoadSandboxMissingAssetsEventData* load_data;
	Stream<Stream<unsigned int>> asset_handles;
};

EDITOR_EVENT(ReloadEvent) {
	ReloadEventData* data = (ReloadEventData*)_data;

	if (data->load_data == nullptr) {
		if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
			// Unload the assets
			DeallocateAssetsWithRemapping(editor_state, data->asset_handles, true);

			data->load_data = InitializeEventData(editor_state, data->asset_handles);
			// Can deallocate the asset handles
			editor_state->editor_allocator->Deallocate(data->asset_handles.buffer);
			// Now load the assets again
			return LoadSandboxMissingAssetsEvent(editor_state, data->load_data);
		}
		else {
			return true;
		}
	}
	else {
		// The load sandbox missing assets event will set the resource loading flag
		// Keep calling the event
		return LoadSandboxMissingAssetsEvent(editor_state, data->load_data);
	}
}

void ReloadAssets(EditorState* editor_state, Stream<Stream<unsigned int>> assets_to_reload)
{
	ReloadEventData event_data;
	// Create a temporary resource manager where the assets will be loaded into
	event_data.asset_handles = StreamDeepCopy(assets_to_reload, editor_state->EditorAllocator());
	event_data.load_data = nullptr;
	EditorAddEvent(editor_state, ReloadEvent, &event_data, sizeof(event_data));
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
			unload_elements.Add({ handles[index], asset_fields[index].type });
		}
	}

	UnregisterSandboxAsset(editor_state, sandbox_index, unload_elements);
}

// -----------------------------------------------------------------------------------------------------------------------------

struct UnloadSandboxAssetsEventData {
	unsigned int sandbox_index;
	bool clear_database_reference;
	Stream<unsigned int> asset_mask[ECS_ASSET_TYPE_COUNT];
};

EDITOR_EVENT(UnloadSandboxAssetsEvent) {
	UnloadSandboxAssetsEventData* data = (UnloadSandboxAssetsEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING) && EditorStateHasFlag(editor_state, EDITOR_STATE_RUNTIME_GRAPHICS_INITIALIZATION_FINISHED)) {
		EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_index);
		ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
		GetProjectAssetsFolder(editor_state, assets_folder);

		DeallocateAssetsWithRemapping(editor_state->asset_database, editor_state->runtime_resource_manager, assets_folder, data->asset_mask);

		// Deallocate the streams
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			if (data->asset_mask[index].size > 0) {
				editor_state->editor_allocator->Deallocate(data->asset_mask[index].buffer);
			}
		}

		if (data->clear_database_reference) {
			EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_index);
			sandbox->database.Reset();
		}

		return false;
	}
	else {
		return true;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void UnloadSandboxAssets(EditorState* editor_state, unsigned int sandbox_index, bool clear_database_reference)
{
	UnloadSandboxAssetsEventData event_data;
	event_data.sandbox_index = sandbox_index;
	event_data.clear_database_reference = clear_database_reference;
	EditorAddEvent(editor_state, UnloadSandboxAssetsEvent, &event_data, sizeof(event_data));
	
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

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
	if (sandbox_index == -1) {
		for (unsigned int index = 0; index < editor_state->sandboxes.size; index++) {
			UpdateAssetToComponents(editor_state, old_asset, new_asset, asset_type, index);
		}
		return;
	}

	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 128, ECS_MB);
	AllocatorPolymorphic stack_allocator = GetAllocatorPolymorphic(&_stack_allocator);

	// Firstly go through all unique components, get their reflection types, and asset fields
	Component max_unique_count = entity_manager->GetMaxComponent();
	Component max_shared_count = entity_manager->GetMaxSharedComponent();

	max_unique_count.value = max_unique_count.value == -1 ? 0 : max_unique_count.value;
	max_shared_count.value = max_shared_count.value == -1 ? 0 : max_shared_count.value;

	struct ComponentInfo {
		const Reflection::ReflectionType* type;
		Stream<LinkComponentAssetField> asset_fields;
	};

	CapacityStream<ComponentInfo> unique_types;
	unique_types.Initialize(stack_allocator, 0, max_unique_count.value);

	auto for_each_component = [&](Component component, ComponentInfo* info_type, bool shared) {
		Stream<char> target_type_name = editor_state->editor_components.TypeFromID(component.value, shared);
		ECS_ASSERT(target_type_name.size > 0);

		ECS_STACK_CAPACITY_STREAM(LinkComponentAssetField, asset_fields, 64);

		info_type->type = editor_state->editor_components.GetType(target_type_name);
		GetAssetFieldsFromLinkComponentTarget(info_type->type, asset_fields);

		// Go through the asset fields. If the component doesn't have the given asset type, then don't bother with it
		for (unsigned int index = 0; index < asset_fields.size; index++) {
			if (asset_fields[index].type != asset_type) {
				asset_fields.RemoveSwapBack(index);
				index--;
			}
		}

		info_type->asset_fields.InitializeAndCopy(stack_allocator, asset_fields);
	};

	entity_manager->ForEachComponent([&](Component component) {
		for_each_component(component, &unique_types[component.value], false);
		});

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
					ECS_SET_ASSET_TARGET_FIELD_RESULT result = SetAssetTargetFieldFromReflectionIfMatches(
						unique_types[components_to_check[index]].type,
						asset_fields[subindex].field_index,
						unique_components[components_indices[index]],
						new_asset,
						asset_type,
						old_asset
					);
					ECS_ASSERT(result == ECS_SET_ASSET_TARGET_FIELD_OK || result == ECS_SET_ASSET_TARGET_FIELD_MATCHED);
				}
			}
		}
		);

	// Clear the allocator
	_stack_allocator.Clear();

	// Do the same for shared components
	ComponentInfo shared_info;
	entity_manager->ForEachSharedComponent([&](Component component) {
		for_each_component(component, &shared_info, true);

		if (shared_info.asset_fields.size > 0) {
			entity_manager->ForEachSharedInstance(component, [&](SharedInstance instance) {
				void* instance_data = entity_manager->GetSharedData(component, instance);
				for (size_t index = 0; index < shared_info.asset_fields.size; index++) {
					ECS_SET_ASSET_TARGET_FIELD_RESULT result = SetAssetTargetFieldFromReflectionIfMatches(
						shared_info.type,
						shared_info.asset_fields[index].field_index,
						instance_data,
						new_asset,
						asset_type,
						old_asset
					);
					ECS_ASSERT(result == ECS_SET_ASSET_TARGET_FIELD_OK || result == ECS_SET_ASSET_TARGET_FIELD_MATCHED);
				}
				});
		}
		});

	_stack_allocator.ClearBackup();
}

// -----------------------------------------------------------------------------------------------------------------------------

void UpdateAssetsToComponents(EditorState* editor_state, Stream<UpdateAssetToComponentElement> elements, unsigned int sandbox_index)
{
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

	EntityManager* entity_manager = ActiveEntityManager(editor_state, sandbox_index);
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(_stack_allocator, ECS_KB * 128, ECS_MB);
	AllocatorPolymorphic stack_allocator = GetAllocatorPolymorphic(&_stack_allocator);

	// Firstly go through all unique components, get their reflection types, and asset fields
	Component max_unique_count = entity_manager->GetMaxComponent();
	Component max_shared_count = entity_manager->GetMaxSharedComponent();

	max_unique_count.value = max_unique_count.value == -1 ? 0 : max_unique_count.value;
	max_shared_count.value = max_shared_count.value == -1 ? 0 : max_shared_count.value;

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
						ECS_ASSERT(result == ECS_SET_ASSET_TARGET_FIELD_OK);
					}
				}
			}
		}
	);

	// Clear the allocator
	_stack_allocator.Clear();

	// Do the same for shared components
	entity_manager->ForEachSharedComponent([&](Component component) {
		LinkComponentWithAssetFields link_with_fields = GetLinkComponentWithAssetFieldForComponent(editor_state, component, true, stack_allocator, assets_to_check, false);

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
						ECS_ASSERT(result == ECS_SET_ASSET_TARGET_FIELD_OK);
					}
				}
			});
		}
	});

	_stack_allocator.ClearBackup();
}

// -----------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------
