#include "editorpch.h"
#include "Game.h"
#include "..\Editor\EditorState.h"
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
	EditorState* editor_state = data->editor_state;

	unsigned int sandbox_index = GetWindowNameIndex(drawer.system->GetWindowName(drawer.window_index));

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
	unsigned int index = *(unsigned int*)stack_memory;

	GameData* game_data = (GameData*)function::OffsetPointer(stack_memory, sizeof(unsigned int));
	
	memset(game_data, 0, sizeof(*game_data));
	game_data->editor_state = editor_state;

	descriptor.draw = GameWindowDraw;
	descriptor.private_action = GamePrivateAction;

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

void GetGameUIWindowName(unsigned int index, CapacityStream<char>& name)
{
	name.Copy(GAME_WINDOW_NAME);
	function::ConvertIntToChars(name, index);
}

void UpdateGameUIWindowIndex(EditorState* editor_state, unsigned int old_index, unsigned int new_index) {
	UpdateUIWindowIndex(editor_state, GAME_WINDOW_NAME, old_index, new_index);
}
