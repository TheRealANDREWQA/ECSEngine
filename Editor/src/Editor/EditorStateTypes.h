#pragma once
#include <stdint.h>

enum EDITOR_LAZY_EVALUATION_COUNTERS : unsigned char {
	EDITOR_LAZY_EVALUATION_SAVE_PROJECT_UI,
	EDITOR_LAZY_EVALUATION_DIRECTORY_EXPLORER,
	EDITOR_LAZY_EVALUATION_FILE_EXPLORER_RETAINED_FILE_CHECK,
	EDITOR_LAZY_EVALUATION_FILE_EXPLORER_TEXTURES,
	EDITOR_LAZY_EVALUATION_FILE_EXPLORER_MESH_THUMBNAIL,
	EDITOR_LAZY_EVALUATION_UPDATE_MODULE_STATUS,
	EDITOR_LAZY_EVALUATION_UPDATE_GRAPHICS_MODULE_STATUS,
	EDITOR_LAZY_EVALUATION_RUNTIME_SETTINGS,
	EDITOR_LAZY_EVALUATION_METADATA_FOR_ASSETS,
	EDITOR_LAZY_EVALUATION_UPDATE_MODULE_DLL_IMPORTS,
	EDITOR_LAZY_EVALUATION_MODULE_SETTINGS_REFRESH,
	EDITOR_LAZY_EVALUATION_PREFAB_FILE_CHANGE,
	EDITOR_LAZY_EVALUATION_PREFAB_ACTIVE_IDS,
	EDITOR_LAZY_EVALUATION_COUNTERS_COUNT,
};

enum EDITOR_STATE_FLAGS : unsigned char {
	EDITOR_STATE_DO_NOT_ADD_TASKS,
	EDITOR_STATE_IS_PLAYING,
	EDITOR_STATE_IS_PAUSED,
	EDITOR_STATE_IS_STEP,
	// By default, the editor will step using the delta time
	// Of the world from the previous frame. When this is set,
	// Then it will use a fixed step to update the simulation
	EDITOR_STATE_IS_FIXED_STEP,
	EDITOR_STATE_FREEZE_TICKS,
	EDITOR_STATE_PREVENT_LAUNCH,
	// This is for runtime only - it doesn't affect the UI loads
	EDITOR_STATE_PREVENT_RESOURCE_LOADING,
	// This flag is used to know whether or not the mouse was visible
	// When the pause/unpause action was performed to restore it when the
	// Simulation is replayed
	EDITOR_STATE_PAUSE_UNPAUSE_MOUSE_IS_HIDDEN,
	// This flag is similar to the one above, but for the raw input state
	EDITOR_STATE_PAUSE_UNPAUSE_MOUSE_IS_RAW_INPUT,
	EDITOR_STATE_FLAG_COUNT
};

struct EditorState;

// These are reference counted, can be called multiple times
// Returns the flag value before the increment
size_t EditorStateSetFlag(EditorState* editor_state, EDITOR_STATE_FLAGS flag);

// These are reference counted, can be called multiple times
// Returns the flag value before the decrement
size_t EditorStateClearFlag(EditorState* editor_state, EDITOR_STATE_FLAGS flag);

// An atomic compare exchange. Returns true if it succeeded, else false
bool EditorStateTrySetFlag(EditorState* editor_state, EDITOR_STATE_FLAGS flag, size_t compare_value, size_t new_value);

bool EditorStateHasFlag(const EditorState* editor_state, EDITOR_STATE_FLAGS flag);

// Waits until the flag is set or cleared
void EditorStateWaitFlag(size_t sleep_milliseconds, const EditorState* editor_state, EDITOR_STATE_FLAGS flag, bool set = true);

// Waits until the flag reaches a certain count
void EditorStateWaitFlagCount(size_t sleep_milliseconds, const EditorState* editor_state, EDITOR_STATE_FLAGS flag, size_t count);