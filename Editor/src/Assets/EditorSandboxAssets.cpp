#include "editorpch.h"
#include "ECSEngineAssets.h"
#include "ECSEngineComponents.h"

#include "../Editor/EditorEvent.h"
#include "../Editor/EditorState.h"
#include "../Project/ProjectFolders.h"
#include "EditorSandboxAssets.h"
#include "AssetManagement.h"

using namespace ECSEngine;

// Separate the atomic parts accordingly such that we don't have false sharing
struct CheckEventData {
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
};

EDITOR_EVENT(CheckLoadFinish) {
	CheckEventData* data = (CheckEventData*)_data;

	if (data->semaphore.count.load(ECS_RELAXED) != 0) {
		// It is not finished yet. Push the event again
		return true;
	}
	else {
		// Deallocate the data and the database and clear the flags
		AllocatorPolymorphic allocator = editor_state->EditorAllocator();
		data->database.DeallocateStreams();
		Deallocate(allocator, data->failures.buffer);
		Deallocate(allocator, data);

		EditorStateClearFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH);
		EditorStateClearFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);
		return false;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

const size_t MAX_FAILURE_COUNT = ECS_KB * 16;

CheckEventData* InitializeEventDataBase(EditorState* editor_state) {
	AllocatorPolymorphic editor_allocator = editor_state->EditorAllocator();

	CheckEventData* event_data = (CheckEventData*)Allocate(editor_allocator, sizeof(CheckEventData));

	event_data->failures.Initialize(editor_allocator, MAX_FAILURE_COUNT);
	event_data->success = true;
	event_data->semaphore.ClearCount();

	return event_data;
}

CheckEventData* InitializeEventData(EditorState* editor_state, unsigned int sandbox_index) {
	CheckEventData* data = InitializeEventDataBase(editor_state);
	const AssetDatabaseReference* reference = &GetSandbox(editor_state, sandbox_index)->database;
	reference->ToStandalone(editor_state->EditorAllocator(), &data->database);
	return data;
}

CheckEventData* InitializeEventData(EditorState* editor_state, CapacityStream<unsigned int>* missing_assets) {
	CheckEventData* data = InitializeEventDataBase(editor_state);
	data->database = editor_state->asset_database->Copy(missing_assets, editor_state->EditorAllocator());
	return data;
}

// -----------------------------------------------------------------------------------------------------------------------------

LoadAssetInfo LoadInfoFromEventData(CheckEventData* event_data, Stream<wchar_t> mount_point) {
	LoadAssetInfo info;

	info.finish_semaphore = &event_data->semaphore;
	info.load_failures = &event_data->failures;
	info.mount_point = mount_point;
	info.success = &event_data->success;

	return info;
}

// -----------------------------------------------------------------------------------------------------------------------------

void GetSandboxMissingAssets(const EditorState* editor_state, unsigned int sandbox_index, CapacityStream<unsigned int>* missing_assets)
{
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	const AssetDatabaseReference* reference = &sandbox->database;
	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);

	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(database_allocator, ECS_KB * 128, ECS_MB * 8);
	AllocatorPolymorphic allocator = GetAllocatorPolymorphic(&database_allocator);

	AssetDatabase standalone_database;
	reference->ToStandalone(allocator, &standalone_database);
	GetDatabaseMissingAssets(editor_state->runtime_resource_manager, &standalone_database, missing_assets, assets_folder);

	database_allocator.ClearBackup();
}

// -----------------------------------------------------------------------------------------------------------------------------

template<typename EventDataFunctor>
void LoadSandboxMissingAssetsImpl(EditorState* editor_state, unsigned int sandbox_index, EventDataFunctor&& functor) {
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

	const AssetDatabaseReference* reference = &sandbox->database;
	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);

	CheckEventData* event_data = functor();
	LoadAssetInfo load_info = LoadInfoFromEventData(event_data, assets_folder);

	LoadAssets(&event_data->database, editor_state->runtime_resource_manager, editor_state->task_manager, &load_info);

	EditorAddEvent(editor_state, CheckLoadFinish, event_data, 0);

	// Lock the start of the runtime and the load of resources
	EditorStateSetFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH);
	EditorStateSetFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);
}

void LoadSandboxMissingAssets(EditorState* editor_state, unsigned int sandbox_index, CapacityStream<unsigned int>* missing_assets)
{
	LoadSandboxMissingAssetsImpl(editor_state, sandbox_index, [&]() {
		return InitializeEventData(editor_state, missing_assets);
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

void LoadSandboxAssets(EditorState* editor_state, unsigned int sandbox_index)
{
	LoadSandboxMissingAssetsImpl(editor_state, sandbox_index, [&]() {
		return InitializeEventData(editor_state, sandbox_index);
	});
}

// -----------------------------------------------------------------------------------------------------------------------------

struct RegisterEventData {
	unsigned int* handle;
	ECS_ASSET_TYPE type;
	unsigned int name_size;
	unsigned int file_size;
	unsigned int sandbox_index;
};

EDITOR_EVENT(RegisterEvent) {
	RegisterEventData* data = (RegisterEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
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
			// Set a warning
			ECS_FORMAT_TEMP_STRING(warning, "Failed to load asset {#} metadata, type {#}.", name, ConvertAssetTypeString(data->type));
			EditorSetConsoleWarn(warning);
			return false;
		}

		if (loaded_now) {
			bool success = CreateAsset(editor_state, handle, data->type);
			if (!success) {
				const char* type_string = ConvertAssetTypeString(data->type);
				ECS_FORMAT_TEMP_STRING(error_message, "Failed to create asset {#}, type {#}. Possible causes: invalid path or the processing failed.", name, type_string);
				EditorSetConsoleError(error_message);
			}
		}
		return false;
	}
	else {
		return true;
	}
}

// The registration needs to be moved into an event in case the resource manager is locked
bool RegisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_index, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type, unsigned int* handle)
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

		EditorAddEvent(editor_state, RegisterEvent, data, write_size);
		return false;
	}
	else {
		*handle = existing_handle;
		return true;
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

struct UnregisterEventData {
	unsigned int sandbox_index;
	Stream<UnregisterSandboxAssetElement> elements;
};

EDITOR_EVENT(UnregisterEvent) {
	UnregisterEventData* data = (UnregisterEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		for (size_t index = 0; index < data->elements.size; index++) {
			unsigned int handle = data->elements[index].handle;
			ECS_ASSET_TYPE type = data->elements[index].type;

			Stream<char> asset_name = editor_state->asset_database->GetAssetName(handle, type);

			EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_index);
			unsigned int database_reference_index = sandbox->database.GetIndex(handle, type);
			bool removed_now = sandbox->database.RemoveAsset(database_reference_index, type);

			if (removed_now) {
				bool success = DeallocateAsset(editor_state, handle, type);
				if (!success) {
					const char* type_string = ConvertAssetTypeString(type);
					ECS_FORMAT_TEMP_STRING(error_message, "Failed to remove asset {#}, type {#}. Possible causes: internal error or the asset was not loaded in the first place.", asset_name, type_string);
					EditorSetConsoleError(error_message);
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

void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_index, unsigned int handle, ECS_ASSET_TYPE type)
{
	UnregisterSandboxAssetElement element = { handle, type };
	UnregisterSandboxAsset(editor_state, sandbox_index, { &element, 1 });
}

// -----------------------------------------------------------------------------------------------------------------------------

void UnregisterSandboxAsset(EditorState* editor_state, unsigned int sandbox_index, Stream<UnregisterSandboxAssetElement> elements)
{
	// No elements to unload
	if (elements.size == 0) {
		return;
	}

	UnregisterEventData event_data = { sandbox_index, { nullptr, 0} };
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

// -----------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------
