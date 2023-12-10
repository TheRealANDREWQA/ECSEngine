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
	// This is a one off flag. Used when creating the dedicated GPU device
	// such that if it is the first time a device is created on a dedicated GPU
	// and it is asleep (like it a laptop) then it will do the creation deferred
	// in another thread and this signals when that creation has finished (this
	// usually takes 1 second and if the user doesn't open up a project quicker than
	// this then there should be no problem.
	EDITOR_STATE_RUNTIME_GRAPHICS_INITIALIZATION_FINISHED,
	EDITOR_STATE_FLAG_COUNT
};

struct EditorState;

// These are reference counted, can be called multiple times
void EditorStateSetFlag(EditorState* editor_state, EDITOR_STATE_FLAGS flag);

// These are reference counted, can be called multiple times
void EditorStateClearFlag(EditorState* editor_state, EDITOR_STATE_FLAGS flag);

bool EditorStateHasFlag(const EditorState* editor_state, EDITOR_STATE_FLAGS flag);

// Waits until the flag is set or cleared
void EditorStateWaitFlag(size_t sleep_milliseconds, const EditorState* editor_state, EDITOR_STATE_FLAGS flag, bool set = true);