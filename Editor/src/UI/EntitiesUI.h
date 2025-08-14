#pragma once
#include "ECSEngineUI.h"

#define ENTITIES_UI_WINDOW_NAME "Entities "
#define MAX_ENTITIES_UI_WINDOWS EDITOR_MAX_SANDBOX_COUNT

struct EditorState;
ECS_TOOLS;

// Stack memory size should be at least 512
// In the stack memory the first 4 bytes need to be the entities window index
void EntitiesUISetDescriptor(ECSEngine::Tools::UIWindowDescriptor& descriptor, EditorState* editor_state, CapacityStream<void>* stack_memory);

void EntitiesUIDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

// It creates the dockspace and the window
// Returns the window index
unsigned int CreateEntitiesUI(EditorState* editor_state);

// It creates the dockspace and the window
// The data pointer needs to be EditorState*
void CreateEntitiesUIAction(ECSEngine::Tools::ActionData* action_data);

// It only creates the window, it will not be assigned to any dockspace and returns the window index
unsigned int CreateEntitiesUIWindow(EditorState* editor_state, unsigned int entities_window_index_index);

void GetEntitiesUIWindowName(unsigned int entities_window_index, ECSEngine::CapacityStream<char>& name);

// It returns the index of the sandbox UI window or -1 if it doesn't exist
unsigned int GetEntitiesUIWindowIndex(const EditorState* editor_state, unsigned int entities_window_index);

// Returns the window index from the name
unsigned int GetEntitiesUIIndexFromName(ECSEngine::Stream<char> name);

// Returns -1 if no Entities UI window is created
unsigned int GetEntitiesUILastWindowIndex(const EditorState* editor_state);

// The index is the actual ordinal value, not the index from the UISystem
unsigned int GetEntitiesUITargetSandbox(const EditorState* editor_state, unsigned int entities_index);

// Retargets all EntitiesUI windows that have the target sandbox the provided handle value to the first valid sandbox value
void ResetEntitiesUITargetSandbox(EditorState* editor_state, unsigned int sandbox_handle);

// Returns the target sandbox index if the given UI window is an EntitiesUI window, else -1
unsigned int EntitiesUITargetSandbox(const EditorState* editor_state, unsigned int window_index);

// Window index is the UI index
void SetEntitiesUITargetSandbox(const EditorState* editor_state, unsigned int window_index, unsigned int target_sandbox);