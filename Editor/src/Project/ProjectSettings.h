// ECS_REFLECT
#pragma once
#include "ECSEngineReflectionMacros.h"

struct EditorState;

struct ECS_REFLECT ProjectSettings {
	// The time step is expressed in seconds
	float fixed_timestep = 0.01f;
	// If set to true, the input of one sandbox is used
	// To update all of them
	bool synchronized_sandbox_input = false;
	// If set to true, then the input of the keyboard is updated
	// When the sandbox is not focused
	bool unfocused_keyboard_input = false;
};

// The project file must be set before calling this function
// Returns true if it succeeded, else false
bool ReadProjectSettings(EditorState* editor_state);

// Returns true if it succeeded, else false
bool WriteProjectSettings(const EditorState* editor_state);

ProjectSettings ProjectSettingsLowerBound();

ProjectSettings ProjectSettingsUpperBound();