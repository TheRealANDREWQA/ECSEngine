#pragma once
#include "ECSEngineCrash.h"
#include "ECSEngineAllocatorPolymorphic.h"

struct EditorState;

// It will set task manager wrappers such that threads respond to the actual crash
void SetSandboxCrashHandlerWrappers(EditorState* editor_state, unsigned int sandbox_index);

// Returns the previous crash handler. It needs a temporary allocator to make some allocations
// The temporary allocator needs to be valid for the entire duration of the world simulation frame
ECSEngine::CrashHandler SandboxSetCrashHandler(
	EditorState* editor_state, 
	unsigned int sandbox_index,
	ECSEngine::AllocatorPolymorphic temporary_allocator
);

// Restores the crash handler that was before setting the sandbox crash handler
void SandboxRestorePreviousCrashHandler(ECSEngine::CrashHandler handler);

// Returns true if the internal state is still valid, else false. It can optionally fill in
// An error message
bool SandboxValidateStateAfterCrash(EditorState* editor_state);