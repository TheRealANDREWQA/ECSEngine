#include "editorpch.h"
#include "Game.h"
#include "..\Editor\EditorState.h"
#include "..\HelperWindows.h"
#include "Common.h"
#include "RenderingCommon.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;

struct GameData {
	EditorState* editor_state;
	uint2 previous_size;
};

void GameWindowDestroy(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	unsigned int sandbox_index = GetWindowNameIndex(system->GetWindowName(system->GetWindowIndexFromBorder(dockspace, border_index)));
	DisableSandboxViewportRendering(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
}

#define RESIZE_THRESHOLD 1

void GameWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	GameData* data = (GameData*)window_data;
	EditorState* editor_state = data->editor_state;

	// Destroy the window if the sandbox index is invalid
	unsigned int sandbox_index = GetWindowNameIndex(drawer.system->GetWindowName(drawer.window_index));
	EDITOR_SANDBOX_VIEWPORT viewport = EDITOR_SANDBOX_VIEWPORT_RUNTIME;
	UIDrawConfig config;

	if (initialize) {
		// Enable the viewport for this window
		data->previous_size = { 0,0 };
		EnableSandboxViewportRendering(editor_state, sandbox_index, viewport);
	}
	else {
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);

		bool multiple_graphics_module = false;
		unsigned int sandbox_graphics_module_index = GetSandboxGraphicsModule(editor_state, sandbox_index, &multiple_graphics_module);
		bool has_graphics = sandbox_graphics_module_index != -1 && !multiple_graphics_module;
		if (has_graphics) {
			ResizeSandboxTextures(editor_state, drawer, viewport, &data->previous_size, 1, sandbox_index);
			DisplaySandboxTexture(editor_state, drawer, viewport, sandbox_index);
			DisplayGraphicsModuleRecompilationWarning(editor_state, sandbox_index, sandbox_graphics_module_index, drawer);
		}
		else {
			DisplayNoGraphicsModule(drawer, multiple_graphics_module);
		}
	}
}

void GameSetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
{
	unsigned int index = *(unsigned int*)stack_memory;

	GameData* game_data = (GameData*)function::OffsetPointer(stack_memory, sizeof(unsigned int));
	
	memset(game_data, 0, sizeof(*game_data));
	game_data->editor_state = editor_state;

	descriptor.draw = GameWindowDraw;

	descriptor.destroy_action = GameWindowDestroy;
	descriptor.destroy_action_data = editor_state;

	CapacityStream<char> window_name(function::OffsetPointer(game_data, sizeof(*game_data)), 0, 128);
	GetGameUIWindowName(index, window_name);

	descriptor.window_name = window_name;
	descriptor.window_data = game_data;
	descriptor.window_data_size = sizeof(*game_data);
}

void CreateGameUIWindow(EditorState* editor_state, unsigned int index) {
	CreateDockspaceFromWindowWithIndex(GAME_WINDOW_NAME, editor_state, index, CreateGameUIWindowOnly);
}

void CreateGameUIWindowAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreateGameUIActionData* data = (CreateGameUIActionData*)_data;
	CreateGameUIWindow(data->editor_state, data->index);
}

unsigned int CreateGameUIWindowOnly(EditorState* editor_state, unsigned int index) {
	return CreateDefaultWindowWithIndex(GAME_WINDOW_NAME, editor_state, { 0.7f, 0.8f }, index, GameSetDecriptor);
}

void DestroyInvalidGameUIWindows(EditorState* editor_state)
{
	DestroyIndexedWindows(editor_state, GAME_WINDOW_NAME, editor_state->sandboxes.size, MAX_GAME_WINDOWS);
}

void GetGameUIWindowName(unsigned int index, CapacityStream<char>& name)
{
	name.Copy(GAME_WINDOW_NAME);
	function::ConvertIntToChars(name, index);
}

void UpdateGameUIWindowIndex(EditorState* editor_state, unsigned int old_index, unsigned int new_index) {
	UpdateUIWindowIndex(editor_state, GAME_WINDOW_NAME, old_index, new_index);
}
