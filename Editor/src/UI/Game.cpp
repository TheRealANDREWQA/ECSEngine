#include "Game.h"
#include "..\Editor\EditorState.h"
#include "ToolbarUI.h"
#include "MiscellaneousBar.h"
#include "..\HelperWindows.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;

template<bool initialize>
void GameWindowDraw(void* window_data, void* drawer_descriptor) {
	UI_PREPARE_DRAWER(initialize);

	EDITOR_STATE(window_data);

	if (!initialize) {
		drawer.system->SetSprite(drawer.dockspace, drawer.border_index, viewport_texture, drawer.region_position, drawer.region_scale, drawer.buffers, drawer.counts);
	}
}

ECS_TEMPLATE_FUNCTION_BOOL_NO_API(void, GameWindowDraw, void*, void*);

void GameSetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
{
	descriptor.draw = GameWindowDraw<false>;
	descriptor.initialize = GameWindowDraw<true>;

	descriptor.window_name = GAME_WINDOW_NAME;
	descriptor.window_data = editor_state;
	descriptor.window_data_size = 0;
}

void CreateGame(EditorState* editor_state) {
	CreateDockspaceFromWindow(GAME_WINDOW_NAME, editor_state, CreateGameWindow);
}

void CreateGameAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreateGame((EditorState*)action_data->data);
}

unsigned int CreateGameWindow(EditorState* editor_state) {
	EDITOR_STATE(editor_state);

	float window_size_y = 0.7f;
	float2 window_size = ui_system->GetSquareScale(window_size_y);

	return CreateDefaultWindow(GAME_WINDOW_NAME, editor_state, window_size, GameWindowDraw<false>, GameWindowDraw<true>);
}
