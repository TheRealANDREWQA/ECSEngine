// ECS_REFLECT
#pragma once
#include "ECSEngineReflectionMacros.h"

struct EditorState;

struct ECS_REFLECT ProjectSettings {
	// The time step is expressed in seconds
	float fixed_timestep = 0.01f;
};

// The project file must be set before calling this function
// Returns true if it succeeded, else false
bool ReadProjectSettings(EditorState* editor_state);

// Returns true if it succeeded, else false
bool WriteProjectSettings(const EditorState* editor_state);

ProjectSettings ProjectSettingsLowerBound();

ProjectSettings ProjectSettingsUpperBound();