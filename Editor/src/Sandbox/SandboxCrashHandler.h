#pragma once
#include "ECSEngineCrash.h"
#include "ECSEngineAllocatorPolymorphic.h"

struct EditorState;

// The destructor calls SandboxRestorePreviousCrashHandler to restore the crash handler.
struct SandboxCrashHandlerStackScope : public ECSEngine::CrashHandler {
	ECS_INLINE SandboxCrashHandlerStackScope(const ECSEngine::CrashHandler& crash_handler) : ECSEngine::CrashHandler(crash_handler) {}

	SandboxCrashHandlerStackScope(SandboxCrashHandlerStackScope&& other)
	{
		function = other.function;
		data = other.data;
		other.function = nullptr;
		other.data = nullptr;
	}

	SandboxCrashHandlerStackScope& operator=(SandboxCrashHandlerStackScope&& other) {
		function = other.function;
		data = other.data;
		other.function = nullptr;
		other.data = nullptr;
		return *this;
	}

	// Once the destructor was called, it the crash handler is considered empty, a new destructor call will do nothing on it
	~SandboxCrashHandlerStackScope();
};

// It will set task manager wrappers such that threads respond to the actual crash
void SetSandboxCrashHandlerWrappers(EditorState* editor_state, unsigned int sandbox_handle);

// Returns the previous crash handler. It needs a temporary allocator to make some allocations
// The temporary allocator needs to be valid for the entire duration of the world simulation frame
SandboxCrashHandlerStackScope SandboxSetCrashHandler(
	EditorState* editor_state, 
	unsigned int sandbox_handle,
	ECSEngine::AllocatorPolymorphic temporary_allocator
);

// Restores the crash handler that was before setting the sandbox crash handler
void SandboxRestorePreviousCrashHandler(ECSEngine::CrashHandler handler);

// Returns true if the internal state is still valid, else false. It can optionally fill in
// An error message
bool SandboxValidateStateAfterCrash(EditorState* editor_state);