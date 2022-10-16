#pragma once
#include "ECSEngineUI.h"

#define GAME_WINDOW_NAME "Game "
#define MAX_GAME_WINDOWS 8

struct EditorState;

// Stack memory size should be at least 512
// In the stack memory the first 4 bytes need to be the sandbox index
void GameSetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void GameWindowDraw(void* window_data, void* drawer_descriptor, bool initialize);

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

void GetGameUIWindowName(unsigned int index, ECSEngine::CapacityStream<char>& name);

// Does nothing if the old_index doesn't exist
void UpdateGameUIWindowIndex(EditorState* editor_state, unsigned int old_index, unsigned int new_index);