#pragma once
#include "ECSEngineContainersCommon.h"
#include "SandboxRecordingFileExtension.h"
#include "SandboxTypes.h"

struct EditorState;
enum EDITOR_SANDBOX_FLAG : size_t;
enum EDITOR_SANDBOX_RECORDING_TYPE : unsigned char;
namespace ECSEngine {
	struct DeltaStateReader;
	struct World;
}

struct SandboxReplayInfo {
	EditorSandbox::ReplayPlayer* replay;
	const char* type_string;
	const wchar_t* extension;
	EDITOR_SANDBOX_FLAG flag;
};

SandboxReplayInfo GetSandboxReplayInfo(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type);

// It will write into the storage the absolute path of the resolved
ECSEngine::Stream<wchar_t> GetSandboxReplayFile(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type, ECSEngine::CapacityStream<wchar_t>& storage);

// If the replay is initialized, it will release any resources that it uses and reset it such that it is not initialized any more,
// While maintaining the existing file path.
void DeallocateSandboxReplay(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type);

// It will call the deallocate function for each individual replay type
void DeallocateSandboxReplays(EditorState* editor_state, unsigned int sandbox_handle);

void DisableSandboxReplay(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type);

void EnableSandboxReplay(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type);

// Returns true if the replay type is enabled (not necessarily that it is active at this moment), else false
bool IsSandboxReplayEnabled(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type);

// Initializes the reader for the given replay type
bool InitializeSandboxReplay(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type, bool check_that_it_is_enabled);

// Initializes all reader replay types and returns the aggregated success status
bool InitializeSandboxReplays(EditorState* editor_state, unsigned int sandbox_handle, bool check_that_it_is_enabled);

// Resets everything such that it is empty. It does not deallocate the reader in case it is initialized, it will overwrite it.
void ResetSandboxReplay(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type);

// Calls this functor for each replay type. It does not deallocate the reader in case it is initialized, it will overwrite it.
void ResetSandboxReplays(EditorState* editor_state, unsigned int sandbox_handle);

// Calls the reader to resolve the data inputs for the current simulation time. Should be called before the simulation execution. 
// Returns true if it succeeded, else false. Outputs a console error message if it fails
bool RunSandboxReplay(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type);

// Calls this functor for each replay type. Returns true if all of them succeeded, else false
bool RunSandboxReplays(EditorState* editor_state, unsigned int sandbox_handle);

// It will deduce the replay type from the extension. The last parameter is used to know whether the replay should be initialized
// Right now in this call, which can be helpful if you want to ensure the file is valid.
// Returns the recording type of the replay if it succeeded, else COUNT if it failed to read the file or it is corrupted
EDITOR_SANDBOX_RECORDING_TYPE SetSandboxReplay(EditorState* editor_state, unsigned int sandbox_handle, ECSEngine::Stream<wchar_t> replay_file, bool initialize_now);

// Updates the valid file boolean for the given sandbox recording
void UpdateSandboxValidFileBoolReplay(EditorState* editor_state, unsigned int sandbox_handle, EDITOR_SANDBOX_RECORDING_TYPE type);