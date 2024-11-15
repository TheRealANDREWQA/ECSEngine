#include "editorpch.h"
#include "AssetManagement.h"
#include "../Editor/EditorState.h"
#include "../Project/ProjectFolders.h"
#include "../Editor/EditorEvent.h"
#include "../Sandbox/Sandbox.h"
#include "EditorSandboxAssets.h"
#include "AssetExtensions.h"

using namespace ECSEngine;

// ----------------------------------------------------------------------------------------------

struct CreateAssetSingleThreadedCallbackEventData {
	UIActionHandler callback;
	ECS_ASSET_TYPE asset_type;
	bool success;
	bool should_release_flags_and_unlock_sandbox;
	unsigned int handle;
	unsigned int previous_handle;
	unsigned int sandbox_index;
};

static EDITOR_EVENT(CreateAssetSingleThreadedCallbackEvent) {
	CreateAssetSingleThreadedCallbackEventData* data = (CreateAssetSingleThreadedCallbackEventData*)_data;
	
	RegisterAssetEventCallbackInfo info;
	info.handle = data->handle;
	info.success = data->success;
	info.type = data->asset_type;
	info.previous_handle = data->previous_handle;

	ActionData dummy_data;
	dummy_data.data = data->callback.data;
	dummy_data.additional_data = &info;
	dummy_data.system = editor_state->ui_system;
	data->callback.action(&dummy_data);

	// Deallocate the data if it has the size > 0
	if (data->callback.data_size > 0) {
		Deallocate(editor_state->MultithreadedEditorAllocator(), data->callback.data);
	}

	// Can release the flags and unlock the sandbox
	if (data->should_release_flags_and_unlock_sandbox) {
		EditorStateClearFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH);
		EditorStateClearFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);

		if (data->sandbox_index != -1) {
			UnlockSandbox(editor_state, data->sandbox_index);
		}
	}

	return false;
}

struct CreateAssetAsyncTaskData {
	EditorState* editor_state;
	ECS_ASSET_TYPE asset_type;
	bool callback_is_single_threaded = true;
	unsigned int handle;
	// This value is used only to pass to the callback
	unsigned int previous_handle;
	// The sandbox index is needed to unlock the sandbox at the end
	// It can be -1
	unsigned int sandbox_index = -1;

	UIActionHandler callback = {};
};

static ECS_THREAD_TASK(CreateAssetAsyncTask) {
	CreateAssetAsyncTaskData* data = (CreateAssetAsyncTaskData*)_data;
	EditorState* editor_state = data->editor_state;

	// We need to acquire the resource lock and possibly the GPU lock
	AssetLock(editor_state, data->asset_type);

	// Elevate the frame pacing to make this a little bit more snappy
	editor_state->ElevateFramePacing(ECS_UI_FRAME_PACING_MEDIUM);

	bool success = false;
	__try {
		success = CreateAsset(editor_state, data->handle, data->asset_type);
		if (!success) {
			const char* type_string = ConvertAssetTypeString(data->asset_type);
			Stream<char> asset_name = editor_state->asset_database->GetAssetName(data->handle, data->asset_type);
			bool is_valid = ValidateAssetMetadataOptions(editor_state->asset_database->GetAssetConst(data->handle, data->asset_type), data->asset_type);
			if (is_valid) {
				ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
				FormatString(error_message, "Failed to create asset {#}, type {#}. Possible causes: invalid path or the processing failed.", asset_name, type_string);
				EditorSetConsoleError(error_message);
			}

			// We should still try to create its dependencies that were loaded for the first time
			CreateAssetInternalDependencies(editor_state, data->handle, data->asset_type);
		}
	}
	__finally {
		AssetUnlock(editor_state, data->asset_type);
	}

	bool callback_is_single_threaded = false;
	if (data->callback.action != nullptr) {
		CreateAssetSingleThreadedCallbackEventData call_callback_data;
		call_callback_data.asset_type = data->asset_type;
		call_callback_data.callback = data->callback;
		call_callback_data.handle = data->handle;
		call_callback_data.previous_handle = data->previous_handle;
		call_callback_data.sandbox_index = data->sandbox_index;
		call_callback_data.should_release_flags_and_unlock_sandbox = false;
		call_callback_data.success = success;

		if (data->callback_is_single_threaded) {
			callback_is_single_threaded = true;
			call_callback_data.should_release_flags_and_unlock_sandbox = true;
			EditorAddEvent(editor_state, CreateAssetSingleThreadedCallbackEvent, &call_callback_data, sizeof(call_callback_data));
		} 
		else {
			CreateAssetSingleThreadedCallbackEvent(editor_state, &call_callback_data);
		}
	}

	editor_state->DeelevateFramePacing();

	if (!callback_is_single_threaded) {
		// Release the editor state flags - only if the callback was not called
		EditorStateClearFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH);
		EditorStateClearFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);

		if (data->sandbox_index != -1) {
			UnlockSandbox(editor_state, data->sandbox_index);
		}
	}

	// Add an event to remove the loading asset entry
	AssetTypedHandle typed_handle = { data->handle, data->asset_type };
	RegisterRemoveLoadingAssetsEvent(editor_state, Stream<AssetTypedHandle>(&typed_handle, 1));
}

struct CreateAssetAsyncEventData {
	unsigned int handle;
	ECS_ASSET_TYPE type;
	bool callback_is_single_threaded;
	UIActionHandler callback = {};
};

static EDITOR_EVENT(CreateAssetAsyncEvent) {
	CreateAssetAsyncEventData* data = (CreateAssetAsyncEventData*)_data;

	if (EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		return true;
	}

	// Launch a background task - block the resource manager first
	EditorStateSetFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);
	EditorStateSetFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH);

	CreateAssetAsyncTaskData task_data;
	task_data.asset_type = data->type;
	task_data.editor_state = editor_state;
	task_data.handle = data->handle;
	task_data.sandbox_index = -1;
	task_data.callback = data->callback;
	task_data.callback_is_single_threaded = data->callback_is_single_threaded;
	EditorStateAddBackgroundTask(editor_state, ECS_THREAD_TASK_NAME(CreateAssetAsyncTask, &task_data, sizeof(task_data)));
	return false;
}

struct RegisterEventData {
	unsigned int* handle;
	ECS_ASSET_TYPE type;
	bool unregister_if_exists;
	bool callback_is_single_threaded;
	unsigned int name_size;
	unsigned int file_size;
	// If the sandbox index is -1, then it will registered as a global asset
	unsigned int sandbox_index;
	UIActionHandler callback;
};

EDITOR_EVENT(RegisterEvent) {
	RegisterEventData* data = (RegisterEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		unsigned int previous_handle = *data->handle;
		if (data->unregister_if_exists) {;
			if (previous_handle != -1) {
				AssetTypedHandle element;
				element.handle = previous_handle;
				element.type = data->type;
				AddUnregisterAssetEvent(editor_state, { &element, 1 }, data->sandbox_index != -1, data->sandbox_index);
			}
		}

		unsigned int data_sizes[2] = {
			data->name_size,
			data->file_size
		};

		Stream<char> name = GetCoalescedStreamFromType(data, 0, data_sizes).AsIs<char>();
		Stream<wchar_t> file = GetCoalescedStreamFromType(data, 1, data_sizes).AsIs<wchar_t>();
		bool loaded_now = false;

		unsigned int handle = -1;
		if (data->sandbox_index != -1) {
			EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_index);
			handle = sandbox->database.AddAsset(name, file, data->type, &loaded_now);
		}
		else {
			handle = editor_state->asset_database->AddAsset(name, file, data->type, &loaded_now);
		}
		*data->handle = handle;

		if (handle == -1) {
			// Send a warning
			ECS_FORMAT_TEMP_STRING(warning, "Failed to load asset {#} metadata, type {#}.", name, ConvertAssetTypeString(data->type));
			EditorSetConsoleWarn(warning);

			// We still have to call the callback
			if (data->callback.action != nullptr) {
				RegisterAssetEventCallbackInfo info;
				info.handle = handle;
				info.type = data->type;
				info.success = false;
				info.previous_handle = previous_handle;

				ActionData dummy_data;
				dummy_data.data = data->callback.data;
				dummy_data.additional_data = &info;
				dummy_data.system = editor_state->ui_system;
				data->callback.action(&dummy_data);

				// Deallocate the data, if it has the size > 0
				if (data->callback.data_size > 0) {
					Deallocate(editor_state->MultithreadedEditorAllocator(), data->callback.data);
				}
			}

			// Unlock the sandbox
			if (data->sandbox_index != -1) {
				UnlockSandbox(editor_state, data->sandbox_index);
			}
			return false;
		}

		if (loaded_now) {
			// We still have the lock on the sandbox here - the Async task will release it

			// Add the asset to the loading array
			AssetTypedHandle loading_asset = { handle, data->type };
			AddLoadingAssets(editor_state, Stream<AssetTypedHandle>(&loading_asset, 1));

			// Launch a background task - block the resource manager first
			EditorStateSetFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);
			EditorStateSetFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH);

			CreateAssetAsyncTaskData task_data;
			task_data.asset_type = data->type;
			task_data.editor_state = editor_state;
			task_data.handle = handle;
			task_data.callback = data->callback;
			task_data.sandbox_index = data->sandbox_index;
			task_data.previous_handle = previous_handle;
			task_data.callback_is_single_threaded = data->callback_is_single_threaded;
			EditorStateAddBackgroundTask(editor_state, ECS_THREAD_TASK_NAME(CreateAssetAsyncTask, &task_data, sizeof(task_data)));
		}
		return false;
	}
	else {
		return true;
	}
}

// ----------------------------------------------------------------------------------------------

struct UnregisterEventDataBase {
	AssetDatabaseReference* database_reference;
	unsigned int sandbox_index;
	bool sandbox_assets;
	UIActionHandler callback;
};

struct UnregisterEventData {
	UnregisterEventDataBase base_data;
	Stream<AssetTypedHandle> elements;
};

struct UnregisterEventHomogeneousData {
	UnregisterEventDataBase base_data;
	ECS_ASSET_TYPE type;
	Stream<unsigned int> elements;
};

template<typename Extractor>
EDITOR_EVENT(UnregisterAssetEventImpl) {
	UnregisterEventDataBase* data = (UnregisterEventDataBase*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		auto fail = [](Stream<char> asset_name, Stream<wchar_t> file, ECS_ASSET_TYPE type, Stream<char> tail_message) {
			const char* type_string = ConvertAssetTypeString(type);
			ECS_STACK_CAPACITY_STREAM(char, error_message, 512);
			if (file.size > 0) {
				FormatString(error_message, "Failed to remove asset {#} with settings {#}, type {#}. {#}", file, asset_name, type_string, tail_message);
			}
			else {
				FormatString(error_message, "Failed to remove asset {#}, type {#}. {#}", asset_name, type_string, tail_message);
			}
			EditorSetConsoleError(error_message);
		};

		auto loop = [=](unsigned int sandbox_index) {
			ActionData dummy_data;
			dummy_data.system = editor_state->ui_system;

			size_t count = Extractor::Count(data);

			for (size_t index = 0; index < count; index++) {
				unsigned int handle = Extractor::Handle(data, index);
				if (handle != -1) {
					ECS_ASSET_TYPE type = Extractor::Type(data, index);

					Stream<char> asset_name = editor_state->asset_database->GetAssetName(handle, type);
					Stream<wchar_t> file = editor_state->asset_database->GetAssetPath(handle, type);

					// Call the callback before actually removing the asset
					if (data->callback.action != nullptr) {
						UnregisterAssetEventCallbackInfo info;
						info.handle = handle;
						info.type = type;

						dummy_data.data = data->callback.data;
						dummy_data.additional_data = &info;
						data->callback.action(&dummy_data);
					}

					AssetDatabaseReference* database_reference = sandbox_index == -1 ? data->database_reference : &GetSandbox(editor_state, sandbox_index)->database;
					bool success = DecrementAssetReference(editor_state, handle, type, database_reference);
					if (!success) {
						fail(asset_name, file, type, "Possible causes: internal error or the asset was not loaded in the first place.");
					}
				}
			}
		};

		if (data->sandbox_assets) {
			SandboxAction(editor_state, data->sandbox_index, [&](unsigned int sandbox_index) {
				loop(sandbox_index);
			});
		}
		else {
			//ActionData dummy_data;
			//dummy_data.system = editor_state->ui_system;

			//size_t count = Extractor::Count(data);
			//for (size_t index = 0; index < count; index++) {
			//	unsigned int handle = Extractor::Handle(data, index);
			//	ECS_ASSET_TYPE type = Extractor::Type(data, index);

			//	Stream<char> asset_name = editor_state->asset_database->GetAssetName(handle, type);
			//	Stream<wchar_t> file = editor_state->asset_database->GetAssetPath(handle, type);

			//	size_t metadata_storage[ECS_KB];

			//	// Call the callback before actually removing the asset
			//	if (data->callback.action != nullptr) {
			//		UnregisterAssetEventCallbackInfo info;
			//		info.handle = handle;
			//		info.type = type;

			//		dummy_data.data = data->callback.data;
			//		dummy_data.additional_data = &info;
			//		data->callback.action(&dummy_data);
			//	}

			//	AssetDatabaseRemoveInfo remove_info;
			//	remove_info.storage = metadata_storage;
			//	bool removed_now = editor_state->asset_database->RemoveAsset(handle, type, &remove_info);

			//	if (removed_now) {
			//		bool success = RemoveAsset(editor_state, metadata_storage, type);
			//		if (!success) {
			//			fail(asset_name, file, type, "Possible causes: internal error or the asset was not loaded in the first place.");
			//		}
			//	}
			//}

			// This will remove the asset without belonging to a certain sandbox
			loop(-1);
		}

		void* element_buffer = Extractor::Data(data);
		// Deallocate the buffer of data
		editor_state->editor_allocator->Deallocate(element_buffer);

		if (data->sandbox_assets) {
			// Unlock the sandbox
			SandboxAction(editor_state, data->sandbox_index, [=](unsigned int sandbox_index) {
				UnlockSandbox(editor_state, sandbox_index);
			});
		}

		if (data->callback.data_size > 0) {
			editor_state->editor_allocator->Deallocate(data->callback.data);
		}
		return false;
	}
	else {
		return true;
	}
}

EDITOR_EVENT(UnregisterAssetEvent) {
	struct Extractor {
		ECS_INLINE static unsigned int Handle(void* _data, size_t index) {
			UnregisterEventData* data = (UnregisterEventData*)_data;
			return data->elements[index].handle;
		}

		ECS_INLINE static ECS_ASSET_TYPE Type(void* _data, size_t index) {
			UnregisterEventData* data = (UnregisterEventData*)_data;
			return data->elements[index].type;
		}

		ECS_INLINE static size_t Count(void* _data) {
			UnregisterEventData* data = (UnregisterEventData*)_data;
			return data->elements.size;
		}

		ECS_INLINE static void* Data(void* _data) {
			UnregisterEventData* data = (UnregisterEventData*)_data;
			return data->elements.buffer;
		}
	};
	return UnregisterAssetEventImpl<Extractor>(editor_state, _data);
}

EDITOR_EVENT(UnregisterAssetEventHomogeneous) {
	struct Extractor {
		ECS_INLINE static unsigned int Handle(void* _data, size_t index) {
			UnregisterEventHomogeneousData* data = (UnregisterEventHomogeneousData*)_data;
			return data->elements[index];
		}

		ECS_INLINE static ECS_ASSET_TYPE Type(void* _data, size_t index) {
			UnregisterEventHomogeneousData* data = (UnregisterEventHomogeneousData*)_data;
			return data->type;
		}

		ECS_INLINE static size_t Count(void* _data) {
			UnregisterEventData* data = (UnregisterEventData*)_data;
			return data->elements.size;
		}

		ECS_INLINE static void* Data(void* _data) {
			UnregisterEventHomogeneousData* data = (UnregisterEventHomogeneousData*)_data;
			return data->elements.buffer;
		}
	};
	return UnregisterAssetEventImpl<Extractor>(editor_state, _data);
}

// ----------------------------------------------------------------------------------------------

Stream<wchar_t> AssetMetadataTimeStampPath(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, CapacityStream<wchar_t>& storage) {
	editor_state->asset_database->FileLocationAsset(GetAssetName(metadata, type), GetAssetFile(metadata, type), storage, type);
	return storage;
}

// ----------------------------------------------------------------------------------------------

Stream<wchar_t> AssetTargetFileTimeStampPath(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, CapacityStream<wchar_t>& storage) {
	Stream<wchar_t> path = { nullptr, 0 };

	switch (type) {
	case ECS_ASSET_MESH:
	case ECS_ASSET_TEXTURE:
	case ECS_ASSET_SHADER:
	case ECS_ASSET_MISC:
	{
		ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
		GetProjectAssetsFolder(editor_state, assets_folder);
		path = MountPathOnlyRel(GetAssetFile(metadata, type), assets_folder, storage);
	}
	break;
	case ECS_ASSET_GPU_SAMPLER:
	case ECS_ASSET_MATERIAL:
	{
		// There is no external target path
		path = { nullptr, 0 };
	}
	break;
	default:
		ECS_ASSERT(false, "Invalid asset type");
	}

	return path;
}

// ----------------------------------------------------------------------------------------------

Stream<wchar_t> AssetTargetFileTimeStampPath(
	const void* metadata, 
	ECS_ASSET_TYPE type, 
	Stream<wchar_t> assets_folder, 
	CapacityStream<wchar_t>& storage
) {
	Stream<wchar_t> path = { nullptr, 0 };

	switch (type) {
	case ECS_ASSET_MESH:
	case ECS_ASSET_TEXTURE:
	case ECS_ASSET_SHADER:
	case ECS_ASSET_MISC:
	{
		path = MountPathOnlyRel(GetAssetFile(metadata, type), assets_folder, storage);
	}
	break;
	case ECS_ASSET_GPU_SAMPLER:
	case ECS_ASSET_MATERIAL:
	{
		// There is no external target path
		path = { nullptr, 0 };
	}
	break;
	default:
		ECS_ASSERT(false, "Invalid asset type");
	}

	return path;
}

// ----------------------------------------------------------------------------------------------

void AddLoadingAssets(EditorState* editor_state, Stream<Stream<unsigned int>> handles) {
	ECS_ASSERT(handles.size == ECS_ASSET_TYPE_COUNT);
	for (size_t asset_type = 0; asset_type < ECS_ASSET_TYPE_COUNT; asset_type++) {
		for (unsigned int index = 0; index < handles[asset_type].size; index++) {
			editor_state->loading_assets.Add({ handles[asset_type][index], (ECS_ASSET_TYPE)asset_type });
		}
	}
}

void AddLoadingAssets(EditorState* editor_state, CapacityStream<unsigned int>* handles) {
	for (size_t asset_type = 0; asset_type < ECS_ASSET_TYPE_COUNT; asset_type++) {
		for (unsigned int index = 0; index < handles[asset_type].size; index++) {
			editor_state->loading_assets.Add({ handles[asset_type][index], (ECS_ASSET_TYPE)asset_type });
		}
	}
}

void AddLoadingAssets(EditorState* editor_state, Stream<AssetTypedHandle> handles) {
	editor_state->loading_assets.AddStream(handles);
}

// ----------------------------------------------------------------------------------------------

void AddUnregisterAssetEvent(
	EditorState* editor_state,
	Stream<AssetTypedHandle> elements,
	bool sandbox_assets,
	unsigned int sandbox_index,
	UIActionHandler callback
)
{
	if (elements.size == 0) {
		return;
	}

	callback.data = CopyNonZero(editor_state->EditorAllocator(), callback.data, callback.data_size);
	UnregisterEventData event_data = { { nullptr, sandbox_index, sandbox_assets, callback } };
	event_data.elements.InitializeAndCopy(editor_state->EditorAllocator(), elements);
	EditorAddEvent(editor_state, UnregisterAssetEvent, &event_data, sizeof(event_data));

	if (sandbox_assets) {
		SandboxAction(editor_state, sandbox_index, [=](unsigned int sandbox_index) {
			LockSandbox(editor_state, sandbox_index);
		});
	}
}

// ----------------------------------------------------------------------------------------------

void AddUnregisterAssetEvent(EditorState* editor_state, Stream<AssetTypedHandle> elements, AssetDatabaseReference* database_reference, UIActionHandler callback)
{
	if (elements.size == 0) {
		return;
	}

	callback.data = CopyNonZero(editor_state->EditorAllocator(), callback.data, callback.data_size);
	UnregisterEventData event_data = { { database_reference, -1, false, callback } };
	event_data.elements.InitializeAndCopy(editor_state->EditorAllocator(), elements);
	EditorAddEvent(editor_state, UnregisterAssetEvent, &event_data, sizeof(event_data));
}

// ----------------------------------------------------------------------------------------------

void AddUnregisterAssetEventHomogeneous(
	EditorState* editor_state,
	Stream<Stream<unsigned int>> elements,
	bool sandbox_assets,
	unsigned int sandbox_index,
	UIActionHandler callback
)
{
	for (size_t index = 0; index < elements.size; index++) {
		if (elements[index].size > 0) {
			UnregisterEventHomogeneousData event_data = { { nullptr, sandbox_index, sandbox_assets, callback }, (ECS_ASSET_TYPE)index };
			event_data.elements.InitializeAndCopy(editor_state->EditorAllocator(), elements[index]);
			EditorAddEvent(editor_state, UnregisterAssetEventHomogeneous, &event_data, sizeof(event_data));

			if (sandbox_assets) {
				SandboxAction(editor_state, sandbox_index, [=](unsigned int sandbox_index) {
					// Lock the sandbox
					LockSandbox(editor_state, sandbox_index);
				});
			}
		}
	}
}

// ----------------------------------------------------------------------------------------------

void AddUnregisterAssetEventHomogeneous(
	EditorState* editor_state,
	Stream<Stream<unsigned int>> elements,
	AssetDatabaseReference* database_reference,
	UIActionHandler callback
)
{
	for (size_t index = 0; index < elements.size; index++) {
		if (elements[index].size > 0) {
			UnregisterEventHomogeneousData event_data = { { database_reference, -1, false, callback }, (ECS_ASSET_TYPE)index };
			event_data.elements.InitializeAndCopy(editor_state->EditorAllocator(), elements[index]);
			EditorAddEvent(editor_state, UnregisterAssetEventHomogeneous, &event_data, sizeof(event_data));
		}
	}
}

// ----------------------------------------------------------------------------------------------

bool AddRegisterAssetEvent(
	EditorState* editor_state,
	Stream<char> name,
	Stream<wchar_t> file,
	ECS_ASSET_TYPE type,
	unsigned int* handle,
	unsigned int sandbox_index,
	bool unload_if_existing,
	UIActionHandler callback,
	bool callback_is_single_threaded
)
{
	ECS_ASSERT(name.size > 0);
	if (AssetHasFile(type)) {
		ECS_ASSERT(file.size > 0);
	}

	unsigned int existing_handle = FindAsset(editor_state, name, file, type);
	if (existing_handle == -1) {
		ECS_STACK_VOID_STREAM(stack_storage, ECS_KB);
		unsigned int write_size = 0;
		Stream<void> streams[] = {
			name,
			file
		};
		RegisterEventData* data = CreateCoalescedStreamsIntoType<RegisterEventData>(stack_storage, { streams, std::size(streams) }, &write_size);
		data->handle = handle;
		data->sandbox_index = sandbox_index;
		data->type = type;
		data->name_size = name.size;
		data->file_size = file.size;
		data->unregister_if_exists = unload_if_existing;
		data->callback = callback;
		// Use the multithreaded allocator such that this can be deallocated from the async task, if the task can run asynchronously
		data->callback.data = CopyNonZero(editor_state->MultithreadedEditorAllocator(), data->callback.data, data->callback.data_size);
		data->callback_is_single_threaded = callback_is_single_threaded;

		EditorAddEvent(editor_state, RegisterEvent, data, write_size);
		if (sandbox_index != -1) {
			// Lock the sandbox
			LockSandbox(editor_state, sandbox_index);
		}
		return false;
	}
	else {
		unsigned int previous_handle = *handle;

		if (sandbox_index != -1) {
			GetSandbox(editor_state, sandbox_index)->database.AddAsset(existing_handle, type, true);
		}
		else {
			editor_state->asset_database->AddAsset(existing_handle, type);
		}
		*handle = existing_handle;

		if (unload_if_existing) {
			if (previous_handle != -1) {
				AssetTypedHandle element;
				element.handle = previous_handle;
				element.type = type;
				AddUnregisterAssetEvent(editor_state, { &element, 1 }, sandbox_index != -1, sandbox_index);
			}
		}

		if (callback.action != nullptr) {
			RegisterAssetEventCallbackInfo info;
			info.handle = existing_handle;
			info.type = type;
			info.success = true;
			info.previous_handle = previous_handle;

			ActionData dummy_data;
			dummy_data.data = callback.data;
			dummy_data.additional_data = &info;
			dummy_data.system = editor_state->ui_system;
			callback.action(&dummy_data);
		}
		return true;
	}
}

// ----------------------------------------------------------------------------------------------

void AssetLock(EditorState* editor_state, ECS_ASSET_TYPE type) {
	// Acquire the locks in this order, the resource manager and then the graphics
	editor_state->RuntimeResourceManager()->Lock();
	AssetTypeLockGPU(type, &editor_state->frame_gpu_lock);
}

void AssetUnlock(EditorState* editor_state, ECS_ASSET_TYPE type) {
	// Release the locks in the reverse order.
	AssetTypeUnlockGPU(type, &editor_state->frame_gpu_lock);
	editor_state->RuntimeResourceManager()->Unlock();
}

// ----------------------------------------------------------------------------------------------

static bool CreateAssetFileImpl(const EditorState* editor_state, Stream<wchar_t> relative_path, Stream<wchar_t> extension) {
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	GetProjectAssetsFolder(editor_state, absolute_path);
	absolute_path.Add(ECS_OS_PATH_SEPARATOR);
	absolute_path.AddStream(relative_path);
	absolute_path.AddStreamSafe(extension);

	return CreateEmptyFile(absolute_path);
}

// ----------------------------------------------------------------------------------------------

// Create it as an empty file. It will exist in the metadata folder. This just makes the connection with it
bool CreateMaterialFile(const EditorState* editor_state, Stream<wchar_t> relative_path)
{
	return CreateAssetFileImpl(editor_state, relative_path, ASSET_MATERIAL_EXTENSIONS[0]);
}

// ----------------------------------------------------------------------------------------------

// Create it as an empty file. It will exist in the metadata folder. This just makes the connection with it
bool CreateSamplerFile(const EditorState* editor_state, Stream<wchar_t> relative_path)
{
	return CreateAssetFileImpl(editor_state, relative_path, ASSET_GPU_SAMPLER_EXTENSIONS[0]);
}

// ----------------------------------------------------------------------------------------------

bool CreateShaderFile(const EditorState* editor_state, Stream<wchar_t> relative_path, ECS_SHADER_TYPE shader_type)
{
	Stream<wchar_t> extension = {};
	if (shader_type != ECS_SHADER_TYPE_COUNT) {
		extension = AssetExtensionFromShaderType(shader_type);
	}
	return CreateAssetFileImpl(editor_state, relative_path, extension);
}

// ----------------------------------------------------------------------------------------------

bool CreateMiscFile(const EditorState* editor_state, Stream<wchar_t> relative_path)
{
	return CreateAssetFileImpl(editor_state, relative_path, ASSET_MISC_EXTENSIONS[0]);
}

// ----------------------------------------------------------------------------------------------

bool CreateAssetSetting(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type)
{
	// Just create the default file
	size_t storage[AssetMetadataMaxSizetSize()];

	bool has_file = AssetHasFile(type);
	if (has_file) {
		CreateDefaultAsset(storage, name, file, type);
	}
	else {
		// Convert the file to a name - since it will be ignored
		ECS_STACK_CAPACITY_STREAM(char, converted_file, 512);
		//file = PathStem(file);
		ConvertWideCharsToASCII(file, converted_file);
		CreateDefaultAsset(storage, converted_file, file, type);
	}

	return editor_state->asset_database->WriteAssetFile(storage, type);
}

// ----------------------------------------------------------------------------------------------

bool CreateShaderSetting(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_SHADER_TYPE shader_type)
{
	ShaderMetadata storage;
	storage.Default(name, file);
	storage.shader_type = shader_type;

	return editor_state->asset_database->WriteShaderFile(&storage);
}

// ----------------------------------------------------------------------------------------------

bool CreateAsset(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type)
{
	// If the target file is valid, then also create the asset
	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);

	void* metadata = editor_state->asset_database->GetAsset(handle, type);
	bool success = CreateAssetFromMetadata(
		editor_state->RuntimeResourceManager(),
		editor_state->asset_database,
		metadata,
		type,
		assets_folder
	);

	// Insert the time stamp if it doesn't already exist, even if it fails
	InsertAssetTimeStamp(editor_state, metadata, type, true);

	// Get its dependencies. If they don't already don't have a time stamp, we need to create time stamps for them as well
	ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 512);
	GetAssetDependencies(metadata, type, &dependencies);
	for (unsigned int index = 0; index < dependencies.size; index++) {
		InsertAssetTimeStamp(editor_state, dependencies[index].handle, dependencies[index].type, true);
	}
	return success;
}

// ----------------------------------------------------------------------------------------------

void CreateAssetAsync(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, UIActionHandler callback, bool callback_is_single_threaded)
{
	callback.data = CopyNonZero(editor_state->MultithreadedEditorAllocator(), callback.data, callback.data_size);
	
	// Add the asset to the loading array
	AssetTypedHandle typed_handle = { handle, type };
	AddLoadingAssets(editor_state, Stream<AssetTypedHandle>(&typed_handle, 1));

	CreateAssetAsyncEventData event_data;
	event_data.handle = handle;
	event_data.type = type;
	event_data.callback = callback;
	event_data.callback_is_single_threaded = callback_is_single_threaded;
	EditorAddEvent(editor_state, CreateAssetAsyncEvent, &event_data, sizeof(event_data));
}

// ----------------------------------------------------------------------------------------------

bool CreateAssetInternalDependencies(
	EditorState* editor_state, 
	unsigned int handle, 
	ECS_ASSET_TYPE type, 
	CapacityStream<CreateAssetInternalDependenciesElement>* dependency_elements
)
{
	const void* metadata = editor_state->asset_database->GetAssetConst(handle, type);
	return CreateAssetInternalDependencies(editor_state, metadata, type, dependency_elements);
}

// ----------------------------------------------------------------------------------------------

bool CreateAssetInternalDependencies(
	EditorState* editor_state, 
	const void* metadata, 
	ECS_ASSET_TYPE type, 
	CapacityStream<CreateAssetInternalDependenciesElement>* dependency_elements
)
{
	bool success = true;

	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);

	ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, internal_dependencies, 512);
	GetAssetDependencies(metadata, type, &internal_dependencies);

	for (unsigned int index = 0; index < internal_dependencies.size; index++) {
		const void* current_dependency = editor_state->asset_database->GetAssetConst(internal_dependencies[index].handle, internal_dependencies[index].type);
		Stream<void> previous_pointer = GetAssetFromMetadata(current_dependency, internal_dependencies[index].type);

		bool current_success = true;
		if (!IsAssetPointerFromMetadataValid(previous_pointer) || !IsAssetFromMetadataLoaded(
			editor_state->RuntimeResourceManager(), 
			current_dependency, 
			internal_dependencies[index].type, 
			assets_folder,
			true)
		) {
			current_success = CreateAsset(editor_state, internal_dependencies[index].handle, internal_dependencies[index].type);
			success &= !current_success;
		}
		Stream<void> new_pointer = GetAssetFromMetadata(current_dependency, internal_dependencies[index].type);

		if (dependency_elements != nullptr) {
			dependency_elements->AddAssert({ internal_dependencies[index].handle, internal_dependencies[index].type, current_success, previous_pointer, new_pointer });
		}
	}

	return success;
}

// ----------------------------------------------------------------------------------------------

// The functor receives as arguments (Stream<Stream<wchar_t>>* extensions, ResizableStream<Stream<wchar_t>>* existing_files)
// which correspond to the asset types in the mapping
// For materials, gpu samplers, shaders and misc files it returns the file path without extension
template<typename ExecuteFunctor>
static void ForEachAssetMetadata(const EditorState* editor_state, Stream<ECS_ASSET_TYPE> asset_type_mapping, bool retrieve_existing_files, ExecuteFunctor&& functor) {
	// At the moment only meshes and textures need this.
	ECS_STACK_CAPACITY_STREAM(Stream<Stream<wchar_t>>, extensions, ECS_ASSET_TYPE_COUNT);

	for (size_t index = 0; index < asset_type_mapping.size; index++) {
		extensions[index] = ASSET_EXTENSIONS[asset_type_mapping[index]];
	}

	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 128, ECS_MB);
	AllocatorPolymorphic allocator = &stack_allocator;

	ECS_STACK_CAPACITY_STREAM(ResizableStream<Stream<wchar_t>>, files, ECS_ASSET_TYPE_COUNT);
	if (retrieve_existing_files) {
		for (size_t index = 0; index < asset_type_mapping.size; index++) {
			files[index].Initialize(allocator, 0);
		}

		struct FunctorData {
			ECS_ASSET_TYPE asset_type;
			ResizableStream<Stream<wchar_t>>* files;
		};

		auto get_functor = [](Stream<wchar_t> path, void* _data) {
			FunctorData* data = (FunctorData*)_data;

			ECS_STACK_CAPACITY_STREAM(wchar_t, current_file, 512);
			Stream<wchar_t> target_file_relative = AssetDatabase::ExtractAssetTargetFromFile(path, current_file);

			if (target_file_relative.size > 0) {
				unsigned int index = FindString(target_file_relative, data->files->ToStream());
				if (index == -1) {
					Stream<wchar_t> copy = StringCopy(data->files->allocator, current_file);
					// Change the relative path separator into an absolute path separator
					ReplaceCharacter(copy, ECS_OS_PATH_SEPARATOR_REL, ECS_OS_PATH_SEPARATOR);
					data->files->Add(copy);
				}
			}
			return true;
		};

		ECS_STACK_CAPACITY_STREAM(wchar_t, metadata_directory, 512);
		for (size_t index = 0; index < asset_type_mapping.size; index++) {
			metadata_directory.size = 0;
			AssetDatabaseFileDirectory(editor_state->asset_database->metadata_file_location, metadata_directory, asset_type_mapping[index]);
			// It has a final backslash
			metadata_directory.size--;

			FunctorData functor_data;
			functor_data.asset_type = asset_type_mapping[index];
			functor_data.files = &files[index];
			ForEachFileInDirectory(metadata_directory, &functor_data, get_functor);
		}
	}

	functor(extensions.buffer, files.buffer);
}

void CreateAssetDefaultSetting(const EditorState* editor_state)
{
	ECS_ASSET_TYPE mapping[] = {
		ECS_ASSET_MESH,
		ECS_ASSET_TEXTURE,
		ECS_ASSET_GPU_SAMPLER,
		ECS_ASSET_MATERIAL,
		ECS_ASSET_MISC
	};

	ForEachAssetMetadata(editor_state, { mapping, std::size(mapping) }, true, [&](Stream<Stream<wchar_t>>* extensions, ResizableStream<Stream<wchar_t>>* existing_files) {
		struct FunctorData {
			const EditorState* editor_state;
			Stream<wchar_t> base_path;
			Stream<Stream<Stream<wchar_t>>> extensions;
			Stream<Stream<wchar_t>> existing_files[_countof(mapping)];
			ECS_ASSET_TYPE* mapping;
		};

		ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
		GetProjectAssetsFolder(editor_state, assets_folder);

		FunctorData functor_data;
		functor_data.editor_state = editor_state;
		functor_data.base_path = assets_folder;
		functor_data.extensions = { extensions, _countof(mapping) };
		functor_data.mapping = mapping;
		for (size_t index = 0; index < std::size(mapping); index++) {
			functor_data.existing_files[index] = existing_files[index];
		}

		auto functor = [](Stream<wchar_t> path, void* _data) {
			FunctorData* data = (FunctorData*)_data;

			Stream<wchar_t> extension = PathExtension(path);
			for (size_t index = 0; index < _countof(mapping); index++) {
				unsigned int extension_index = FindString(extension, data->extensions[index]);
				if (extension_index != -1) {
					Stream<wchar_t> relative_path = PathRelativeToAbsolute(path, data->base_path);
					ECS_ASSET_TYPE asset_type = data->mapping[index];
					unsigned int existing_index = FindString(relative_path, data->existing_files[index]);
					if (existing_index == -1) {
						// Create a default setting for it
						// Don't check for failure - this is not a critical operation
						CreateAssetSetting(data->editor_state, "Default", relative_path, asset_type);
					}
					break;
				}
			}

			return true;
		};

		ForEachFileInDirectoryRecursive(assets_folder, &functor_data, functor);
	});
}

// ----------------------------------------------------------------------------------------------

void ChangeAssetTimeStamp(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, size_t new_stamp)
{
	ChangeAssetTimeStamp(editor_state, editor_state->asset_database->GetAssetConst(handle, type), type, new_stamp);
}

// ----------------------------------------------------------------------------------------------

void ChangeAssetTimeStamp(EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, size_t new_stamp)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, storage, 512);

	Stream<wchar_t> metadata_file = AssetMetadataTimeStampPath(editor_state, metadata, type, storage);
	editor_state->RuntimeResourceManager()->ChangeTimeStamp(metadata_file, ResourceType::TimeStamp, new_stamp);
}

// ----------------------------------------------------------------------------------------------

bool DeallocateAsset(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, bool recurse, CapacityStream<DeallocateAssetDependency>* dependent_assets)
{
	return DeallocateAsset(editor_state, editor_state->asset_database->GetAsset(handle, type), type, recurse, dependent_assets);
}

// ----------------------------------------------------------------------------------------------

bool DeallocateAsset(EditorState* editor_state, void* metadata, ECS_ASSET_TYPE type, bool recurse, CapacityStream<DeallocateAssetDependency>* dependent_assets)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);

	unsigned int previous_dependent_assets_size = 0;
	if (dependent_assets != nullptr) {
		Stream<void> current_asset = GetAssetFromMetadata(metadata, type);
		unsigned int current_handle = editor_state->asset_database->FindAssetEx(current_asset, type);
		previous_dependent_assets_size = dependent_assets->Add({ current_handle, type, current_asset, { nullptr, 0 } });
	}

	DeallocateAssetFromMetadataOptions options;
	options.material_check_resource = true;
	bool success = DeallocateAssetFromMetadata(
		editor_state->RuntimeResourceManager(),
		editor_state->asset_database,
		metadata,
		type,
		assets_folder,
		options
	);
	if (success) {
		if (recurse) {
			// Deallocate its dependent assets before randomizing the pointer
			ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
			Stream<Stream<unsigned int>> current_dependent_assets = editor_state->asset_database->GetDependentAssetsFor(metadata, type, &stack_allocator);
			for (size_t index = 0; index < current_dependent_assets.size; index++) {
				ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
				for (size_t subindex = 0; subindex < current_dependent_assets[index].size; subindex++) {
					void* current_metadata = editor_state->asset_database->GetAsset(current_dependent_assets[index][subindex], current_type);
					Stream<void> previous_data = GetAssetFromMetadata(current_metadata, current_type);
					bool recurse_success = DeallocateAsset(editor_state, current_metadata, current_type, true, dependent_assets);
					success &= recurse_success;
					//if (recurse_success && dependent_assets != nullptr) {
						//dependent_assets->AddAssert({ current_dependent_assets[index][subindex], current_type, previous_data, GetAssetFromMetadata(current_metadata, current_type) });
					//}
				}
			}
		}

		// Randomize the asset
		editor_state->asset_database->RandomizePointer(metadata, type);

		if (dependent_assets != nullptr) {
			dependent_assets->buffer[previous_dependent_assets_size].new_pointer = GetAssetFromMetadata(metadata, type);
		}
	}
	return success;
}

// ----------------------------------------------------------------------------------------------

struct DeleteAssetSettingEventData {
	unsigned int name_size;
	unsigned int file_size;
	ECS_ASSET_TYPE type;
};

EDITOR_EVENT(DeleteAssetSettingEvent) {
	DeleteAssetSettingEventData* data = (DeleteAssetSettingEventData*)_data;

	if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		unsigned int sizes[] = {
			data->name_size,
			data->file_size * sizeof(wchar_t)
		};

		Stream<char> name = GetCoalescedStreamFromType(data, 0, sizes).As<char>();
		Stream<wchar_t> file = GetCoalescedStreamFromType(data, 1, sizes).As<wchar_t>();

		unsigned int handle = editor_state->asset_database->FindAsset(name, file, data->type);
		if (handle != -1) {
			// Deallocate the asset from the resource manager before removing from the asset database
			bool success = DeallocateAsset(editor_state, handle, data->type);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(error_message, "Failed to deallocate asset {#}, type {#}.", name, ConvertAssetTypeString(data->type));
				EditorSetConsoleError(error_message);
			}

			// Remove it from the asset database
			editor_state->asset_database->RemoveAssetForced(handle, data->type);

			// Remove it from the sandbox references
			SandboxAction(editor_state, -1, [&](unsigned int index) {
				EditorSandbox* sandbox = GetSandbox(editor_state, index);
				unsigned int reference_index = sandbox->database.GetIndex(handle, data->type);
				sandbox->database.RemoveAssetThisOnly(reference_index, data->type);
			});
		}

		// Now delete its disk file
		ECS_STACK_CAPACITY_STREAM(wchar_t, file_path, 512);
		editor_state->asset_database->FileLocationAsset(name, file, file_path, data->type);
		if (ExistsFileOrFolder(file_path)) {
			bool success = RemoveFile(file_path);
			if (!success) {
				ECS_FORMAT_TEMP_STRING(error_message, "Failed to delete metadata file {#}.", file_path);
				EditorSetConsoleError(error_message);
			}
		}

		return false;
	}
	else {
		return true;
	}
}

void DeleteAssetSetting(EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type)
{
	ECS_STACK_VOID_STREAM(stack_storage, ECS_KB * 2);
	unsigned int write_size = 0;
	Stream<void> buffers[] = {
		name,
		file
	};

	DeleteAssetSettingEventData* data = CreateCoalescedStreamsIntoType<DeleteAssetSettingEventData>(stack_storage, { buffers, std::size(buffers) }, &write_size);
	data->name_size = name.size;
	data->file_size = file.size;
	data->type = type;
	EditorAddEvent(editor_state, DeleteAssetSettingEvent, data, write_size);
}

// ----------------------------------------------------------------------------------------------

void DeleteMissingAssetSettings(const EditorState* editor_state)
{
	ECS_ASSET_TYPE mapping[] = {
		ECS_ASSET_MESH,
		ECS_ASSET_TEXTURE,
		ECS_ASSET_GPU_SAMPLER,
		ECS_ASSET_SHADER,
		ECS_ASSET_MATERIAL,
		ECS_ASSET_MISC
	};

	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);
	assets_folder.Add(ECS_OS_PATH_SEPARATOR);

	struct FunctorData {
		Stream<Stream<wchar_t>> extensions;
		CapacityStream<wchar_t>* assets_folder;
	};

	auto functor = [](Stream<wchar_t> path, void* _data) {
		FunctorData* data = (FunctorData*)_data;

		ECS_STACK_CAPACITY_STREAM(wchar_t, current_file, 512);
		Stream<wchar_t> target_file = AssetDatabase::ExtractAssetTargetFromFile(path, *data->assets_folder, current_file);

		if (target_file.size > 0) {
			bool exists = ExistsFileOrFolder(target_file);
			if (!exists) {
				// Destroy the current setting
				RemoveFile(path);
			}
		}

		return true;
	};

	ECS_STACK_CAPACITY_STREAM(wchar_t, metadata_directory, 512);
	for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
		metadata_directory.size = 0;
		AssetDatabaseFileDirectory(editor_state->asset_database->metadata_file_location, metadata_directory, (ECS_ASSET_TYPE)index);
		// It has a final backslash
		metadata_directory.size--;

		FunctorData functor_data;
		functor_data.assets_folder = &assets_folder;
		functor_data.extensions = ASSET_EXTENSIONS[index];
		ForEachFileInDirectory(metadata_directory, &functor_data, functor);
	}
}

// ----------------------------------------------------------------------------------------------

bool DecrementAssetReference(
	EditorState* editor_state, 
	unsigned int handle, 
	ECS_ASSET_TYPE type, 
	unsigned int sandbox_index, 
	unsigned int decrement_count,
	bool* was_removed
)
{
	AssetDatabaseReference* database_reference = sandbox_index != -1 ? &GetSandbox(editor_state, sandbox_index)->database : nullptr;
	return DecrementAssetReference(editor_state, handle, type, database_reference, decrement_count, was_removed);
}

// ----------------------------------------------------------------------------------------------

bool DecrementAssetReference(
	EditorState* editor_state, 
	unsigned int handle, 
	ECS_ASSET_TYPE type, 
	AssetDatabaseReference* database_reference, 
	unsigned int decrement_count,
	bool* was_removed
)
{
	auto remove_from_reference = [=]() {
		if (database_reference != nullptr) {
			for (unsigned int index = 0; index < decrement_count; index++) {
				unsigned int reference_index = database_reference->GetIndex(handle, type);
				if (reference_index != -1) {
					database_reference->RemoveAssetThisOnly(reference_index, type);
				}
			}
		}
	};

	unsigned int reference_count = editor_state->asset_database->GetReferenceCount(handle, type);
	if (reference_count <= decrement_count) {
		bool success = RemoveAsset(editor_state, handle, type);
		if (success) {
			remove_from_reference();
		}
		if (was_removed != nullptr) {
			*was_removed = true;
		}
		return success;
	}
	else {
		for (unsigned int index = 0; index < decrement_count; index++) {
			editor_state->asset_database->RemoveAsset(handle, type);
		}
		remove_from_reference();
		if (was_removed != nullptr) {
			*was_removed = false;
		}
		return true;
	}
}

// ----------------------------------------------------------------------------------------------

bool ExistsAsset(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type)
{
	return FindAsset(editor_state, name, file, type) != -1;
}

// ----------------------------------------------------------------------------------------------

bool ExistsAssetInResourceManager(const EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type)
{
	const void* metadata = editor_state->asset_database->GetAssetConst(handle, type);
	return ExistsAssetInResourceManager(editor_state, metadata, type);
}

// ----------------------------------------------------------------------------------------------

bool ExistsAssetInResourceManager(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);
	return IsAssetFromMetadataLoaded(editor_state->runtime_resource_manager, metadata, type, assets_folder);
}

// ----------------------------------------------------------------------------------------------

void EvictAssetFromDatabase(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type)
{
	// Remove the time stamp from the resource manager as well
	RemoveAssetTimeStamp(editor_state, handle, type);
	editor_state->asset_database->RemoveAssetForced(handle, type);
}

// ----------------------------------------------------------------------------------------------

Stream<Stream<unsigned int>> FilterUnloadedAssets(const EditorState* editor_state, Stream<Stream<unsigned int>> handles)
{
	ECS_ASSERT(handles.size == ECS_ASSET_TYPE_COUNT);
	for (size_t index = 0; index < handles.size; index++) {
		ECS_ASSET_TYPE asset_type = (ECS_ASSET_TYPE)index;
		for (size_t subindex = 0; subindex < handles[index].size; subindex++) {
			if (ExistsAssetInResourceManager(editor_state, handles[index][subindex], asset_type)) {
				handles[index].RemoveSwapBack(subindex);
				subindex--;
			}
		}
	}
	return handles;
}

// ----------------------------------------------------------------------------------------------

unsigned int FindAsset(const EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type) {
	return editor_state->asset_database->FindAsset(name, file, type);
}

// ----------------------------------------------------------------------------------------------

unsigned int FindOrAddAsset(EditorState* editor_state, Stream<char> name, Stream<wchar_t> file, ECS_ASSET_TYPE type, bool increment_reference_count)
{
	unsigned int existing_asset = FindAsset(editor_state, name, file, type);
	if (existing_asset != -1) {
		if (increment_reference_count) {
			editor_state->asset_database->AddAsset(existing_asset, type);
		}
		return existing_asset;
	}
	else {
		// Insert the asset
		return editor_state->asset_database->AddAsset(name, file, type);
	}
}

// ----------------------------------------------------------------------------------------------

void FromAssetNameToThunkOrForwardingFile(Stream<char> name, Stream<wchar_t> extension, CapacityStream<wchar_t>& relative_path)
{
	ConvertASCIIToWide(relative_path, name);
	// Replace relative slashes with absolute ones
	ReplaceCharacter(relative_path, ECS_OS_PATH_SEPARATOR_REL, ECS_OS_PATH_SEPARATOR);
	relative_path.AddStreamSafe(extension);
}

// ----------------------------------------------------------------------------------------------

void FromAssetNameToThunkOrForwardingFile(Stream<char> name, ECS_ASSET_TYPE type, CapacityStream<wchar_t>& relative_path)
{
	Stream<wchar_t> extension = { nullptr, 0 };
	
	switch (type) {
	case ECS_ASSET_GPU_SAMPLER:
		extension = ASSET_GPU_SAMPLER_EXTENSIONS[0];
		break;
	case ECS_ASSET_SHADER:
		extension = ASSET_SHADER_EXTENSIONS[0];
		break;
	case ECS_ASSET_MATERIAL:
		extension = ASSET_MATERIAL_EXTENSIONS[0];
		break;
	case ECS_ASSET_MISC:
		extension = ASSET_MISC_EXTENSIONS[0];
		break;
	default:
		ECS_ASSERT(false, "Invalid asset type.");
	}

	FromAssetNameToThunkOrForwardingFile(name, extension, relative_path);
}

// ----------------------------------------------------------------------------------------------

void GetAssetNameFromThunkOrForwardingFile(const EditorState* editor_state, Stream<wchar_t> absolute_path, CapacityStream<char>& name)
{
	Stream<wchar_t> relative_path = GetProjectAssetRelativePath(editor_state, absolute_path);
	ECS_ASSERT(relative_path.size > 0);
	GetAssetNameFromThunkOrForwardingFileRelative(editor_state, relative_path, name);
}

// ----------------------------------------------------------------------------------------------

void GetAssetNameFromThunkOrForwardingFileRelative(const EditorState* editor_state, Stream<wchar_t> relative_path, CapacityStream<char>& name)
{
	ConvertWideCharsToASCII(relative_path, name);
	// Replace absolute separators with relative ones
	ReplaceCharacter(name, ECS_OS_PATH_SEPARATOR_ASCII, ECS_OS_PATH_SEPARATOR_ASCII_REL);
}

// ----------------------------------------------------------------------------------------------

bool GetAssetFileFromForwardingFile(Stream<wchar_t> absolute_path, CapacityStream<wchar_t>& target_path)
{
	const size_t MAX_SIZE = ECS_KB * 32;
	char buffer[MAX_SIZE];

	ECS_FILE_HANDLE file_handle = 0;
	ECS_FILE_STATUS_FLAGS status = OpenFile(absolute_path, &file_handle, ECS_FILE_ACCESS_READ_BINARY_SEQUENTIAL);
	if (status != ECS_FILE_STATUS_OK) {
		return false;
	}

	size_t file_byte_size = GetFileByteSize(file_handle);

	ScopedFile scoped_file{ { file_handle } };

	if (file_byte_size == 0) {
		return true;
	}

	if (file_byte_size > MAX_SIZE) {
		return false;
	}

	unsigned int read_size = ReadFromFile(file_handle, { buffer, ECS_KB * 32 });
	if (read_size == -1) {
		return false;
	}
	if (read_size > target_path.capacity - target_path.size) {
		// Fail if the file is too big
		return false;
	}
	target_path.AddStream({ buffer, read_size / sizeof(wchar_t) });
	return true;
}

// ----------------------------------------------------------------------------------------------

bool GetAssetFileFromAssetMetadata(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, CapacityStream<wchar_t>& path, bool absolute_path)
{
	Stream<wchar_t> target_file = GetAssetFile(metadata, type);
	Stream<char> name = GetAssetName(metadata, type);

	if (absolute_path) {
		GetProjectAssetsFolder(editor_state, path);
		path.Add(ECS_OS_PATH_SEPARATOR);
	}

	switch (type) {
		// Meshes and textures are handled the same
	case ECS_ASSET_MESH:
	case ECS_ASSET_TEXTURE:
		path.AddStreamSafe(target_file);
		ReplaceCharacter(path, ECS_OS_PATH_SEPARATOR_REL, ECS_OS_PATH_SEPARATOR);

		break;
		// All the other assets have thunk or forwarding file
	case ECS_ASSET_GPU_SAMPLER:
	case ECS_ASSET_MATERIAL:
	case ECS_ASSET_SHADER:
	case ECS_ASSET_MISC:
	{
		ConvertASCIIToWide(path, name);
		ReplaceCharacter(path, ECS_OS_PATH_SEPARATOR_REL, ECS_OS_PATH_SEPARATOR);
	}
		break;
	default:
		ECS_ASSERT(false, "Invalid asset type");
	}

	return true;
}

// ----------------------------------------------------------------------------------------------

bool GetAssetFileFromAssetMetadata(const EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, CapacityStream<wchar_t>& path, bool absolute_path)
{
	return GetAssetFileFromAssetMetadata(editor_state, editor_state->asset_database->GetAssetConst(handle, type), type, path, absolute_path);
}

// ----------------------------------------------------------------------------------------------

Stream<Stream<char>> GetAssetCorrespondingMetadata(const EditorState* editor_state, Stream<wchar_t> file, ECS_ASSET_TYPE type, AllocatorPolymorphic allocator)
{
	return editor_state->asset_database->GetMetadatasForFile(file, type, allocator);
}

// ----------------------------------------------------------------------------------------------

enum GetOutOfDateAssetsType {
	GET_OUT_OF_DATE_ASSETS_BOTH,
	GET_OUT_OF_DATE_ASSETS_METADATA,
	GET_OUT_OF_DATE_ASSETS_TARGET
};

template<GetOutOfDateAssetsType stamp_type>
Stream<Stream<unsigned int>> GetOutOfDateAssetsImpl(EditorState* editor_state, AllocatorPolymorphic allocator, bool update_stamp, bool include_dependencies) {
	Stream<Stream<unsigned int>> handles;
	handles.Initialize(allocator, ECS_ASSET_TYPE_COUNT);

	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	if constexpr (stamp_type == GET_OUT_OF_DATE_ASSETS_BOTH || stamp_type == GET_OUT_OF_DATE_ASSETS_TARGET) {
		GetProjectAssetsFolder(editor_state, assets_folder);
	}

	ECS_STACK_CAPACITY_STREAM(unsigned int, out_of_date_temporary, ECS_KB * 8);
	for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
		ECS_ASSET_TYPE current_type = (ECS_ASSET_TYPE)index;
		unsigned int asset_count = editor_state->asset_database->GetAssetCount(current_type);
		handles[current_type].Initialize(allocator, asset_count);
		handles[current_type].size = 0;

		for (unsigned int subindex = 0; subindex < asset_count; subindex++) {
			unsigned int current_handle = editor_state->asset_database->GetAssetHandleFromIndex(subindex, current_type);
			const void* metadata = editor_state->asset_database->GetAssetConst(current_handle, current_type);
			size_t external_time_stamp = 0;
			if constexpr (stamp_type == GET_OUT_OF_DATE_ASSETS_BOTH) {
				external_time_stamp = GetAssetExternalTimeStamp(editor_state, metadata, current_type, assets_folder);
			}
			else if constexpr (stamp_type == GET_OUT_OF_DATE_ASSETS_TARGET) {
				external_time_stamp = GetAssetTargetFileTimeStamp(metadata, current_type, assets_folder);
			}
			else if constexpr (stamp_type == GET_OUT_OF_DATE_ASSETS_METADATA) {
				external_time_stamp = GetAssetMetadataTimeStamp(editor_state, metadata, current_type);
			}
			size_t runtime_time_stamp = GetAssetRuntimeTimeStamp(editor_state, metadata, current_type);

			if (external_time_stamp > runtime_time_stamp) {
				// Register it
				handles[current_type].Add(current_handle);

				if (update_stamp) {
					ChangeAssetTimeStamp(editor_state, metadata, current_type, external_time_stamp);
				}
			}
			else if (runtime_time_stamp == -1 && external_time_stamp != -1) {
				// Register it and insert the time stamp
				handles[current_type].Add(current_handle);

				if (update_stamp) {
					InsertAssetTimeStamp(editor_state, metadata, current_type);
				}
			}
		}
	}

	if (include_dependencies) {
		// Use the capacity streams as a way to resize when there is not enough capacity
		ECS_STACK_CAPACITY_STREAM(CapacityStream<unsigned int>, current_handles, ECS_ASSET_TYPE_COUNT);
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			current_handles[index] = { handles[index].buffer, (unsigned int)handles[index].size, editor_state->asset_database->GetAssetCount((ECS_ASSET_TYPE)index) };
		}

		for (size_t index = 0; index < std::size(ECS_ASSET_TYPES_WITH_DEPENDENCIES); index++) {
			ECS_ASSET_TYPE current_type = ECS_ASSET_TYPES_WITH_DEPENDENCIES[index];
			editor_state->asset_database->ForEachAsset(current_type, [&](unsigned int handle) {
				unsigned int existing_index = SearchBytes(
					current_handles[current_type].buffer,
					current_handles[current_type].size,
					handle,
					sizeof(unsigned int)
				);
				if (existing_index == -1) {
					ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 512);
					const void* current_asset = editor_state->asset_database->GetAssetConst(handle, current_type);
					GetAssetDependencies(current_asset, current_type, &dependencies);
					for (unsigned int subindex = 0; subindex < dependencies.size; subindex++) {
						bool is_out_of_date = SearchBytes(
							current_handles[dependencies[subindex].type].buffer,
							current_handles[dependencies[subindex].type].size,
							dependencies[subindex].handle,
							sizeof(unsigned int)
						) != -1;

						if (is_out_of_date) {
							current_handles[current_type].AddResize(handle, allocator, 3);
						}
					}
				}
			});
		}

		// Make sure to put back the streams
		for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
			handles[index] = current_handles[index];
		}
	}

	return handles;
}

// ----------------------------------------------------------------------------------------------

Stream<Stream<unsigned int>> GetOutOfDateAssets(EditorState* editor_state, AllocatorPolymorphic allocator, bool update_stamp, bool include_dependencies)
{
	return GetOutOfDateAssetsImpl<GET_OUT_OF_DATE_ASSETS_BOTH>(editor_state, allocator, update_stamp, include_dependencies);
}

// ----------------------------------------------------------------------------------------------

Stream<Stream<unsigned int>> GetOutOfDateAssetsMetadata(EditorState* editor_state, AllocatorPolymorphic allocator, bool update_stamp, bool include_dependencies)
{
	return GetOutOfDateAssetsImpl<GET_OUT_OF_DATE_ASSETS_METADATA>(editor_state, allocator, update_stamp, include_dependencies);
}

// ----------------------------------------------------------------------------------------------

Stream<Stream<unsigned int>> GetOutOfDateAssetsTargetFile(EditorState* editor_state, AllocatorPolymorphic allocator, bool update_stamp, bool include_dependencies)
{
	return GetOutOfDateAssetsImpl<GET_OUT_OF_DATE_ASSETS_TARGET>(editor_state, allocator, update_stamp, include_dependencies);
}

// ----------------------------------------------------------------------------------------------

Stream<Stream<unsigned int>> GetDependentAssetsFor(
	const EditorState* editor_state, 
	const void* metadata, 
	ECS_ASSET_TYPE type, 
	AllocatorPolymorphic allocator, 
	bool include_itself
)
{
	return editor_state->asset_database->GetDependentAssetsFor(metadata, type, allocator, include_itself);
}

// ----------------------------------------------------------------------------------------------

size_t GetAssetRuntimeTimeStamp(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, storage, 512);

	// The time stamp stored is already the maximum between the 2 values
	Stream<wchar_t> metadata_file = AssetMetadataTimeStampPath(editor_state, metadata, type, storage);
	size_t metadata_time_stamp = editor_state->runtime_resource_manager->GetTimeStamp(metadata_file, ResourceType::TimeStamp);

	return metadata_time_stamp;
}

// ----------------------------------------------------------------------------------------------

size_t GetAssetExternalTimeStamp(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, storage, 512);

	size_t time_stamp = 0;
	Stream<wchar_t> target_file = AssetTargetFileTimeStampPath(editor_state, metadata, type, storage);
	if (target_file.size > 0) {
		time_stamp = OS::GetFileLastWrite(target_file);
		time_stamp = time_stamp == -1 ? 0 : time_stamp;
	}
	storage.size = 0;

	Stream<wchar_t> metadata_file = AssetMetadataTimeStampPath(editor_state, metadata, type, storage);
	size_t metadata_time_stamp = OS::GetFileLastWrite(metadata_file);
	return std::max(time_stamp, metadata_time_stamp);
}

// ----------------------------------------------------------------------------------------------

size_t GetAssetExternalTimeStamp(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, Stream<wchar_t> assets_folder)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, storage, 512);

	size_t time_stamp = 0;
	Stream<wchar_t> target_file = AssetTargetFileTimeStampPath(metadata, type, assets_folder, storage);
	if (target_file.size > 0) {
		time_stamp = OS::GetFileLastWrite(target_file);
		time_stamp = time_stamp == -1 ? 0 : time_stamp;
	}
	storage.size = 0;

	Stream<wchar_t> metadata_file = AssetMetadataTimeStampPath(editor_state, metadata, type, storage);
	size_t metadata_time_stamp = OS::GetFileLastWrite(metadata_file);
	return std::max(time_stamp, metadata_time_stamp);
}

// ----------------------------------------------------------------------------------------------

size_t GetAssetMetadataTimeStamp(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, storage, 512);
	Stream<wchar_t> metadata_file = AssetMetadataTimeStampPath(editor_state, metadata, type, storage);
	return OS::GetFileLastWrite(metadata_file);
}

// ----------------------------------------------------------------------------------------------

size_t GetAssetTargetFileTimeStamp(const EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, storage, 512);
	Stream<wchar_t> target_file = AssetTargetFileTimeStampPath(editor_state, metadata, type, storage);
	return target_file.size > 0 ? OS::GetFileLastWrite(target_file) : 0;
}

// ----------------------------------------------------------------------------------------------

size_t GetAssetTargetFileTimeStamp(const void* metadata, ECS_ASSET_TYPE type, Stream<wchar_t> assets_folder)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, storage, 512);
	Stream<wchar_t> target_file = AssetTargetFileTimeStampPath(metadata, type, assets_folder, storage);
	return target_file.size > 0 ? OS::GetFileLastWrite(target_file) : 0;
}

// ----------------------------------------------------------------------------------------------

unsigned int GetAssetReferenceCount(const EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type)
{
	return editor_state->asset_database->GetReferenceCount(handle, type);
}

// ----------------------------------------------------------------------------------------------

Stream<Stream<wchar_t>> GetAssetsFromAssetsFolder(const EditorState* editor_state, AllocatorPolymorphic allocator)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, assets_folder, 512);
	GetProjectAssetsFolder(editor_state, assets_folder);

	ECS_STACK_CAPACITY_STREAM(Stream<wchar_t>, valid_extensions, 64);
	GetAssetExtensionsWithThunkOrForwardingFile(valid_extensions);

	ResizableStream<Stream<wchar_t>> asset_files;
	asset_files.Initialize(allocator, 0);

	AdditionStream<Stream<wchar_t>> asset_files_addition;
	asset_files_addition.is_capacity = false;
	asset_files_addition.resizable_stream = &asset_files;

	GetDirectoriesOrFilesOptions options;
	options.relative_root = assets_folder;
	GetDirectoryFilesWithExtensionRecursive(assets_folder, allocator, asset_files_addition, valid_extensions, options);

	return asset_files.ToStream();
}

// ----------------------------------------------------------------------------------------------

Stream<Stream<unsigned int>> GetNotLoadedAssets(
	const EditorState* editor_state, 
	AllocatorPolymorphic stack_allocator, 
	Stream<Stream<unsigned int>> handles,
	bool* has_entries
)
{
	*has_entries = false;

	Stream<Stream<unsigned int>> entries;
	entries.Initialize(stack_allocator, ECS_ASSET_TYPE_COUNT);
	for (size_t asset_type = 0; asset_type < ECS_ASSET_TYPE_COUNT; asset_type++) {
		entries[asset_type].Initialize(stack_allocator, handles[asset_type].size);
		entries[asset_type].size = 0;
		for (size_t index = 0; index < handles[asset_type].size; index++) {
			if (!IsAssetBeingLoaded(editor_state, handles[asset_type][index], (ECS_ASSET_TYPE)asset_type)) {
				entries[asset_type].Add(handles[asset_type][index]);
				*has_entries = true;
			}
		}
	}

	return entries;
}

// ----------------------------------------------------------------------------------------------

bool HasAssetTimeStamp(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type)
{
	return HasAssetTimeStamp(editor_state, editor_state->asset_database->GetAssetConst(handle, type), type);
}

// ----------------------------------------------------------------------------------------------

bool HasAssetTimeStamp(EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, storage, 512);
	Stream<wchar_t> target_file = AssetMetadataTimeStampPath(editor_state, metadata, type, storage);
	return editor_state->RuntimeResourceManager()->Exists(target_file, ResourceType::TimeStamp);
}

// ----------------------------------------------------------------------------------------------

void InsertAssetTimeStamp(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, bool if_is_missing)
{
	const void* metadata = editor_state->asset_database->GetAssetConst(handle, type);
	InsertAssetTimeStamp(editor_state, metadata, type, if_is_missing);
}

// ----------------------------------------------------------------------------------------------

void InsertAssetTimeStamp(EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type, bool if_is_missing) {
	if (if_is_missing) {
		if (HasAssetTimeStamp(editor_state, metadata, type)) {
			return;
		}
	}

	ECS_STACK_CAPACITY_STREAM(wchar_t, storage, 512);

	size_t runtime_asset_stamp = 0;

	Stream<wchar_t> target_file = AssetTargetFileTimeStampPath(editor_state, metadata, type, storage);
	if (target_file.size > 0) {
		runtime_asset_stamp = OS::GetFileLastWrite(target_file);
		runtime_asset_stamp = runtime_asset_stamp == -1 ? 0 : runtime_asset_stamp;
	}
	storage.size = 0;

	Stream<wchar_t> metadata_file = AssetMetadataTimeStampPath(editor_state, metadata, type, storage);
	size_t metadata_file_stamp = OS::GetFileLastWrite(metadata_file);
	metadata_file_stamp = metadata_file_stamp == -1 ? 0 : metadata_file_stamp;

	editor_state->runtime_resource_manager->AddTimeStamp(metadata_file, max(metadata_file_stamp, runtime_asset_stamp));
}

// ----------------------------------------------------------------------------------------------

#define MAX_FAILURE_COUNT ECS_KB * 16

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
	// We need this array separately since there can be assets
	// Which are already loaded and are brought in the database
	// And when we try to remove them from the loading assets array
	// They are not there
	Stream<Stream<unsigned int>> missing_handles;
	EntityManager* update_link_entity_manager;

	bool success;
	bool has_launched;
	bool insert_time_stamps;

	// This is needed only to unlock a sandbox, if that is desired
	unsigned int sandbox_index;

	// We need this list of pointers for randomized assets - meshes and materials are allocated
	// When registering materials tho it will use the allocation already made in order to copy the contents even when successful.
	// We also need the list of pointers such that we can update the link components
	void** previous_pointers[ECS_ASSET_TYPE_COUNT];
	std::atomic<LOAD_EDITOR_ASSETS_STATE>* load_state;
};

static LoadAssetInfo LoadInfoFromEventData(EditorState* editor_state, LoadSandboxMissingAssetsEventData* event_data, Stream<wchar_t> mount_point) {
	LoadAssetInfo info;

	info.finish_semaphore = &event_data->semaphore;
	info.load_failures = event_data->failures;
	info.mount_point = mount_point;
	info.success = &event_data->success;

	return info;
}


static EDITOR_EVENT(LoadSandboxMissingAssetsEvent) {
	LoadSandboxMissingAssetsEventData* data = (LoadSandboxMissingAssetsEventData*)_data;

	if (data->semaphore.count.load(ECS_RELAXED) != 0) {
		// It is not finished yet. Push the event again
		return true;
	}
	else {
		// Check to see if we have launched/need to launch the load
		if (!data->has_launched) {
			// We need to wait graphics initialization as well
			if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
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
				LoadAssetInfo load_info = LoadInfoFromEventData(editor_state, data, assets_folder);
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

			if (data->load_state != nullptr) {
				*data->load_state = failure_count == 0 ? LOAD_EDITOR_ASSETS_SUCCESS : LOAD_EDITOR_ASSETS_PENDING;
			}

			// Update the link components
			RemoveLoadingAssets(editor_state, data->missing_handles);

			data->database.DeallocateStreams();
			DeallocateIfBelongs(allocator, data->failures->buffer);
			Deallocate(allocator, data);
			for (size_t asset_index = 0; asset_index < ECS_ASSET_TYPE_COUNT; asset_index++) {
				data->missing_handles[asset_index].Deallocate(allocator);
			}
			data->missing_handles.Deallocate(allocator);

			EditorStateClearFlag(editor_state, EDITOR_STATE_PREVENT_LAUNCH);
			EditorStateClearFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING);

			if (data->update_link_entity_manager != nullptr) {
				UpdateAssetsToComponents(editor_state, update_elements, data->update_link_entity_manager);
			}
			else {
				UpdateAssetsToComponents(editor_state, update_elements);
			}

			// Re-render the sandbox, if present
			if (data->sandbox_index != -1) {
				// Unlock the sandbox as well
				UnlockSandbox(editor_state, data->sandbox_index);
				RenderSandboxViewports(editor_state, data->sandbox_index);
			}

			return false;
		}
	}
}

static LoadSandboxMissingAssetsEventData* InitializeEventDataBase(EditorState* editor_state, AtomicStream<LoadAssetFailure>* failures = nullptr) {
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

static LoadSandboxMissingAssetsEventData* InitializeEventData(
	EditorState* editor_state,
	CapacityStream<unsigned int>* missing_assets
) {
	LoadSandboxMissingAssetsEventData* data = InitializeEventDataBase(editor_state);
	data->database = editor_state->asset_database->Copy(missing_assets, editor_state->EditorAllocator());
	data->sandbox_index = -1;
	data->missing_handles.Initialize(editor_state->EditorAllocator(), ECS_ASSET_TYPE_COUNT);
	for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
		data->missing_handles[index].InitializeAndCopy(editor_state->EditorAllocator(), missing_assets[index]);
	}
	data->load_state = nullptr;
	return data;
}

static LoadSandboxMissingAssetsEventData* InitializeEventData(
	EditorState* editor_state,
	Stream<Stream<unsigned int>> missing_assets,
	AtomicStream<LoadAssetFailure>* failures = nullptr
) {
	LoadSandboxMissingAssetsEventData* data = InitializeEventDataBase(editor_state, failures);
	data->database = editor_state->asset_database->Copy(missing_assets.buffer, editor_state->EditorAllocator());
	data->sandbox_index = -1;
	data->missing_handles.Initialize(editor_state->EditorAllocator(), ECS_ASSET_TYPE_COUNT);
	for (size_t index = 0; index < ECS_ASSET_TYPE_COUNT; index++) {
		data->missing_handles[index].InitializeAndCopy(editor_state->EditorAllocator(), missing_assets[index]);
	}
	data->load_state = nullptr;
	return data;
}

// ----------------------------------------------------------------------------------------------

template<typename HandleStreamType>
static void LoadEditorAssetsImpl(
	EditorState* editor_state,
	HandleStreamType handles,
	LoadEditorAssetsOptionalData* optional_data
) {
	// Check to see if there are actually any assets to be loaded
	unsigned int total_count = 0;
	for (size_t asset_type = 0; asset_type < ECS_ASSET_TYPE_COUNT; asset_type++) {
		total_count += handles[asset_type].size;
	}

	if (total_count > 0) {
		// Add the assets to the loading array
		AddLoadingAssets(editor_state, handles);

		LoadSandboxMissingAssetsEventData* event_data = InitializeEventData(editor_state, handles);
		event_data->sandbox_index = optional_data->sandbox_index;
		event_data->load_state = optional_data->state;
		event_data->update_link_entity_manager = optional_data->update_link_entity_manager;
		if (optional_data->state != nullptr) {
			*optional_data->state = LOAD_EDITOR_ASSETS_PENDING;
		}

		if (optional_data->callback == nullptr) {
			EditorAddEvent(editor_state, LoadSandboxMissingAssetsEvent, event_data, 0);
		}
		else {
			EditorAddEventWithContinuation(
				editor_state, 
				LoadSandboxMissingAssetsEvent, 
				event_data, 
				0, 
				optional_data->callback,
				optional_data->callback_data,
				optional_data->callback_data_size
			);
		}
		if (optional_data->sandbox_index != -1) {
			// Lock the sandbox as well
			LockSandbox(editor_state, optional_data->sandbox_index);
		}
	}
	else {
		// If there is a callback, can add it now
		if (optional_data->callback != nullptr) {
			EditorAddEvent(editor_state, optional_data->callback, optional_data->callback_data, optional_data->callback_data_size);
		}
	}
}

// ----------------------------------------------------------------------------------------------

void LoadEditorAssets(
	EditorState* editor_state, 
	Stream<Stream<unsigned int>> handles, 
	LoadEditorAssetsOptionalData* optional_data
)
{
	LoadEditorAssetsImpl(editor_state, handles, optional_data);
}

// ----------------------------------------------------------------------------------------------

void LoadEditorAssets(
	EditorState* editor_state,
	CapacityStream<unsigned int>* handles,
	LoadEditorAssetsOptionalData* optional_data
) {
	LoadEditorAssetsImpl(editor_state, handles, optional_data);
}

// ----------------------------------------------------------------------------------------------

void LoadAssetsWithRemapping(EditorState* editor_state, Stream<Stream<unsigned int>> handles)
{
	// Determine which handles are not already being loaded
	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 64, ECS_MB);
	bool has_entries = false;
	Stream<Stream<unsigned int>> not_loading_handles = GetNotLoadedAssets(editor_state, &stack_allocator, handles, &has_entries);

	if (has_entries) {
		// Add these entries to the loading array
		AddLoadingAssets(editor_state, not_loading_handles);

		LoadSandboxMissingAssetsEventData* event_data = InitializeEventData(editor_state, not_loading_handles);
		EditorAddEvent(editor_state, LoadSandboxMissingAssetsEvent, event_data, 0);
	}
}

// ----------------------------------------------------------------------------------------------

bool RegisterGlobalAsset(
	EditorState* editor_state, 
	Stream<char> name, 
	Stream<wchar_t> file, 
	ECS_ASSET_TYPE type, 
	unsigned int* handle, 
	bool unload_if_existing, 
	UIActionHandler callback
)
{
	return AddRegisterAssetEvent(editor_state, name, file, type, handle, -1, unload_if_existing, callback);
}

// ----------------------------------------------------------------------------------------------

struct RemoveLoadingAssetsEventData {
	Stream<AssetTypedHandle> handles;
};

EDITOR_EVENT(RemoveLoadingAssetsEvent) {
	RemoveLoadingAssetsEventData* data = (RemoveLoadingAssetsEventData*)_data;
	RemoveLoadingAssets(editor_state, data->handles);
	data->handles.Deallocate(editor_state->EditorAllocator());
	return false;
}

void RegisterRemoveLoadingAssetsEvent(EditorState* editor_state, Stream<AssetTypedHandle> handles) {
	RemoveLoadingAssetsEventData event_data = { handles.Copy(editor_state->EditorAllocator()) };
	EditorAddEvent(editor_state, RemoveLoadingAssetsEvent, &event_data, sizeof(event_data));
}

// ----------------------------------------------------------------------------------------------

void RegisterRemoveLoadingAssetsEvent(EditorState* editor_state, Stream<Stream<unsigned int>> handles) {
	RemoveLoadingAssetsEventData event_data;
	size_t total_count = 0;
	for (size_t asset_type = 0; asset_type < ECS_ASSET_TYPE_COUNT; asset_type++) {
		total_count += handles[asset_type].size;
	}
	event_data.handles.Initialize(editor_state->EditorAllocator(), total_count);
	event_data.handles.size = 0;
	for (size_t asset_type = 0; asset_type < ECS_ASSET_TYPE_COUNT; asset_type) {
		for (size_t index = 0; index < handles[asset_type].size; index++) {
			event_data.handles.Add({ handles[asset_type][index], (ECS_ASSET_TYPE)asset_type });
		}
	}
	EditorAddEvent(editor_state, RemoveLoadingAssetsEvent, &event_data, sizeof(event_data));
}

// ----------------------------------------------------------------------------------------------

void RemoveAssetTimeStamp(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type)
{
	const void* metadata = editor_state->asset_database->GetAssetConst(handle, type);
	RemoveAssetTimeStamp(editor_state, metadata, type);
}

// ----------------------------------------------------------------------------------------------

void RemoveAssetTimeStamp(EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type)
{
	ECS_STACK_CAPACITY_STREAM(wchar_t, storage, 512);

	Stream<wchar_t> metadata_file = AssetMetadataTimeStampPath(editor_state, metadata, type, storage);
	editor_state->runtime_resource_manager->RemoveTimeStamp(metadata_file);
}

// ----------------------------------------------------------------------------------------------

bool RemoveAsset(EditorState* editor_state, void* metadata, ECS_ASSET_TYPE type)
{
	bool success = DeallocateAsset(editor_state, metadata, type);
	if (success) {
		RemoveAssetTimeStamp(editor_state, metadata, type);
	}
	return success;
}

// ----------------------------------------------------------------------------------------------

bool RemoveAsset(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, bool recurse)
{
	alignas(alignof(size_t)) char asset_metadata[AssetMetadataMaxByteSize()];
	void* metadata = editor_state->asset_database->GetAsset(handle, type);
	memcpy(asset_metadata, metadata, AssetMetadataByteSize(type));
	bool success = RemoveAsset(editor_state, asset_metadata, type);
	if (success) {
		ECS_STACK_CAPACITY_STREAM(AssetTypedHandle, dependencies, 512);
		AssetDatabaseRemoveInfo remove_info;
		if (recurse) {
			remove_info.dependencies = &dependencies;
			remove_info.remove_dependencies = false;
		}
		editor_state->asset_database->RemoveAssetForced(handle, type, &remove_info);

		if (recurse) {
			// Get its dependencies and remove them as well
			for (unsigned int index = 0; index < dependencies.size; index++) {
				// Get the reference count - if it is 1, it is maintained by this asset and will be removed
				unsigned int current_reference_count = editor_state->asset_database->GetReferenceCount(dependencies[index].handle, dependencies[index].type);
				if (current_reference_count == 1) {
					// The asset is already deallocated
					EvictAssetFromDatabase(editor_state, dependencies[index].handle, dependencies[index].type);
				}
				else {
					// We need to decrement the reference count
					editor_state->asset_database->RemoveAsset(dependencies[index].handle, dependencies[index].type);
				}
			}
		}
	}
	return success;
}

// ----------------------------------------------------------------------------------------------

void RemoveAssetDependencies(EditorState* editor_state, const void* metadata, ECS_ASSET_TYPE type)
{
	editor_state->asset_database->RemoveAssetDependencies(metadata, type, [=](unsigned int handle, ECS_ASSET_TYPE type) {
		RemoveAssetTimeStamp(editor_state, handle, type);
	});
}

// ----------------------------------------------------------------------------------------------

void RemoveLoadingAsset(EditorState* editor_state, AssetTypedHandle handle)
{
	unsigned int load_index = FindAssetBeingLoaded(editor_state, handle.handle, handle.type);
	ECS_ASSERT(load_index != -1);
	editor_state->loading_assets.RemoveSwapBack(load_index);
}

// ----------------------------------------------------------------------------------------------

void RemoveLoadingAssets(EditorState* editor_state, Stream<Stream<unsigned int>> handles)
{
	ECS_ASSERT(handles.size == ECS_ASSET_TYPE_COUNT);
	for (size_t asset_type = 0; asset_type < ECS_ASSET_TYPE_COUNT; asset_type++) {
		for (size_t index = 0; index < handles[asset_type].size; index++) {
			RemoveLoadingAsset(editor_state, { handles[asset_type][index], (ECS_ASSET_TYPE)asset_type });
		}
	}
}

// ----------------------------------------------------------------------------------------------

void RemoveLoadingAssets(EditorState* editor_state, Stream<AssetTypedHandle> handles)
{
	for (size_t index = 0; index < handles.size; index++) {
		RemoveLoadingAsset(editor_state, handles[index]);
	}
	// We can trim here to reduce the memory consumption
	editor_state->loading_assets.TrimPercentageDefault();
}

// ----------------------------------------------------------------------------------------------

bool WriteForwardingFile(Stream<wchar_t> absolute_path, Stream<wchar_t> target_path)
{
	return WriteBufferToFileBinary(absolute_path, target_path) == ECS_FILE_STATUS_OK;
}

// ----------------------------------------------------------------------------------------------

void UnregisterGlobalAsset(EditorState* editor_state, unsigned int handle, ECS_ASSET_TYPE type, UIActionHandler callback) {
	AssetTypedHandle element;
	element.handle = handle;
	element.type = type;
	UnregisterGlobalAsset(editor_state, Stream<AssetTypedHandle>(&element, 1), callback);
}

// ----------------------------------------------------------------------------------------------

void UnregisterGlobalAsset(EditorState* editor_state, Stream<AssetTypedHandle> elements, UIActionHandler callback) {
	AddUnregisterAssetEvent(editor_state, elements, false, -1, callback);
}

// ----------------------------------------------------------------------------------------------

void UnregisterGlobalAsset(EditorState* editor_state, Stream<Stream<unsigned int>> elements, UIActionHandler callback) {
	AddUnregisterAssetEventHomogeneous(editor_state, elements, false, -1, callback);
}

// ----------------------------------------------------------------------------------------------