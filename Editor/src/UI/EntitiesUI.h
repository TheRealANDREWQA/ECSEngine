#pragma once
#include "ECSEngineUI.h"

#define ENTITIES_UI_WINDOW_NAME "Entities "
#define MAX_ENTITIES_UI_WINDOWS 8

struct EditorState;

// Stack memory size should be at least 512
// In the stack memory the first 4 bytes need to be the entities window index
void EntitiesUISetDescriptor(ECSEngine::Tools::UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void EntitiesUIDraw(void* window_data, void* drawer_descriptor, bool initialize);

// It creates the dockspace and the window
// A new window is created.
void CreateEntitiesUI(EditorState* editor_state);

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

// Does nothing if the old_index doesn't exist. Changes all Entities windows which targeted the old index
// into the new index
void UpdateEntitiesUITargetSandbox(EditorState* editor_state, unsigned int old_index, unsigned int new_index);