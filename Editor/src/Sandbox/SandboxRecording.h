#pragma once
#include "ECSEngineContainersCommon.h"
#include "SandboxRecordingFileExtension.h"

struct EditorState;
enum EDITOR_SANDBOX_FLAG : size_t;
enum EDITOR_SANDBOX_RECORDING_TYPE : unsigned char;
namespace ECSEngine {
	struct DeltaStateWriter;
	struct World;
}

struct SandboxRecordingInfo {
	ECSEngine::DeltaStateWriter* delta_writer;
	bool* is_delta_writer_initialized;
	bool* is_recording_automatic;
	bool* is_recording_file_valid;
	unsigned int* recording_automatic_index;
	float* entire_state_tick_seconds;
	ECSEngine::CapacityStream<wchar_t>* file_path;
	const char* type_string;
	const wchar_t* extension;
	EDITOR_SANDBOX_FLAG flag;
};

SandboxRecordingInfo GetSandboxRecordingInfo(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type);

// It takes into consideration automatic recording. It will write into the storage the absolute path of the resolved
ECSEngine::Stream<wchar_t> GetSandboxRecordingFile(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type, ECSEngine::CapacityStream<wchar_t>& storage);

void DisableSandboxRecording(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type);

void EnableSandboxRecording(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type);

// It writes the remaining data to be written by the delta writer and closes the file. It will then deallocate the recorder.
// It returns true if it succeeded, else false. Outputs a console error message if it fails.
bool FinishSandboxRecording(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type, bool check_that_it_is_enabled);

// Calls this functor for each recording type. Returns true if all of them succeeded, else false.
// Outputs console error messages if a recording failed.
bool FinishSandboxRecordings(EditorState* editor_state, unsigned int sandbox_index, bool check_that_it_is_enabled);

// Initializes the writer for the given data and creates the file which will hold the recorded data
bool InitializeSandboxRecording(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type, bool check_that_it_is_enabled);

// Initializes all writer types and returns the aggregated success status
bool InitializeSandboxRecordings(EditorState* editor_state, unsigned int sandbox_index, bool check_that_it_is_enabled);

// Resets everything such that it is empty
void ResetSandboxRecording(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type);

// Calls this functor for each recording type
void ResetSandboxRecordings(EditorState* editor_state, unsigned int sandbox_index);

// Runs an iteration of the delta state writer. Should be called before the simulation execution. 
// Returns true if it succeeded, else false. Outputs a console error message if it fails.
bool RunSandboxRecording(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type);

// Calls this functor for each recording type. Returns true if all of them succeeded, else false
bool RunSandboxRecordings(EditorState* editor_state, unsigned int sandbox_index);

// Updates the valid file boolean for the given sandbox recording
void UpdateSandboxValidFileBoolRecording(EditorState* editor_state, unsigned int sandbox_index, EDITOR_SANDBOX_RECORDING_TYPE type);