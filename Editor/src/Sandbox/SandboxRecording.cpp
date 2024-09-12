#include "editorpch.h"
#include "SandboxRecording.h"
#include "SandboxAccessor.h"

#define INPUT_RECORDER_ALLOCATOR_CAPACITY ECS_MB * 32
#define INPUT_RECORDER_BUFFERING_CAPACITY ECS_MB

void ChangeSandboxInputRecordingFile(EditorState* editor_state, unsigned int sandbox_index, Stream<wchar_t> file) {

}

void DeallocateSandboxInputRecording(EditorState* editor_state, unsigned int sandbox_index) {
	// Just deallocate the recorder allocator - all else will be deallocated as well
	// The file should have been closed
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	FreeAllocator(sandbox->input_recorder.allocator);
	ZeroOut(&sandbox->input_recorder);
}

void DisableSandboxInputRecording(EditorState* editor_state, unsigned int sandbox_index) {

}

void EnableSandboxInputRecording(EditorState* editor_state, unsigned int sandbox_index) {

}

bool InitializeSandboxInputRecording(EditorState* editor_state, unsigned int sandbox_index) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	
	// Open the file for write
	ECS_FILE_HANDLE input_file = -1;
	ECS_STACK_CAPACITY_STREAM(char, error_message, ECS_KB);
	ECS_FILE_STATUS_FLAGS status = OpenFile(sandbox->input_recorder_file, &input_file, ECS_FILE_ACCESS_WRITE_BINARY_TRUNCATE, &error_message);
	if (status != ECS_FILE_STATUS_OK) {
		ECS_FORMAT_TEMP_STRING(console_message, "Opening input recording file {#} failed. Reason: {#}.", sandbox->input_recorder_file, error_message);
		EditorSetConsoleError(console_message);
		return false;
	}

	// Create an allocator just for the recorder, such that we can wink it at the end
	GlobalMemoryManager* sandbox_allocator = sandbox->GlobalMemoryManager();
	MemoryManager* input_recorder_allocator = (MemoryManager*)sandbox_allocator->Allocate(sizeof(MemoryManager));
	*input_recorder_allocator = MemoryManager(INPUT_RECORDER_ALLOCATOR_CAPACITY, ECS_KB * 4, INPUT_RECORDER_ALLOCATOR_CAPACITY, sandbox_allocator);

	// Allocate the write instrument out of it
	CapacityStream<void> write_instrument_buffering;
	write_instrument_buffering.Initialize(input_recorder_allocator, INPUT_RECORDER_BUFFERING_CAPACITY);

	BufferedFileWriteInstrument* write_instrument = (BufferedFileWriteInstrument*)input_recorder_allocator->Allocate(sizeof(BufferedFileWriteInstrument));
	*write_instrument = BufferedFileWriteInstrument(input_file, write_instrument_buffering, 0);

	ECS_STACK_VOID_STREAM(stack_memory, ECS_KB * 32);
	DeltaStateWriterInitializeInfo initialize_info;
	SetInputDeltaWriterWorldInitializeInfo(initialize_info.functor_info, &sandbox->sandbox_world, stack_memory);

	initialize_info.write_instrument = write_instrument;
	initialize_info.allocator = input_recorder_allocator;
	initialize_info.entire_state_write_seconds_tick = sandbox->input_recorder_entire_state_tick_seconds;

	sandbox->input_recorder.Initialize(initialize_info);
	return true;
}