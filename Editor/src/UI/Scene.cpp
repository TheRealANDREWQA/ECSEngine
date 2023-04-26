#include "editorpch.h"
#include "Scene.h"
#include "..\Editor\EditorState.h"
#include "..\HelperWindows.h"
#include "Common.h"

#include "ECSEngineMath.h"
#include "ECSEngineForEach.h"

// Have a threshold for resizing such that small resizes are not triggered when dragging the window
#define RESIZE_THRESHOLD 25

struct SceneDrawData {
	EditorState* editor_state;
	uint2 previous_texel_size;
};

void SceneUISetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory) {
	unsigned int index = *(unsigned int*)stack_memory;

	SceneDrawData* data = (SceneDrawData*)function::OffsetPointer(stack_memory, sizeof(unsigned int));

	memset(data, 0, sizeof(*data));
	data->editor_state = editor_state;

	descriptor.draw = SceneUIWindowDraw;

	CapacityStream<char> window_name(function::OffsetPointer(data, sizeof(*data)), 0, 128);
	GetSceneUIWindowName(index, window_name);

	descriptor.window_name = window_name;
	descriptor.window_data = data;
	descriptor.window_data_size = sizeof(*data);
}

struct SceneClickActionData {

};

void SceneClickAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SceneClickActionData* data = (SceneClickActionData*)_data;
}

void SceneUIWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	SceneDrawData* data = (SceneDrawData*)window_data;
	EditorState* editor_state = data->editor_state;

	unsigned int sandbox_index = GetWindowNameIndex(drawer.system->GetWindowName(drawer.window_index));
	if (initialize) {
		data->previous_texel_size = { 0, 0 };
	}
	else {
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		UIDrawConfig config;

		bool multiple_graphics_module = false;
		bool has_graphics = GetSandboxGraphicsModule(editor_state, sandbox_index, &multiple_graphics_module) != -1;
		if (has_graphics) {
			// Determine if the destination needs to be resized
			uint2 current_texel_size = drawer.system->GetWindowTexelSize(drawer.window_index);
			uint2 difference = Abs(current_texel_size, data->previous_texel_size);
			if (difference.x >= RESIZE_THRESHOLD || difference.y >= RESIZE_THRESHOLD) {
				// Resize the textures
				data->previous_texel_size = current_texel_size;
				ResizeSandboxRenderTextures(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE, current_texel_size);

				// Now render the sandbox
				RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
			}

			UIConfigAbsoluteTransform output_transform = drawer.GetWholeRegionTransform();

			config.AddFlag(output_transform);

			SceneClickActionData click_data;

			D3D11_TEXTURE2D_DESC texture_desc;
			Texture2D viewport_texture = sandbox->viewport_transferred_texture[0].GetResource();
			viewport_texture.tex->GetDesc(&texture_desc);

			// Draw the output texture
			drawer.SpriteRectangle(
				UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE,
				config,
				sandbox->viewport_transferred_texture[EDITOR_SANDBOX_VIEWPORT_SCENE]
			);

			UIActionHandler click_handler = { SceneClickAction, &click_data, sizeof(click_data) };
			drawer.SetWindowClickable(&click_handler);
		}
		else {
			UIDrawerRowLayout row_layout = drawer.GenerateRowLayout();
			row_layout.SetHorizontalAlignment(ECS_UI_ALIGN_MIDDLE);
			row_layout.SetVerticalAlignment(ECS_UI_ALIGN_MIDDLE);

			Stream<char> message = multiple_graphics_module ? "Multiple Graphics Modules Assigned" : "No Graphics Module Assigned";
			row_layout.AddLabel(message);

			size_t configuration = 0;
			row_layout.GetTransform(config, configuration);

			drawer.Text(configuration, config, message);
		}
	}
}

void CreateSceneUIWindow(EditorState* editor_state, unsigned int index) {
	CreateDockspaceFromWindowWithIndex(SCENE_WINDOW_NAME, editor_state, index, CreateSceneUIWindowOnly);
}

void CreateSceneUIWindowAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	CreateSceneUIWindowActionData* data = (CreateSceneUIWindowActionData*)_data;
	CreateSceneUIWindow(data->editor_state, data->index);
}

unsigned int CreateSceneUIWindowOnly(EditorState* editor_state, unsigned int index) {
	return CreateDefaultWindowWithIndex(SCENE_WINDOW_NAME, editor_state, { 0.7f, 0.8f }, index, SceneUISetDecriptor);
}

void DestroyInvalidSceneUIWindows(EditorState* editor_state)
{
	DestroyIndexedWindows(editor_state, SCENE_WINDOW_NAME, editor_state->sandboxes.size, MAX_SCENE_WINDOWS);
}

void GetSceneUIWindowName(unsigned int index, CapacityStream<char>& name) {
	name.Copy(SCENE_WINDOW_NAME);
	function::ConvertIntToChars(name, index);
}

void UpdateSceneUIWindowIndex(EditorState* editor_state, unsigned int old_index, unsigned int new_index) {
	UpdateUIWindowIndex(editor_state, SCENE_WINDOW_NAME, old_index, new_index);
}