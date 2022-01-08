#pragma once
#include "ECSEngineUI.h"

constexpr const char* GAME_WINDOW_NAME = "Game";

struct EditorState;

// Stack memory size should be at least 512
void GameSetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void GameWindowDraw(void* window_data, void* drawer_descriptor, bool initialize);

// It creates the dockspace and the window
void CreateGame(EditorState* editor_state);

// It creates the dockspace and the window
void CreateGameAction(ECSEngine::Tools::ActionData* action_data);

// It only creates the window, it will not be assigned to any dockspace and returns the window index
unsigned int CreateGameWindow(EditorState* editor_state);