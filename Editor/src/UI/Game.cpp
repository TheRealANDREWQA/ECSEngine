#include "editorpch.h"
#include "Game.h"
#include "..\Editor\EditorState.h"
#include "ToolbarUI.h"
#include "MiscellaneousBar.h"
#include "..\HelperWindows.h"

using namespace ECSEngine;
using namespace ECSEngine::Tools;

constexpr size_t GAME_CAMERA_COUNT = 5;
constexpr const char* GAME_RESOURCE_STRING = "RESOURCE STRING";

constexpr ECSEngine::float2 GAME_ROTATE_SENSITIVITY = { 30.0f, 30.0f };
constexpr float GAME_SCROLL_SENSITIVITY = 0.015f;
constexpr ECSEngine::float2 GAME_PAN_SENSITIVITY = { 10.0f, 10.0f };

struct GameData {
	EditorState* editor_state;
	Camera cameras[GAME_CAMERA_COUNT];
	unsigned int active_camera;
};

void GameWindowDraw(void* window_data, void* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	GameData* data = (GameData*)window_data;
	EDITOR_STATE(data->editor_state);

	if (!initialize) {
		//drawer.system->SetSprite(drawer.dockspace, drawer.border_index, data->editor_state->viewport_texture, drawer.region_position, drawer.region_scale, drawer.buffers, drawer.counts);
	}
}

void GamePrivateAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	GameData* data = (GameData*)_additional_data;
	
	Camera* camera = data->cameras + data->active_camera;
	const HID::MouseState* mouse_state = mouse->GetState();

	if (mouse_state->MiddleButton()) {
		float3 right_vector = GetRightVector(camera->rotation);
		float3 up_vector = GetUpVector(camera->rotation);

		camera->translation -= right_vector * float3::Splat(mouse_delta.x) * float3::Splat(GAME_PAN_SENSITIVITY.x) - up_vector * float3::Splat(mouse_delta.y) * float3::Splat(GAME_PAN_SENSITIVITY.y);
	}

	if (mouse_state->RightButton()) {
		camera->rotation.x += mouse_delta.y * GAME_ROTATE_SENSITIVITY.y;
		camera->rotation.y += mouse_delta.x * GAME_ROTATE_SENSITIVITY.x;
	}

	int scroll_delta = mouse_state->ScrollDelta();
	if (scroll_delta != 0) {
		float3 forward_vector = GetForwardVector(camera->rotation);

		camera->translation += forward_vector * float3::Splat(scroll_delta * GAME_SCROLL_SENSITIVITY);
	}
}

void GameSetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory)
{
	GameData* game_data = (GameData*)stack_memory;
	
	memset(game_data, 0, sizeof(*game_data));
	game_data->editor_state = editor_state;

	descriptor.draw = GameWindowDraw;

	descriptor.window_name = GAME_WINDOW_NAME;
	descriptor.window_data = game_data;
	descriptor.window_data_size = sizeof(*game_data);
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

	return CreateDefaultWindow(GAME_WINDOW_NAME, editor_state, window_size, GameWindowDraw);
}
