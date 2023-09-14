#pragma once
#include "ECSEngineUI.h"

#define GAME_WINDOW_NAME "Game "
#define MAX_GAME_WINDOWS 8u

struct EditorState;

// Stack memory size should be at least 512
// In the stack memory the first 4 bytes need to be the sandbox index
void GameSetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void GameWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize);

// It creates the dockspace and the window
void CreateGameUIWindow(EditorState* editor_state, unsigned int index);

struct CreateGameUIActionData {
	EditorState* editor_state;
	unsigned int index;
};

// It creates the dockspace and the window
// Must have a CreateGameUIActionData* as data
void CreateGameUIWindowAction(ActionData* action_data);

// It only creates the window, it will not be assigned to any dockspace and returns the window index
unsigned int CreateGameUIWindowOnly(EditorState* editor_state, unsigned int index);

// Destroys all Game windows whose sandbox no longer exists
void DestroyInvalidGameUIWindows(EditorState* editor_state);

void GetGameUIWindowName(unsigned int index, ECSEngine::CapacityStream<char>& name);

unsigned int GetGameUIWindowIndex(const EditorState* editor_state, unsigned int sandbox_index);

// If the window is present, it will enable the UI rendering
void DisableGameUIRendering(EditorState* editor_state, unsigned int sandbox_index);

// If the window is present, it will enable the UI rendering
void EnableGameUIRendering(EditorState* editor_state, unsigned int sandbox_index);

// Does nothing if the old_index doesn't exist
void UpdateGameUIWindowIndex(EditorState* editor_state, unsigned int old_index, unsigned int new_index);

// Returns the target sandbox index from that window_index. If the window is not a game UI window,
// it will return -1
unsigned int GameUITargetSandbox(const EditorState* editor_state, unsigned int window_index);