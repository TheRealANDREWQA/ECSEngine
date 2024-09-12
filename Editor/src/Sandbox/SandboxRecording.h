#pragma once
#include "ECSEngineContainersCommon.h"

struct EditorState;

void ChangeSandboxInputRecordingFile(EditorState* editor_state, unsigned int sandbox_index, ECSEngine::Stream<wchar_t> file);

void DeallocateSandboxInputRecording(EditorState* editor_state, unsigned int sandbox_index);

void DisableSandboxInputRecording(EditorState* editor_state, unsigned int sandbox_index);

void EnableSandboxInputRecording(EditorState* editor_state, unsigned int sandbox_index);

// Finalizes the input recording file - returns true if it succeeded, else false
bool FinishSandboxInputRecording(EditorState* editor_state, unsigned int sandbox_index);

// Returns true if it managed to initialize the input recording, else false. It can fail if it cannot open the given file
bool InitializeSandboxInputRecording(EditorState* editor_state, unsigned int sandbox_index);