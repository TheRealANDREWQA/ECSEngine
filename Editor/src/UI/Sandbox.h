#pragma once
#include "ECSEngineUI.h"

constexpr const char* SANDBOX_UI_WINDOW_NAME = "Sandbox ";

struct EditorState;

// Stack memory size should be at least 512
// In the stack memory the first 4 bytes need to be the sandbox index
void SandboxUISetDescriptor(ECSEngine::Tools::UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void SandboxUIDraw(void* window_data, void* drawer_descriptor, bool initialize);

// It creates the dockspace and the window
void CreateSandboxUI(EditorState* editor_state, unsigned int sandbox_index);

struct CreateSandboxUIActionData {
	EditorState* editor_state;
	unsigned int sandbox_index;
};

// It creates the dockspace and the window
// It needs in the data field a CreateSandboxUIActionData*
void CreateSandboxUIAction(ECSEngine::Tools::ActionData* action_data);

// It only creates the window, it will not be assigned to any dockspace and returns the window index
unsigned int CreateSandboxUIWindow(EditorState* editor_state, unsigned int sandbox_index);

void GetSandboxUIWindowName(unsigned int sandbox_index, ECSEngine::CapacityStream<char>& name);

// It returns the index of the sandbox UI window or -1 if it doesn't exist
unsigned int GetSandboxUIWindowIndex(const EditorState* editor_state, unsigned int sandbox_index);

// Returns the sandbox index from the name
unsigned int GetSandboxUIIndexFromName(ECSEngine::Stream<char> name);

// Does nothing if the old_index doesn't exist
void UpdateSandboxUIWindowIndex(EditorState* editor_state, unsigned int old_index, unsigned int new_index);