#pragma once
#include "ECSEngineCrash.h"

struct EditorState;

// This function will add static and dynamic wrappers for the task manager
// Such that if a crash has occured the threads stop immediately after finishing
// Their
void SandboxAddConcurrencyCrashWrappers(EditorState* editor_state, unsigned int sandbox_index);

// Returns the previous crash handler
ECSEngine::CrashHandler SandboxSetCrashHandler(EditorState* editor_state, unsigned int sandbox_index);

// Restores the crash handler that was before setting the sandbox crash handler
void SandboxRestorePreviousCrashHandler(ECSEngine::CrashHandler handler);