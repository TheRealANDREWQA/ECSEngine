#pragma once
#include "ECSEngineUI.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;

constexpr const char* TOOLBAR_WINDOW_NAME = "Toolbar";
constexpr float TOOLBAR_SIZE_Y = ECS_TOOLS_UI_WINDOW_LAYOUT_DEFAULT_ELEMENT_Y;

enum TOOLBAR_WINDOW_MENU_INDEX {
	TOOLBAR_WINDOW_MENU_INJECT_WINDOW,
	TOOLBAR_WINDOW_MENU_GAME,
	TOOLBAR_WINDOW_MENU_CONSOLE,
	TOOLBAR_WINDOW_MENU_DIRECTORY_EXPLORER,
	TOOLBAR_WINDOW_MENU_FILE_EXPLORER,
	TOOLBAR_WINDOW_MENU_MODULE_EXPLORER,
	TOOLBAR_WINDOW_MENU_INSPECTOR,
	TOOLBAR_WINDOW_MENU_COUNT
};

#define TOOLBAR_WINDOWS_MENU_CHAR_DESCRIPTION "Inject Window\nGame\nConsole\nDirectory Explorer\nFile Explorer\nModule Explorer\nInspector"

struct EditorState;

// Stack memory size should be at least 512
void ToolbarSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

template<bool initialize>
void ToolbarDraw(void* window_data, void* drawer_descriptor);

// Taking editor state as void* ptr in order to avoid including it in the header file
// and avoid recompiling when modifying the editor state
void CreateToolbarUI(EditorState* editor_state);