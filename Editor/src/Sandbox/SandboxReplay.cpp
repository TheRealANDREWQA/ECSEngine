#include "editorpch.h"
#include "SandboxReplay.h"
#include "SandboxAccessor.h"
#include "Sandbox.h"
#include "../Project/ProjectFolders.h"

#define PATH_MAX_CAPACITY 512

#define REPLAY_ALLOCATOR_CAPACITY ECS_MB * 32
#define REPLAY_INSTRUMENT_BUFFERING_CAPACITY ECS_MB

static void UpdateValidFileBoolReplay(const EditorState* editor_state, const SandboxReplayInfo& info) {
	*info.is_replay_file_valid = false;

	// Verify that the parent path is valid
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path, 512);
	Stream<wchar_t> parent_path = PathParent(*info.file_path, ECS_OS_PATH_SEPARATOR_REL);
	GetProjectPathFromAssetRelative(editor_state, absolute_path, parent_path);
	if (ExistsFileOrFolder(absolute_path)) {
		Stream<wchar_t> file_stem = PathStem(*info.file_path, ECS_OS_PATH_SEPARATOR_REL);
		if (file_stem.size > 0) {
			absolute_path.AddStreamAssert(file_stem);
			absolute_path.AddStreamAssert(EDITOR_INPUT_RECORDING_FILE_EXTENSION);
			if (ExistsFileOrFolder(absolute_path)) {
				*info.is_replay_file_valid = true;
			}
		}
	}
}

static Stream<wchar_t> GetSandboxReplayFileImpl(EditorState* editor_state, const SandboxReplayInfo& info, CapacityStream<wchar_t>& storage) {
	UpdateValidFileBoolReplay(editor_state, info);

	if (*info.is_replay_file_valid && info.file_path->size > 0) {
		Stream<wchar_t> path_no_extension = PathNoExtension(*info.file_path, ECS_OS_PATH_SEPARATOR_REL);
		GetProjectPathFromAssetRelative(editor_state, storage, path_no_extension);
		storage.AddStreamAssert(info.extension);
		return storage;
	}
	else {
		return { nullptr, 0 };
	}
}

typedef void (*InitializeSandboxReplayFunctor)(DeltaStateReaderInitializeFunctorInfo& initialize_info, World* world, CapacityStream<void>& stack_memory);

static bool InitializeSandboxReplayImpl(
	EditorState* editor_state,
	unsigned int sandbox_index,
	bool check_that_it_is_enabled,
	const SandboxReplayInfo& info,
	size_t allocator_size,
	size_t buffering_size,
	InitializeSandboxReplayFunctor initialize_functor
) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	if (check_that_it_is_enabled) {
		if (!HasFlag(sandbox->flags, info.flag)) {
			return true;
		}
	}

	// Update the file bool status - if it is not set, then don't continue
	UpdateValidFileBoolReplay(editor_state, info);
	if (!*info.is_replay_file_valid) {
		return true;
	}

	*info.is_delta_reader_initialized = false;
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_file_path_storage, 1024);
	Stream<wchar_t> absolute_file_path = GetSandboxReplayFileImpl(editor_state, info, absolute_file_path_storage);
	ECS_STACK_CAPACITY_STREAM(char, error_message, 1024);

	// Try to open the file firstly
	ECS_FILE_HANDLE file_handle = -1;
	ECS_FILE_STATUS_FLAGS status = OpenFile(absolute_file_path, &file_handle, ECS_FILE_ACCESS_READ_BINARY_SEQUENTIAL, &error_message);
	if (status != ECS_FILE_STATUS_OK) {
		ECS_FORMAT_TEMP_STRING(console_message, "Opening {#} replay file {#} failed. Reason: {#}", info.type_string, *info.file_path, error_message);
		EditorSetConsoleError(console_message);
		return false;
	}

	// Can create the allocator now. It doesn't need to be extremely large, since it is not used to read the actual scene in memory.
	AllocatorPolymorphic sandbox_allocator = sandbox->GlobalMemoryManager();
	MemoryManager* replay_allocator = (MemoryManager*)Allocate(sandbox_allocator, sizeof(MemoryManager));
	*replay_allocator = MemoryManager(allocator_size, ECS_KB * 4, allocator_size, sandbox_allocator);

	CapacityStream<void> read_instrument_buffering;
	read_instrument_buffering.Initialize(replay_allocator, buffering_size);

	// Allocate the read instrument out of the replay allocator
	BufferedFileReadInstrument* read_instrument = AllocateAndConstruct<BufferedFileReadInstrument>(replay_allocator, file_handle, read_instrument_buffering, 0);

	ECS_STACK_VOID_STREAM(stack_memory, ECS_KB * 32);
	DeltaStateReaderInitializeInfo initialize_info;
	initialize_functor(initialize_info.functor_info, &sandbox->sandbox_world, stack_memory);

	initialize_info.read_instrument = read_instrument;
	initialize_info.allocator = replay_allocator;
	initialize_info.error_message = &error_message;

	bool success = info.delta_reader->Initialize(initialize_info);
	if (success) {
		*info.is_delta_reader_initialized = true;
	}
	else {
		ECS_FORMAT_TEMP_STRING(console_message, "Failed to initialize {#} replay for sandbox {#}. Reason: {#}", info.type_string, sandbox_index, error_message);
		EditorSetConsoleError(console_message);
	}
	return success;
}

SandboxReplayInfo GetSandboxReplayInfo(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type) {
	SandboxReplayInfo info;

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	switch (type) {
	case EDITOR_SANDBOX_RECORDING_INPUT:
	{
		info.delta_reader = &sandbox->input_replay;
		info.extension = EDITOR_INPUT_RECORDING_FILE_EXTENSION;
		info.file_path = &sandbox->input_replay_file;
		info.flag = EDITOR_SANDBOX_FLAG_REPLAY_INPUT;
		info.is_delta_reader_initialized = &sandbox->is_input_replay_initialized;
		info.is_replay_file_valid = &sandbox->is_input_replay_valid;
		info.type_string = EDITOR_INPUT_RECORDER_TYPE_STRING;
	}
	break;
	case EDITOR_SANDBOX_RECORDING_STATE:
	{
		info.delta_reader = &sandbox->state_replay;
		info.extension = EDITOR_STATE_RECORDING_FILE_EXTENSION;
		info.file_path = &sandbox->state_replay_file;
		info.flag = EDITOR_SANDBOX_FLAG_REPLAY_STATE;
		info.is_delta_reader_initialized = &sandbox->is_state_replay_initialized;
		info.is_replay_file_valid = &sandbox->is_state_replay_valid;
		info.type_string = EDITOR_STATE_RECORDER_TYPE_STRING;
	}
	break;
	default:
		ECS_ASSERT(false, "Invalid sandbox replay type enum");
	}

	return info;
}

Stream<wchar_t> GetSandboxReplayFile(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type, CapacityStream<wchar_t>& storage) {
	SandboxReplayInfo info = GetSandboxReplayInfo(editor_state, sandbox_index, type);
	return GetSandboxReplayFileImpl(editor_state, info, storage);
}

void DeallocateSandboxReplay(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	SandboxReplayInfo info = GetSandboxReplayInfo(editor_state, sandbox_index, type);
	
	if (*info.is_delta_reader_initialized) {
		// Close the file
		const BufferedFileReadInstrument* read_instrument = (BufferedFileReadInstrument*)info.delta_reader->read_instrument;
		CloseFile(read_instrument->file);

		// Deallocate the allocator, including its pointer
		FreeAllocator(info.delta_reader->allocator);
		Deallocate(sandbox->GlobalMemoryManager(), info.delta_reader->allocator.allocator);
		ZeroOut(info.delta_reader);
		*info.is_delta_reader_initialized = false;
	}
}

void DeallocateSandboxReplays(EditorState* editor_state, unsigned int sandbox_index) {
	for (size_t index = 0; index < EDITOR_SANDBOX_RECORDING_TYPE_COUNT; index++) {
		DeallocateSandboxReplay(editor_state, sandbox_index, (EDITOR_SANDBOX_RECORDING_TYPE)index);
	}
}

void DisableSandboxReplay(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	SandboxReplayInfo info = GetSandboxReplayInfo(editor_state, sandbox_index, type);
	sandbox->flags = ClearFlag(sandbox->flags, info.flag);
}

void EnableSandboxReplay(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	SandboxReplayInfo info = GetSandboxReplayInfo(editor_state, sandbox_index, type);
	sandbox->flags = SetFlag(sandbox->flags, info.flag);
}

bool IsSandboxReplayEnabled(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type) {
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	SandboxReplayInfo info = GetSandboxReplayInfo(editor_state, sandbox_index, type);
	return HasFlag(sandbox->flags, info.flag);
}

bool InitializeSandboxReplay(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type, bool check_that_it_is_enabled) {
	InitializeSandboxReplayFunctor initialize_functor = nullptr;
	switch (type) {
	case EDITOR_SANDBOX_RECORDING_INPUT:
		initialize_functor = SetInputDeltaReaderWorldInitializeInfo;
		break;
	case EDITOR_SANDBOX_RECORDING_STATE:
		// TODO: Implement this
		break;
	default:
		ECS_ASSERT(false, "Invalid sandbox recording type enum in initialize replay");
	}

	return InitializeSandboxReplayImpl(editor_state, sandbox_index, check_that_it_is_enabled, GetSandboxReplayInfo(editor_state, sandbox_index, type), REPLAY_ALLOCATOR_CAPACITY, REPLAY_INSTRUMENT_BUFFERING_CAPACITY, initialize_functor);
}

bool InitializeSandboxReplays(EditorState* editor_state, unsigned int sandbox_index, bool check_that_it_is_enabled) {
	bool success = true;
	for (size_t index = 0; index < EDITOR_SANDBOX_RECORDING_TYPE_COUNT; index++) {
		success &= InitializeSandboxReplay(editor_state, sandbox_index, (EDITOR_SANDBOX_RECORDING_TYPE)index, check_that_it_is_enabled);
	}
	return success;
}

void ResetSandboxReplay(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type) {
	SandboxReplayInfo info = GetSandboxReplayInfo(editor_state, sandbox_index, type);

	ZeroOut(info.delta_reader);
	*info.is_delta_reader_initialized = false;
	*info.is_replay_file_valid = false;
	info.file_path->Initialize(GetSandbox(editor_state, sandbox_index)->GlobalMemoryManager(), 0, PATH_MAX_CAPACITY);
}

void ResetSandboxReplays(EditorState* editor_state, unsigned int sandbox_index) {
	for (size_t index = 0; index < EDITOR_SANDBOX_RECORDING_TYPE_COUNT; index++) {
		ResetSandboxReplay(editor_state, sandbox_index, (EDITOR_SANDBOX_RECORDING_TYPE)index);
	}
}

bool RunSandboxReplay(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type) {
	SandboxReplayInfo info = GetSandboxReplayInfo(editor_state, sandbox_index, type);
	if (*info.is_delta_reader_initialized) {
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		if (HasFlag(sandbox->flags, info.flag) && !info.delta_reader->IsFailed()) {
			ECS_STACK_CAPACITY_STREAM(char, error_message, ECS_KB);
			if (!info.delta_reader->Advance(sandbox->sandbox_world.elapsed_seconds, &error_message)) {
				ECS_FORMAT_TEMP_STRING(console_message, "Failed to read {#} replay at moment {#}. (Reason: {#})", info.type_string, sandbox->sandbox_world.elapsed_seconds, error_message);
				EditorSetConsoleError(console_message);

				// Pause the sandbox worlds to let the user know
				PauseSandboxWorlds(editor_state);
				return false;
			}
		}
	}
	return true;
}

bool RunSandboxReplays(EditorState* editor_state, unsigned int sandbox_index) {
	bool success = true;
	for (size_t index = 0; index < EDITOR_SANDBOX_RECORDING_TYPE_COUNT; index++) {
		success &= RunSandboxReplay(editor_state, sandbox_index, (EDITOR_SANDBOX_RECORDING_TYPE)index);
	}
	return success;
}

void UpdateSandboxValidFileBoolReplay(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type) {
	UpdateValidFileBoolReplay(editor_state, GetSandboxReplayInfo(editor_state, sandbox_index, type));
}