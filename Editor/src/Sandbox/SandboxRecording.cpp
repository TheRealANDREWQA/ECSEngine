#include "editorpch.h"
#include "SandboxRecording.h"
#include "SandboxAccessor.h"
#include "../Project/ProjectFolders.h"

#define PATH_MAX_CAPACITY 512

#define RECORDER_ALLOCATOR_CAPACITY ECS_MB * 32
#define RECORDER_BUFFERING_CAPACITY ECS_MB

#define INPUT_RECORDER_TYPE_STRING "input"
#define STATE_RECORDER_TYPE_STRING "state"

// Choose a reasonable default - 15 seconds seems a decent value
#define DEFAULT_ENTIRE_STATE_TICK_SECONDS 15.0f

using namespace ECSEngine;

static void DeallocateSandboxRecording(DeltaStateWriter& delta_writer, AllocatorPolymorphic sandbox_allocator) {
	// Just deallocate the recorder allocator - all else will be deallocated as well
	// The file should have been closed by the Finish call

	// The allocator pointer must be deallocated as well
	Deallocate(sandbox_allocator, delta_writer.allocator.allocator);
	FreeAllocator(delta_writer.allocator);
	ZeroOut(&delta_writer);
}

static bool FinishSandboxRecording(
	EditorState* editor_state,
	unsigned int sandbox_index,
	bool check_that_it_is_enabled,
	const SandboxRecordingInfo& info
) {
	if (check_that_it_is_enabled) {
		const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		if (!HasFlag(sandbox->flags, info.flag)) {
			return true;
		}
	}

	auto deallocate_stack_scope = StackScope([&]() {
		if (*info.is_delta_writer_initialized) {
			DeallocateSandboxRecording(*info.delta_writer, GetSandbox(editor_state, sandbox_index)->GlobalMemoryManager());
			*info.is_delta_writer_initialized = false;
		}
		});

	bool success = true;
	if (*info.is_delta_writer_initialized) {
		BufferedFileWriteInstrument* write_instrument = (BufferedFileWriteInstrument*)info.delta_writer->write_instrument;
		size_t before_flush_offset = write_instrument->GetOffset();

		// Flush the writer
		success = info.delta_writer->Flush();

		if (success) {
			// Close the file
			success = CloseFile(write_instrument->file);
		}
		else {
			ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_path_storage, 512);

			// Try once more. If we fail this time as well, then remove the file and report to the user
			if (!write_instrument->ResetAndSeekTo(ECS_INSTRUMENT_SEEK_START, before_flush_offset)) {
				ECS_FORMAT_TEMP_STRING(console_error, "Failed to finish {#} recording. Could not seek for a retry.", info.type_string);
				EditorSetConsoleError(console_error);

				// Remove the file
				CloseFile(write_instrument->file);
				Stream<wchar_t> absolute_file_path = GetProjectPathFromAssetRelative(editor_state, absolute_path_storage, *info.file_path);
				if (!RemoveFile(absolute_file_path)) {
					ECS_FORMAT_TEMP_STRING(remove_error, "Failed to remove {#} recording file.", *info.file_path);
					EditorSetConsoleError(remove_error);
				}
				return false;
			}

			// Try again
			if (!info.delta_writer->Flush()) {
				// Failed once more, exit
				ECS_FORMAT_TEMP_STRING(console_error, "Failed to finish {#} recording. Could not flush the contents.", info.type_string);
				EditorSetConsoleError(console_error);

				// Remove the file
				CloseFile(write_instrument->file);
				Stream<wchar_t> absolute_file_path = GetProjectPathFromAssetRelative(editor_state, absolute_path_storage, *info.file_path);
				if (!RemoveFile(absolute_file_path)) {
					ECS_FORMAT_TEMP_STRING(remove_error, "Failed to remove {#} recording file.", *info.file_path);
					EditorSetConsoleError(remove_error);
				}
				return false;
			}
			else {
				// Success, close the file
				success = CloseFile(write_instrument->file);
			}
		}
	}

	return success;
}

typedef void (*InitializeSandboxRecordingFunctor)(DeltaStateWriterInitializeFunctorInfo& initialize_info, const World* world, CapacityStream<void>& stack_memory);

static bool InitializeSandboxRecording(
	EditorState* editor_state,
	unsigned int sandbox_index,
	bool check_that_it_is_enabled,
	const SandboxRecordingInfo& info,
	size_t allocator_size,
	size_t buffering_size,
	InitializeSandboxRecordingFunctor initialize_functor
) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	if (check_that_it_is_enabled) {
		if (!HasFlag(sandbox->flags, info.flag)) {
			return true;
		}
	}

	// Open the file for write
	ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_file_path_storage, 512);
	Stream<wchar_t> absolute_file_path = GetProjectPathFromAssetRelative(editor_state, absolute_file_path_storage, *info.file_path);

	ECS_FILE_HANDLE input_file = -1;
	ECS_STACK_CAPACITY_STREAM(char, error_message, ECS_KB);
	ECS_FILE_STATUS_FLAGS status = OpenFile(absolute_file_path, &input_file, ECS_FILE_ACCESS_WRITE_BINARY_TRUNCATE, &error_message);
	if (status != ECS_FILE_STATUS_OK) {
		ECS_FORMAT_TEMP_STRING(console_message, "Opening {#} recording file {#} failed. Reason: {#}.", info.type_string, *info.file_path, error_message);
		EditorSetConsoleError(console_message);
		return false;
	}

	// Create an allocator just for the recorder, such that we can wink it at the end
	GlobalMemoryManager* sandbox_allocator = sandbox->GlobalMemoryManager();
	MemoryManager* input_recorder_allocator = (MemoryManager*)sandbox_allocator->Allocate(sizeof(MemoryManager));
	*input_recorder_allocator = MemoryManager(allocator_size, ECS_KB * 4, allocator_size, sandbox_allocator);

	// Allocate the write instrument out of it
	CapacityStream<void> write_instrument_buffering;
	write_instrument_buffering.Initialize(input_recorder_allocator, buffering_size);

	BufferedFileWriteInstrument* write_instrument = (BufferedFileWriteInstrument*)input_recorder_allocator->Allocate(sizeof(BufferedFileWriteInstrument));
	*write_instrument = BufferedFileWriteInstrument(input_file, write_instrument_buffering, 0);

	ECS_STACK_VOID_STREAM(stack_memory, ECS_KB * 32);
	DeltaStateWriterInitializeInfo initialize_info;
	initialize_functor(initialize_info.functor_info, &sandbox->sandbox_world, stack_memory);

	initialize_info.write_instrument = write_instrument;
	initialize_info.allocator = input_recorder_allocator;
	initialize_info.entire_state_write_seconds_tick = *info.entire_state_tick_seconds;

	info.delta_writer->Initialize(initialize_info);
	*info.is_delta_writer_initialized = true;
	return true;
}

static void ResetSandboxRecording(EditorState* editor_state, unsigned int sandbox_index, const SandboxRecordingInfo& info) {
	ZeroOut(info.delta_writer);
	*info.entire_state_tick_seconds = DEFAULT_ENTIRE_STATE_TICK_SECONDS;
	*info.is_delta_writer_initialized = false;
	*info.is_recording_automatic = false;
	info.file_path->Initialize(GetSandbox(editor_state, sandbox_index)->GlobalMemoryManager(), 0, PATH_MAX_CAPACITY);
}

SandboxRecordingInfo GetSandboxRecordingInfo(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	switch (type) {
	case EDITOR_SANDBOX_RECORDING_INPUT:
	{
		return { 
			&sandbox->input_recorder, 
			&sandbox->is_input_recorder_initialized, 
			&sandbox->is_input_recorder_file_automatic, 
			&sandbox->input_recorder_entire_state_tick_seconds, 
			&sandbox->input_recorder_file, 
			INPUT_RECORDER_TYPE_STRING, 
			EDITOR_SANDBOX_FLAG_RECORD_INPUT 
		};
	}
	break;
	case EDITOR_SANDBOX_RECORDING_STATE:
	{
		return { 
			&sandbox->state_recorder, 
			&sandbox->is_state_recorder_initialized, 
			&sandbox->is_state_recorder_file_automatic,
			&sandbox->state_recorder_entire_state_tick_seconds, 
			&sandbox->state_recorder_file, 
			STATE_RECORDER_TYPE_STRING, 
			EDITOR_SANDBOX_FLAG_RECORD_STATE
		};
	}
	break;
	default:
		ECS_ASSERT(false, "Invalid sandbox recording type enum.");
	}

	return {};
}

void ChangeSandboxRecordingFile(EditorState* editor_state, unsigned int sandbox_index, Stream<wchar_t> file, EDITOR_SANDBOX_RECORDING_TYPE type) {
	GetSandboxRecordingInfo(editor_state, sandbox_index, type).file_path->CopyOther(file);
}

void DisableSandboxRecording(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	SandboxRecordingInfo info = GetSandboxRecordingInfo(editor_state, sandbox_index, type);
	sandbox->flags = ClearFlag(sandbox->flags, info.flag);
}

void EnableSandboxRecording(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	SandboxRecordingInfo info = GetSandboxRecordingInfo(editor_state, sandbox_index, type);
	sandbox->flags = SetFlag(sandbox->flags, info.flag);
}

bool FinishSandboxRecording(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type, bool check_that_it_is_enabled) {
	return FinishSandboxRecording(editor_state, sandbox_index, check_that_it_is_enabled, GetSandboxRecordingInfo(editor_state, sandbox_index, type));
}

bool FinishSandboxRecordings(EditorState* editor_state, unsigned int sandbox_index, bool check_that_it_is_enabled) {
	bool success = true;
	for (size_t index = 0; index < EDITOR_SANDBOX_RECORDING_TYPE_COUNT; index++) {
		success &= FinishSandboxRecording(editor_state, sandbox_index, (EDITOR_SANDBOX_RECORDING_TYPE)index, check_that_it_is_enabled);
	}
	return success;
}

bool InitializeSandboxRecording(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type, bool check_that_it_is_enabled) {
	InitializeSandboxRecordingFunctor initialize_functor = nullptr;
	switch (type) {
	case EDITOR_SANDBOX_RECORDING_INPUT:
	{
		initialize_functor = SetInputDeltaWriterWorldInitializeInfo;
	}
	break;
	case EDITOR_SANDBOX_RECORDING_STATE:
	{
		// TODO: Handle this
	}
	break;
	default:
		ECS_ASSERT(false, "Invalid sandbox recording type enum in initialize.");
	}
	return InitializeSandboxRecording(editor_state, sandbox_index, check_that_it_is_enabled, GetSandboxRecordingInfo(editor_state, sandbox_index, type), RECORDER_ALLOCATOR_CAPACITY, RECORDER_BUFFERING_CAPACITY, initialize_functor);
}

bool InitializeSandboxRecordings(EditorState* editor_state, unsigned int sandbox_index, bool check_that_it_is_enabled) {
	bool success = true;
	for (size_t index = 0; index < EDITOR_SANDBOX_RECORDING_TYPE_COUNT; index++) {
		success &= InitializeSandboxRecording(editor_state, sandbox_index, (EDITOR_SANDBOX_RECORDING_TYPE)index, check_that_it_is_enabled);
	}
	return success;
}

void ResetSandboxRecording(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type) {
	ResetSandboxRecording(editor_state, sandbox_index, GetSandboxRecordingInfo(editor_state, sandbox_index, type));
}

void ResetSandboxRecordings(EditorState* editor_state, unsigned int sandbox_index) {
	for (size_t index = 0; index < EDITOR_SANDBOX_RECORDING_TYPE_COUNT; index++) {
		ResetSandboxRecording(editor_state, sandbox_index, (EDITOR_SANDBOX_RECORDING_TYPE)index);
	}
}

bool RunSandboxRecording(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type) {
	SandboxRecordingInfo info = GetSandboxRecordingInfo(editor_state, sandbox_index, type);
	if (*info.is_delta_writer_initialized) {
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		if (HasFlag(sandbox->flags, info.flag)) {
			if (!info.delta_writer->Write()) {
				ECS_FORMAT_TEMP_STRING(console_message, "Failed to write {#} recording at moment {#}.", info.type_string, sandbox->sandbox_world.elapsed_seconds);
				EditorSetConsoleWarn(console_message);
				return false;
			}
		}
	}

	return true;
}

bool RunSandboxRecordings(EditorState* editor_state, unsigned int sandbox_index) {
	bool success = true;
	for (size_t index = 0; index < EDITOR_SANDBOX_RECORDING_TYPE_COUNT; index++) {
		success &= RunSandboxRecording(editor_state, sandbox_index, (EDITOR_SANDBOX_RECORDING_TYPE)index);
	}
	return success;
}