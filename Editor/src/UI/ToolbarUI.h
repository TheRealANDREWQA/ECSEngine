#pragma once
#include "ECSEngineUI.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;

#define TOOLBAR_WINDOW_NAME "Toolbar"
#define TOOLBAR_SIZE_Y ECS_TOOLS_UI_WINDOW_LAYOUT_DEFAULT_ELEMENT_Y

enum TOOLBAR_WINDOW_MENU_INDEX {
	TOOLBAR_WINDOW_MENU_CONSOLE,
	TOOLBAR_WINDOW_MENU_DIRECTORY_EXPLORER,
	TOOLBAR_WINDOW_MENU_FILE_EXPLORER,
	TOOLBAR_WINDOW_MENU_MODULE_EXPLORER,
	TOOLBAR_WINDOW_MENU_SANDBOX_EXPLORER,
	TOOLBAR_WINDOW_MENU_INSPECTOR,
	TOOLBAR_WINDOW_MENU_BACKUPS,
	TOOLBAR_WINDOW_MENU_SANDBOX_UI,
	TOOLBAR_WINDOW_MENU_ENTITIES_UI,
	TOOLBAR_WINDOW_MENU_COUNT
};

#define TOOLBAR_WINDOWS_MENU_CHAR_DESCRIPTION "Console\nDirectory Explorer\nFile Explorer\nModule Explorer\nSandbox Explorer\nInspector\nBackups\nSandbox\nEntities"

struct EditorState;

// Stack memory size should be at least 512
void ToolbarSetDescriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory);

void ToolbarDraw(void* window_data, void* drawer_descriptor, bool initialize);

// Taking editor state as void* ptr in order to avoid including it in the header file
// and avoid recompiling when modifying the editor state
void CreateToolbarUI(EditorState* editor_state);