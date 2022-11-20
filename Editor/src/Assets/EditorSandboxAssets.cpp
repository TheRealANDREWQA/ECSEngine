#include "editorpch.h"
#include "ECSEngineAssets.h"
#include "ECSEngineComponents.h"

#include "../Editor/EditorEvent.h"
#include "../Editor/EditorState.h"
#include "../Editor/EditorSandboxEntityOperations.h"
#include "../Project/ProjectFolders.h"
#include "EditorSandboxAssets.h"
#include "AssetManagement.h"

using namespace ECSEngine;

// Separate the atomic parts accordingly such that we don't have false sharing
struct LoadSandboxMissingAssetsEventData {
	AtomicStream<LoadAssetFailure> failures;
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

	// We need this list of pointers for randomized assets - meshes and materials are allocated
	// When registering materials tho it will use the allocation already made in order to copy the contents even when successful.
	// We also need the list of pointers such that we can update the link components
	void** previous_pointers[ECS_ASSET_TYPE_COUNT];
};

// -----------------------------------------------------------------------------------------------------------------------------

LoadAssetInfo LoadInfoFromEventData(LoadSandboxMissingAssetsEventData* event_data, Stream<wchar_t> mount_point) {
	LoadAssetInfo info;

	info.finish_semaphore = &event_data->semaphore;
	info.load_failures = &event_data->failures;
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
				LoadAssets(&data->database, editor_state->runtime_resource_manager, editor_state->task_manager, &load_info);
			}
			return true;
		}
		else {
			ECS_STACK_CAPACITY_STREAM(char, failure_string, 512);
			unsigned int failure_count = data->failures.size.load(ECS_RELAXED);
			for (unsigned int index = 0; index < failure_count; index++) {
				data->failures[index].ToString(&data->database, failure_string);
				EditorSetConsoleError(failure_string);
				failure_string.size = 0;
			}

			// Deallocate the data and the database and clear the flags
			AllocatorPolymorphic allocator = editor_state->EditorAllocator();

			// Copy the resources to the editor database that were loaded successfully.
			if (failure_count == 0) {
				editor_state->asset_database->CopyAssetPointers(&data->database);
			}
			else {
				// Returns true if the asset has failed, else false
				auto has_failed = [data, failure_count](unsigned int handle, ECS_ASSET_TYPE type) {
					for (unsigned int index = 0; index < failure_count; index++) {
						if (data->failures[index].handle == handle && data->failures[index].asset_type == type) {
							return true;
						}
					}
					return false;
				};

				// Create the asset mask
				ECS_STACK_CAPACITY_STREAM(Stream<unsigned int>, asset_mask, ECS_ASSET_TYPE_COUNT);
				unsigned int total_asset_count = 0;
				for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
					total_asset_count += data->database.GetAssetCount((ECS_ASSET_TYPE)index);
				}

				ECS_STACK_CAPACITY_STREAM_DYNAMIC(unsigned int, asset_mask_storage, total_asset_count - failure_count);
				for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
					ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
					unsigned int current_count = data->database.GetAssetCount(current_type);
					asset_mask[current_type].buffer = asset_mask_storage.buffer + asset_mask_storage.size;
					for (unsigned int subindex = 0; subindex < current_count; subindex++) {
						unsigned int current_handle = data->database.GetAssetHandleFromIndex(subindex, current_type);
						if (!has_failed(current_handle, current_type)) {
							asset_mask[current_type].Add(current_handle);
							// Also update the link component targets
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
						// Deallocate the pointer - from the asset database - if a mesh
						// If multiple loads are launched at the same time, an asset can get
						// deallocated multiple times, so use a belong deallocation
						if (current_type == ECS_ASSET_MESH) {
							Deallocate(editor_state->asset_database->Allocator(), data->previous_pointers[current_type][subindex]);
						}
						// And we also need to update the link components
						UpdateAssetToComponents(editor_state, { data->previous_pointers[current_type][subindex], 0 }, current_asset, current_type);
					}
				}
				Deallocate(allocator, data->previous_pointers[current_type]);
			}

			data->database.DeallocateStreams();
			Deallocate(allocator, data->failures.buffer);
			Deallocate(allocator, data);

			EditorStateClearFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH);
			EditorStateClearFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);

			return false;
		}
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

const size_t MAX_FAILURE_COUNT = ECS_KB * 16;

LoadSandboxMissingAssetsEventData* InitializeEventDataBase(EditorState* editor_state) {
	AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();

	LoadSandboxMissingAssetsEventData* event_data = (LoadSandboxMissingAssetsEventData*)Allocate(editor_allocator, sizeof(LoadSandboxMissingAssetsEventData));

	event_data->failures.Initialize(editor_allocator, MAX_FAILURE_COUNT);
	event_data->success = true;
	event_data->semaphore.ClearCount();
	event_data->has_launched = false;

	return event_data;
}

LoadSandboxMissingAssetsEventData* InitializeEventData(EditorState* editor_state, CapacityStream<unsigned int>* missing_assets) {
	LoadSandboxMissingAssetsEventData* data = InitializeEventDataBase(editor_state);
	data->database = editor_state->asset_database->Copy(missing_assets, editor_state->EditorAllocator());
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

void LoadSandboxMissingAssets(EditorState* editor_state, unsigned int sandbox_index, CapacityStream<unsigned int>* missing_assets)
{
	LoadSandboxMissingAssetsEventData* event_data = InitializeEventData(editor_state, missing_assets);
	EditorAddEvent(editor_state, LoadSandboxMissingAssetsEvent, event_data, 0);
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

struct RegisterAssetTaskData {
	EditorState* editor_state;
	unsigned int handle;
	ECS_ASSET_TYPE asset_type;

	UIActionHandler callback;
};

ECS_THREAD_TASK(RegisterAssetTask) {
	RegisterAssetTaskData* data = (RegisterAssetTaskData*)_data;

	bool success = CreateAsset(data->editor_state, data->handle, data->asset_type);
	if (!success) {
		const char* type_string = ConvertAssetTypeString(data->asset_type);
		Stream<char> asset_name = data->editor_state->asset_database->GetAssetName(data->handle, data->asset_type);
		ECS_FORMAT_TEMP_STRING(error_message, "Failed to create asset {#}, type {#}. Possible causes: invalid path or the processing failed.", asset_name, type_string);
		EditorSetConsoleError(error_message);
	}

	if (data->callback.action != nullptr) {
		ActionData dummy_data;
		dummy_data.data = data->callback.data;
		dummy_data.additional_data = &data->handle;
		dummy_data.system = nullptr;
		data->callback.action(&dummy_data);

		// Deallocate the data if it has the size > 0
		if (data->callback.data_size > 0) {
			data->editor_state->editor_allocator->Deallocate(data->callback.data);
		}
	}

	// Release the editor state flags
	EditorStateClearFlag(data->editor_state, EDITOR_STATE_PREVENT_LAUNCH);
	EditorStateClearFlag(data->editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);
}

struct RegisterEventData {
	unsigned int* handle;
	ECS_ASSET_TYPE type;
	bool unregister_if_exists;
	unsigned int name_size;
	unsigned int file_size;
	unsigned int sandbox_index;
	UIActionHandler callback;
};

EDITOR_EVENT(RegisterEvent) {
	RegisterEventData* data = (RegisterEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		if (data->unregister_if_exists) {
			unsigned int current_handle = *data->handle;
			if (current_handle != -1) {
				UnregisterSandboxAsset(editor_state, data->sandbox_index, current_handle, data->type);
			}
		}

		unsigned int data_sizes[2] = {
			data->name_size,
			data->file_size
		};

		EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_index);
		Stream<char> name = function::GetCoallescedStreamFromType(data, 0, data_sizes).AsIs<char>();
		Stream<wchar_t> file = function::GetCoallescedStreamFromType(data, 1, data_sizes).AsIs<wchar_t>();
		
		bool loaded_now = false;
		unsigned int handle = sandbox->database.AddAsset(name, file, data->type, &loaded_now);
		*data->handle = handle;

		if (handle == -1) {
			// Send a warning
			ECS_FORMAT_TEMP_STRING(warning, "Failed to load asset {#} metadata, type {#}.", name, ConvertAssetTypeString(data->type));
			EditorSetConsoleWarn(warning);

			// We still have to call the callback
			if (data->callback.action != nullptr) {
				ActionData dummy_data;
				dummy_data.data = data->callback.data;
				dummy_data.additional_data = &handle;
				dummy_data.system = nullptr;
				data->callback.action(&dummy_data);

				// Deallocate the data, if it has the size > 0
				if (data->callback.data_size > 0) {
					editor_state->editor_allocator->Deallocate(data->callback.data);
				}
			}

			return false;
		}

		if (loaded_now) {
			// Launch a background task - block the resource manager first
			EditorStateSetFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);
			EditorStateSetFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH);

			RegisterAssetTaskData task_data;
			task_data.asset_type = data->type;
			task_data.editor_state = editor_state;
			task_data.handle = handle;
			task_data.callback = data->callback;
			EditorStateAddBackgroundTask(editor_state, ECS_THREAD_TASK_NAME(RegisterAssetTask, &task_data, sizeof(task_data)));
		}
		return false;
	}
	else {
		return true;
	}
}

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
	ECS_ASSERT(name.size > 0);
	if (AssetHasFile(type)) {
		ECS_ASSERT(file.size > 0);
	}

	unsigned int existing_handle = FindAsset(editor_state, name, file, type);
	if (existing_handle == -1) {
		size_t storage[128];
		unsigned int write_size = 0;
		Stream<void> streams[] = {
			name,
			file
		};
		RegisterEventData* data = function::CreateCoallescedStreamsIntoType<RegisterEventData>(storage, { streams, std::size(streams) }, &write_size);
		data->handle = handle;
		data->sandbox_index = sandbox_index;
		data->type = type;
		data->name_size = name.size;
		data->file_size = file.size;
		data->unregister_if_exists = unregister_if_exists;
		data->callback = callback;
		data->callback.data = function::CopyNonZero(editor_state->editor_allocator, data->callback.data, data->callback.data_size);

		EditorAddEvent(editor_state, RegisterEvent, data, write_size);
		return false;
	}
	else {
		unsigned int previous_handle = *handle;

		GetSandbox(editor_state, sandbox_index)->database.AddAsset(existing_handle, type, true);
		*handle = existing_handle;

		if (unregister_if_exists) {
			if (previous_handle != -1) {
				UnregisterSandboxAsset(editor_state, sandbox_index, previous_handle, type);
			}
		}

		if (callback.action != nullptr) {
			ActionData dummy_data;
			dummy_data.data = callback.data;
			dummy_data.additional_data = &existing_handle;
			dummy_data.system = nullptr;
			callback.action(&dummy_data);
		}
		return true;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

struct UnregisterEventData {
	unsigned int sandbox_index;
	Stream<UnregisterSandboxAssetElement> elements;
	UIActionHandler callback;
};

EDITOR_EVENT(UnregisterEvent) {
	UnregisterEventData* data = (UnregisterEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		ActionData dummy_data;
		dummy_data.system = nullptr;

		for (size_t index = 0; index < data->elements.size; index++) {
			unsigned int handle = data->elements[index].handle;
			ECS_ASSET_TYPE type = data->elements[index].type;

			Stream<char> asset_name = editor_state->asset_database->GetAssetName(handle, type);
			Stream<wchar_t> file = editor_state->asset_database->GetAssetPath(handle, type);

			size_t metadata_storage[ECS_KB];

			auto fail = [type, asset_name, file](Stream<char> tail_message) {
				const char* type_string = ConvertAssetTypeString(type);
				ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
				if (file.size > 0) {
					ECS_FORMAT_STRING(error_message, "Failed to remove asset {#} with settings {#}, type {#}. {#}", file, asset_name, type_string, tail_message);
				}
				else {
					ECS_FORMAT_STRING(error_message, "Failed to remove asset {#}, type {#}. {#}", asset_name, type_string, tail_message);
				}
				EditorSetConsoleError(error_message);
			};

			EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_index);
			unsigned int database_reference_index = sandbox->database.GetIndex(handle, type);
			if (database_reference_index == -1) {
				__debugbreak();
				ECS_FORMAT_TEMP_STRING(tail_message, "It doesn't exist in the sandbox {#} asset database reference.", data->sandbox_index);
				fail(tail_message);
			}
			
			// Call the callback before actually removing the asset
			if (data->callback.action != nullptr) {
				dummy_data.data = data->callback.data;
				dummy_data.additional_data = &handle;
				data->callback.action(&dummy_data);
			}

			bool removed_now = sandbox->database.RemoveAsset(database_reference_index, type, metadata_storage);

			if (removed_now) {
				bool success = DeallocateAsset(editor_state, metadata_storage, type);
				if (!success) {
					fail("Possible causes: internal error or the asset was not loaded in the first place.");
				}
			}
		}

		// Deallocate the buffer of data
		editor_state->editor_allocator->Deallocate(data->elements.buffer);
		return false;
	}
	else {
		return true;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_index, unsigned int handle, ECS_ASSET_TYPE type, UIActionHandler callback)
{
	UnregisterSandboxAssetElement element = { handle, type };
	UnregisterSandboxAsset(editor_state, sandbox_index, { &element, 1 }, callback);
}

// -----------------------------------------------------------------------------------------------------------------------------

void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_index, Stream<UnregisterSandboxAssetElement> elements, UIActionHandler callback)
{
	// No elements to unload
	if (elements.size == 0) {
		return;
	}

	UnregisterEventData event_data = { sandbox_index, { nullptr, 0}, callback };
	UnregisterEventData* allocated_data = (UnregisterEventData*)EditorAddEvent(editor_state, UnregisterEvent, &event_data, sizeof(event_data));
	allocated_data->elements.InitializeAndCopy(editor_state->EditorAllocator(), elements);
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
	Stream<unsigned int> asset_mask[ECS_ASSET_TYPE_COUNT];
};

EDITOR_EVENT(UnloadSandboxAssetsEvent) {
	UnloadSandboxAssetsEventData* data = (UnloadSandboxAssetsEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING) && EditorStateHasFlag(editor_state, EDITOR_STATE_RUNTIME_GRAPHICS_INITIALIZATION_FINISHED)) {
		EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_index);
		ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
		GetProjectAssetsFolder(editor_state, assets_folder);

		UnloadAssets(&sandbox->database, editor_state->runtime_resource_manager, assets_folder, data->asset_mask);

		// Deallocate the streams
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			if (data->asset_mask[index].size > 0) {
				editor_state->editor_allocator->Deallocate(data->asset_mask[index].buffer);
			}
		}

		return false;
	}
	else {
		return true;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

void UnloadSandboxAssets(EditorState* editor_state, unsigned int sandbox_index)
{
	UnloadSandboxAssetsEventData event_data;
	event_data.sandbox_index = sandbox_index;
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
					ECS_ASSERT(result == ECS_SET_ASSET_TARGET_FIELD_OK);
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
					ECS_ASSERT(result == ECS_SET_ASSET_TARGET_FIELD_OK);
				}
				});
		}
		});

	_stack_allocator.ClearBackup();
}

// -----------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------
