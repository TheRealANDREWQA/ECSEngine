#include "editorpch.h"
#include "Scene.h"
#include "../Editor/EditorState.h"
#include "../HelperWindows.h"
#include "Common.h"
#include "../Editor/EditorPalette.h"
#include "ECSEngineMath.h"
#include "ECSEngineForEach.h"
#include "../Modules/Module.h"

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

struct SceneHoverableActionData {
	EditorState* editor_state;
	unsigned int sandbox_index;
};

void SceneHoverableAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SceneHoverableActionData* data = (SceneHoverableActionData*)_data;
	if (mouse_tracker->RightButton() == MBPRESSED || mouse_tracker->MiddleButton() == MBPRESSED) {
		mouse->EnableRawInput();
	}
	else if (mouse_tracker->RightButton() == MBRELEASED || mouse_tracker->MiddleButton() == MBRELEASED) {
		mouse->DisableRawInput();
	}

	if (mouse_tracker->RightButton() == MBHELD) {
		system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
	}

	EDITOR_SANDBOX_VIEWPORT current_viewport = GetSandboxCurrentViewport(data->editor_state, data->sandbox_index);

	if (mouse_tracker->RightButton() == MBHELD) {
		float rotation_factor = 75.0f;
		float2 mouse_position = system->GetNormalizeMousePosition();
		float2 delta = system->GetMouseDelta(mouse_position);

		if (keyboard->IsKeyDown(HID::Key::LeftShift)) {
			rotation_factor = 10.0f;
		}

		float3 rotation = { delta.y * rotation_factor, delta.x * rotation_factor, 0.0f };
		// If the rotation has changed, rerender the sandbox
		if (rotation.x != 0.0f || rotation.y != 0.0f) {
			RotateSandboxCamera(data->editor_state, data->sandbox_index, rotation);
			RenderSandbox(data->editor_state, data->sandbox_index, current_viewport);
		}
	}

	if (mouse_tracker->MiddleButton() == MBHELD) {
		float2 mouse_position = system->GetNormalizeMousePosition();
		float2 delta = system->GetMouseDelta(mouse_position);

		float3 camera_rotation = GetSandboxCameraPoint(data->editor_state, data->sandbox_index).rotation;
		float3 right_vector = GetRightVector(camera_rotation);
		float3 up_vector = GetUpVector(camera_rotation);

		float translation_factor = 10.0f;

		if (keyboard->IsKeyDown(HID::Key::LeftShift)) {
			translation_factor = 2.5f;
		}

		float3 translation = right_vector * float3::Splat(-delta.x * translation_factor) + up_vector * float3::Splat(delta.y * translation_factor);
		if (translation.x != 0.0f || translation.y != 0.0f || translation.z != 0.0f) {
			TranslateSandboxCamera(data->editor_state, data->sandbox_index, translation);
			RenderSandbox(data->editor_state, data->sandbox_index, current_viewport);
		}
	}

	int scroll_delta = mouse->GetScrollDelta();
	if (scroll_delta != 0) {
		float factor = 0.015f;

		if (keyboard->IsKeyDown(HID::Key::LeftShift)) {
			factor = 0.005f;
		}

		float3 camera_rotation = GetSandboxCameraPoint(data->editor_state, data->sandbox_index).rotation;
		float3 forward_vector = GetForwardVector(camera_rotation);

		TranslateSandboxCamera(data->editor_state, data->sandbox_index, forward_vector * float3::Splat(scroll_delta * factor));
		RenderSandbox(data->editor_state, data->sandbox_index, current_viewport);
	}
}

void SceneUIWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	SceneDrawData* data = (SceneDrawData*)window_data;
	EditorState* editor_state = data->editor_state;

	unsigned int sandbox_index = GetWindowNameIndex(drawer.system->GetWindowName(drawer.window_index));
	EDITOR_SANDBOX_VIEWPORT current_viewport = GetSandboxCurrentViewport(editor_state, sandbox_index);
	if (initialize) {
		data->previous_texel_size = { 0, 0 };
	}
	else {
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		UIDrawConfig config;

		bool multiple_graphics_module = false;
		unsigned int sandbox_graphics_module_index = GetSandboxGraphicsModule(editor_state, sandbox_index, &multiple_graphics_module);
		bool has_graphics = sandbox_graphics_module_index != -1;
		if (has_graphics) {
			// Determine if the destination needs to be resized
			uint2 current_texel_size = drawer.system->GetWindowTexelSize(drawer.window_index);
			uint2 difference = Abs(current_texel_size, data->previous_texel_size);
			if (difference.x >= RESIZE_THRESHOLD || difference.y >= RESIZE_THRESHOLD) {
				// Resize the textures
				data->previous_texel_size = current_texel_size;
				ResizeSandboxRenderTextures(editor_state, sandbox_index, current_viewport, current_texel_size);

				// Now render the sandbox
				RenderSandbox(editor_state, sandbox_index, current_viewport);
			}

			UIConfigAbsoluteTransform output_transform = drawer.GetWholeRegionTransform();

			config.AddFlag(output_transform);

			D3D11_TEXTURE2D_DESC texture_desc;
			Texture2D viewport_texture = sandbox->viewport_transferred_texture[current_viewport].GetResource();
			viewport_texture.tex->GetDesc(&texture_desc);

			// Draw the output texture
			drawer.SpriteRectangle(
				UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE,
				config,
				sandbox->viewport_transferred_texture[current_viewport]
			);

			SceneHoverableActionData hoverable_data;
			hoverable_data.editor_state = editor_state;
			hoverable_data.sandbox_index = sandbox_index;

			UIActionHandler click_handler = { SceneHoverableAction, &hoverable_data, sizeof(hoverable_data) };
			// This is for translation and rotation
			drawer.SetWindowHoverable(&click_handler);

			config.flag_count = 0;

			// Determine if the graphics module is out of date to display a warning
			bool is_out_of_date = GetSandboxModuleInfo(editor_state, sandbox_index, sandbox_graphics_module_index)->load_status == EDITOR_MODULE_LOAD_OUT_OF_DATE;
			if (is_out_of_date) {
				float2 warning_scale = drawer.GetSquareScale(0.1f);

				// Display a warning in the bottom right corner
				float2 warning_position = drawer.GetCornerRectangle(ECS_UI_ALIGN_RIGHT, ECS_UI_ALIGN_BOTTOM, warning_scale);

				UIConfigAbsoluteTransform warning_transform;
				warning_transform.position = warning_position;
				warning_transform.scale = warning_scale;

				config.AddFlag(warning_transform);

				struct ClickableData {
					EditorState* editor_state;
					unsigned int module_index;
					EDITOR_MODULE_CONFIGURATION configuration;
				};

				auto warning_clickable = [](ActionData* action_data) {
					UI_UNPACK_ACTION_DATA;

					ClickableData* data = (ClickableData*)_data;
					EDITOR_LAUNCH_BUILD_COMMAND_STATUS command_status = BuildModule(data->editor_state, data->module_index, data->configuration);
					PrintConsoleMessageForBuildCommand(data->editor_state, data->module_index, data->configuration, command_status);
				};

				ClickableData clickable_data;
				clickable_data.editor_state = editor_state;
				clickable_data.module_index = sandbox->modules_in_use[sandbox_graphics_module_index].module_index;
				clickable_data.configuration = sandbox->modules_in_use[sandbox_graphics_module_index].module_configuration;
				drawer.SpriteButton(
					UI_CONFIG_ABSOLUTE_TRANSFORM | UI_CONFIG_DO_NOT_ADVANCE | UI_CONFIG_DO_NOT_VALIDATE_POSITION,
					config,
					{ warning_clickable, &clickable_data, sizeof(clickable_data) },
					ECS_TOOLS_UI_TEXTURE_WARN_ICON,
					EDITOR_YELLOW_COLOR
				);

				config.flag_count = 0;
			}
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