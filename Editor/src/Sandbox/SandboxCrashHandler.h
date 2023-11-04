#pragma once
#include "ECSEngineCrash.h"

struct EditorState;

// Returns the previous crash handler
ECSEngine::CrashHandler SandboxSetCrashHandler(EditorState* editor_state, unsigned int sandbox_index);

// Restores the crash handler that was before setting the sandbox crash handler
void SandboxRestorePreviousCrashHandler(ECSEngine::CrashHandler handler);