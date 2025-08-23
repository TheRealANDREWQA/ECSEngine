#include "editorpch.h"
#include "SandboxReplay.h"
#include "SandboxAccessor.h"
#include "Sandbox.h"
#include "../Project/ProjectFolders.h"
#include "../Modules/Module.h"
#include "../Assets/EditorSandboxAssets.h"
#include "../UI/Inspector.h"

#define PATH_MAX_CAPACITY 512

#define REPLAY_ALLOCATOR_CAPACITY ECS_MB * 32
#define REPLAY_INSTRUMENT_BUFFERING_CAPACITY ECS_MB

static void UpdateValidFileBoolReplay(const EditorState* editor_state, const SandboxReplayInfo& info) {
	info.replay->is_file_valid = false;

	// Verify that the parent path is valid
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	Stream<wchar_t> parent_path = PathParent(info.replay->file, ECS_OS_PATH_SEPARATOR_REL);
	GetProjectPathFromAssetRelative(editor_state, absolute_path, parent_path);
	if (ExistsFileOrFolder(absolute_path)) {
		Stream<wchar_t> file_stem = PathStem(info.replay->file, ECS_OS_PATH_SEPARATOR_REL);
		if (file_stem.size > 0) {
			absolute_path.AddAssert(ECS_OS_PATH_SEPARATOR);
			absolute_path.AddStreamAssert(file_stem);
			absolute_path.AddStreamAssert(info.extension);
			if (ExistsFileOrFolder(absolute_path)) {
				info.replay->is_file_valid = true;
			}
		}
	}
}

static Stream<wchar_t> GetSandboxReplayFileImpl(EditorState* editor_state, const SandboxReplayInfo& info, CapacityStream<wchar_t>& storage) {
	UpdateValidFileBoolReplay(editor_state, info);

	if (info.replay->is_file_valid && info.replay->file.size > 0) {
		Stream<wchar_t> path_no_extension = PathNoExtension(info.replay->file, ECS_OS_PATH_SEPARATOR_REL);
		GetProjectPathFromAssetRelative(editor_state, storage, path_no_extension);
		storage.AddStreamAssert(info.extension);
		return storage;
	}
	else {
		return { nullptr, 0 };
	}
}

typedef void (*InitializeSandboxReplayFunctor)(DeltaStateReaderInitializeFunctorInfo& initialize_info, EditorState* editor_state, unsigned int sandbox_handle, AllocatorPolymorphic temporary_allocator);

static bool InitializeSandboxReplayImpl(
	EditorState* editor_state,
	unsigned int sandbox_handle,
	bool check_that_it_is_enabled,
	const SandboxReplayInfo& info,
	size_t allocator_size,
	size_t buffering_size,
	InitializeSandboxReplayFunctor initialize_functor
) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	if (check_that_it_is_enabled) {
		if (!HasFlag(sandbox->flags, info.flag)) {
			return true;
		}
	}

	// If the replay is already initialized, can stop now
	if (info.replay->is_initialized) {
		return true;
	}

	// Update the file bool status - if it is not set, then don't continue
	UpdateValidFileBoolReplay(editor_state, info);
	if (!info.replay->is_file_valid) {
		return true;
	}

	info.replay->is_initialized = false;
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_file_path_storage, 1024);
	Stream<wchar_t> absolute_file_path = GetSandboxReplayFileImpl(editor_state, info, absolute_file_path_storage);
	ECS_STACK_CAPACITY_STREAM(char, error_message, 1024);

	// Try to open the file firstly
	ECS_FILE_HANDLE file_handle = -1;
	ECS_FILE_STATUS_FLAGS status = OpenFile(absolute_file_path, &file_handle, ECS_FILE_ACCESS_READ_BINARY_SEQUENTIAL, &error_message);
	if (status != ECS_FILE_STATUS_OK) {
		ECS_FORMAT_TEMP_STRING(console_message, "Opening {#} replay file {#} failed. Reason: {#}", info.type_string, info.replay->file, error_message);
		EditorSetConsoleError(console_message);
		return false;
	}

	// Can create the allocator now. It doesn't need to be extremely large, since it is not used to read the actual scene in memory.
	AllocatorPolymorphic sandbox_allocator = sandbox->GlobalMemoryManager();
	MemoryManager* replay_allocator = (MemoryManager*)Allocate(sandbox_allocator, sizeof(MemoryManager));
	new (replay_allocator) MemoryManager(allocator_size, ECS_KB * 4, allocator_size, sandbox_allocator);

	CapacityStream<void> read_instrument_buffering;
	read_instrument_buffering.Initialize(replay_allocator, buffering_size);

	// Allocate the read instrument out of the replay allocator
	BufferedFileReadInstrument* read_instrument = AllocateAndConstruct<BufferedFileReadInstrument>(replay_allocator, file_handle, read_instrument_buffering, 0);

	ECS_STACK_RESIZABLE_LINEAR_ALLOCATOR(stack_allocator, ECS_KB * 32, ECS_MB);
	DeltaStateReaderInitializeInfo initialize_info;
	initialize_functor(initialize_info.functor_info, editor_state, sandbox_handle, &stack_allocator);

	initialize_info.read_instrument = read_instrument;
	initialize_info.allocator = replay_allocator;
	initialize_info.error_message = &error_message;

	bool success = info.replay->delta_reader.Initialize(initialize_info);
	if (success) {
		info.replay->is_initialized = true;
	}
	else {
		ECS_FORMAT_TEMP_STRING(console_message, "Failed to initialize {#} replay for sandbox {#}. Reason: {#}", info.type_string, sandbox_handle, error_message);
		EditorSetConsoleError(console_message);
		// Close the file handle, such that it can be removed
		CloseFile(file_handle);
		// Also, deallocate the allocator
		replay_allocator->Free();
		Deallocate(sandbox_allocator, replay_allocator);
	}
	return success;
}

struct ClearUIEntityInspectorStateCallbackData {
	EditorState* editor_state;
	unsigned int sandbox_handle;
};

// This callback is used before deserialization to clear the Entity inspector's component allocators if they are custom for a component.
// The reason for this is because we clear the entire entity manager, and the component allocators get implicitely deallocated, but the
// UI doesn't know about this. So we need to this before the deserialization
static bool ClearUIEntityInspectorStateCallback(SceneDeltaReaderEntireCallbackData* callback_data) {
	BlittableCopyable<ClearUIEntityInspectorStateCallbackData>* data = (BlittableCopyable<ClearUIEntityInspectorStateCallbackData>*)callback_data->data;
	
	// Find all inspectors that target this sandbox and reset them
	ECS_STACK_CAPACITY_STREAM(unsigned int, sandbox_inspectors, 512);
	GetInspectorsForSandbox(data->data.editor_state, data->data.sandbox_handle, &sandbox_inspectors);

	for (unsigned int index = 0; index < sandbox_inspectors.size; index++) {
		InspectorEntityResetComponentAllocators(data->data.editor_state, sandbox_inspectors[index]);
	}

	return true;
}

SandboxReplayInfo GetSandboxReplayInfo(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type) {
	SandboxReplayInfo info;

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	switch (type) {
	case EDITOR_SANDBOX_RECORDING_INPUT:
	{
		info.replay = &sandbox->input_replay;
		info.extension = EDITOR_INPUT_RECORDING_FILE_EXTENSION;
		info.flag = EDITOR_SANDBOX_FLAG_REPLAY_INPUT;
		info.type_string = EDITOR_INPUT_RECORDER_TYPE_STRING;
	}
	break;
	case EDITOR_SANDBOX_RECORDING_STATE:
	{
		info.replay = &sandbox->state_replay;
		info.extension = EDITOR_STATE_RECORDING_FILE_EXTENSION;
		info.flag = EDITOR_SANDBOX_FLAG_REPLAY_STATE;
		info.type_string = EDITOR_STATE_RECORDER_TYPE_STRING;
	}
	break;
	default:
		ECS_ASSERT(false, "Invalid sandbox replay type enum");
	}

	return info;
}

Stream<wchar_t> GetSandboxReplayFile(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type, CapacityStream<wchar_t>& storage) {
	SandboxReplayInfo info = GetSandboxReplayInfo(editor_state, sandbox_handle, type);
	return GetSandboxReplayFileImpl(editor_state, info, storage);
}

void DeallocateSandboxReplay(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	SandboxReplayInfo info = GetSandboxReplayInfo(editor_state, sandbox_handle, type);
	
	if (info.replay->is_initialized) {
		// Close the file
		const BufferedFileReadInstrument* read_instrument = (BufferedFileReadInstrument*)info.replay->delta_reader.read_instrument;
		CloseFile(read_instrument->file);

		// Deallocate the allocator, including its pointer
		FreeAllocator(info.replay->delta_reader.allocator);
		Deallocate(sandbox->GlobalMemoryManager(), info.replay->delta_reader.allocator.allocator);
		ZeroOut(&info.replay->delta_reader);
		info.replay->is_initialized = false;
	}
}

void DeallocateSandboxReplays(EditorState* editor_state, unsigned int sandbox_handle) {
	for (size_t index = 0; index < EDITOR_SANDBOX_RECORDING_TYPE_COUNT; index++) {
		DeallocateSandboxReplay(editor_state, sandbox_handle, (EDITOR_SANDBOX_RECORDING_TYPE)index);
	}
}

void DisableSandboxReplay(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	SandboxReplayInfo info = GetSandboxReplayInfo(editor_state, sandbox_handle, type);
	sandbox->flags = ClearFlag(sandbox->flags, info.flag);
}

void EnableSandboxReplay(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	SandboxReplayInfo info = GetSandboxReplayInfo(editor_state, sandbox_handle, type);
	sandbox->flags = SetFlag(sandbox->flags, info.flag);
}

bool IsSandboxReplayEnabled(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type) {
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	SandboxReplayInfo info = GetSandboxReplayInfo(editor_state, sandbox_handle, type);
	return HasFlag(sandbox->flags, info.flag);
}

bool InitializeSandboxReplay(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type, bool check_that_it_is_enabled) {
	InitializeSandboxReplayFunctor initialize_functor = nullptr;
	switch (type) {
	case EDITOR_SANDBOX_RECORDING_INPUT:
	{
		initialize_functor = [](DeltaStateReaderInitializeFunctorInfo& initialize_info, EditorState* editor_state, unsigned int sandbox_handle, AllocatorPolymorphic temporary_allocator) -> void {
			SetInputDeltaReaderWorldInitializeInfo(initialize_info, &GetSandbox(editor_state, sandbox_handle)->sandbox_world, temporary_allocator);
		};
	}
		break;
	case EDITOR_SANDBOX_RECORDING_STATE:
	{
		initialize_functor = [](DeltaStateReaderInitializeFunctorInfo& initialize_info, EditorState* editor_state, unsigned int sandbox_handle, AllocatorPolymorphic temporary_allocator) -> void {
			SceneDeltaReaderInitializeInfoOptions options;
			
			ResizableStream<const AppliedModule*> applied_modules(temporary_allocator, 8);
			ModulesToAppliedModules(editor_state, &applied_modules);
			options.module_component_functions = ModuleAggregateComponentFunctions(applied_modules, temporary_allocator);

			// We can use the editor state asset database since it is readonly - it does not modify it.
			bool gather_deserialize_overrides = GetModuleTemporaryDeserializeOverrides(
				editor_state, 
				editor_state->asset_database, 
				temporary_allocator, 
				options.unique_overrides, 
				options.shared_overrides, 
				options.global_overrides
			);
			ECS_ASSERT_FORMAT(gather_deserialize_overrides, "Failed to gather module deserialize overrides for sandbox {#} scene replay", sandbox_handle);

			options.asset_callback = ReplaySandboxAssetsCallback;
			options.asset_callback_data = GetReplaySandboxAssetsCallbackData(editor_state, sandbox_handle, temporary_allocator);

			// We need to set some entire scene custom functors (at the moment just one)
			options.custom_entire_functors.Initialize(temporary_allocator, 1);
			options.custom_entire_functors[0].callback = ClearUIEntityInspectorStateCallback;
			options.custom_entire_functors[0].call_before_deserialization = true;
			BlittableCopyable<ClearUIEntityInspectorStateCallbackData>* clear_ui_inspectors_data = AllocateAndConstruct<BlittableCopyable<ClearUIEntityInspectorStateCallbackData>>(temporary_allocator);
			clear_ui_inspectors_data->data = { editor_state, sandbox_handle };
			options.custom_entire_functors[0].data = clear_ui_inspectors_data;

			// TODO: Add a prefab deserializer?

			SetSceneDeltaReaderWorldInitializeInfo(
				initialize_info,
				&GetSandbox(editor_state, sandbox_handle)->sandbox_world,
				editor_state->GlobalReflectionManager(),
				temporary_allocator,
				&options
			);
		};
	}
		break;
	default:
		ECS_ASSERT(false, "Invalid sandbox recording type enum in initialize replay");
	}

	return InitializeSandboxReplayImpl(editor_state, sandbox_handle, check_that_it_is_enabled, GetSandboxReplayInfo(editor_state, sandbox_handle, type), REPLAY_ALLOCATOR_CAPACITY, REPLAY_INSTRUMENT_BUFFERING_CAPACITY, initialize_functor);
}

bool InitializeSandboxReplays(EditorState* editor_state, unsigned int sandbox_handle, bool check_that_it_is_enabled) {
	bool success = true;
	for (size_t index = 0; index < EDITOR_SANDBOX_RECORDING_TYPE_COUNT; index++) {
		success &= InitializeSandboxReplay(editor_state, sandbox_handle, (EDITOR_SANDBOX_RECORDING_TYPE)index, check_that_it_is_enabled);
	}
	return success;
}

void ResetSandboxReplay(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type) {
	SandboxReplayInfo info = GetSandboxReplayInfo(editor_state, sandbox_handle, type);

	ZeroOut(&info.replay->delta_reader);
	info.replay->is_initialized = false;
	info.replay->is_file_valid  = false;
	info.replay->is_driving_delta_time = false;
	info.replay->file.Initialize(GetSandbox(editor_state, sandbox_handle)->GlobalMemoryManager(), 0, PATH_MAX_CAPACITY);
}

void ResetSandboxReplays(EditorState* editor_state, unsigned int sandbox_handle) {
	for (size_t index = 0; index < EDITOR_SANDBOX_RECORDING_TYPE_COUNT; index++) {
		ResetSandboxReplay(editor_state, sandbox_handle, (EDITOR_SANDBOX_RECORDING_TYPE)index);
	}
}

bool RunSandboxReplay(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type) {
	SandboxReplayInfo info = GetSandboxReplayInfo(editor_state, sandbox_handle, type);
	if (info.replay->is_initialized) {
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
		if (HasFlag(sandbox->flags, info.flag) && !info.replay->delta_reader.IsFailed() && !info.replay->delta_reader.IsFinished()) {
			ECS_STACK_CAPACITY_STREAM(char, error_message, ECS_KB);
			if (!info.replay->delta_reader.Advance(sandbox->sandbox_world.elapsed_seconds, &error_message)) {
				ECS_FORMAT_TEMP_STRING(console_message, "Failed to read {#} replay at moment {#} for sandbox {#}. (Reason: {#})", info.type_string, sandbox->sandbox_world.elapsed_seconds, sandbox_handle, error_message);
				EditorSetConsoleError(console_message);

				// Pause the sandbox worlds to let the user know
				PauseSandboxWorlds(editor_state);
				return false;
			}
		}
	}
	return true;
}

bool RunSandboxReplays(EditorState* editor_state, unsigned int sandbox_handle) {
	bool success = true;
	for (size_t index = 0; index < EDITOR_SANDBOX_RECORDING_TYPE_COUNT; index++) {
		success &= RunSandboxReplay(editor_state, sandbox_handle, (EDITOR_SANDBOX_RECORDING_TYPE)index);
	}
	return success;
}

EDITOR_SANDBOX_RECORDING_TYPE SetSandboxReplay(EditorState* editor_state, unsigned int sandbox_handle, Stream<wchar_t> replay_file, bool initialize_now)
{
	EDITOR_SANDBOX_RECORDING_TYPE type = GetRecordingTypeFromExtension(PathExtension(replay_file));
	SandboxReplayInfo replay_info = GetSandboxReplayInfo(editor_state, sandbox_handle, type);

	ECS_STACK_CAPACITY_STREAM(wchar_t, relative_path_storage, 512);
	Stream<wchar_t> relative_path = GetProjectAssetRelativePathWithSeparatorReplacement(editor_state, replay_file, relative_path_storage);
	// Remove the extension as well
	relative_path = PathNoExtension(relative_path, ECS_OS_PATH_SEPARATOR_REL);
	replay_info.replay->file.CopyOther(relative_path);

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	sandbox->flags = SetFlag(sandbox->flags, replay_info.flag);

	if (initialize_now) {
		// If this initialization fails, reset the type
		if (!InitializeSandboxReplay(editor_state, sandbox_handle, type, false)) {
			type = EDITOR_SANDBOX_RECORDING_TYPE_COUNT;
		}
	}

	return type;
}

void UpdateSandboxValidFileBoolReplay(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type) {
	UpdateValidFileBoolReplay(editor_state, GetSandboxReplayInfo(editor_state, sandbox_handle, type));
}