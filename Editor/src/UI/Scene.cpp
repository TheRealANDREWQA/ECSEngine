#include "editorpch.h"
#include "Scene.h"
#include "../Editor/EditorState.h"
#include "HelperWindows.h"
#include "Common.h"
#include "../Editor/EditorPalette.h"
#include "ECSEngineMath.h"
#include "ECSEngineForEach.h"
#include "../Modules/Module.h"
#include "RenderingCommon.h"
#include "../Editor/EditorSandboxEntityOperations.h"
#include "ECSEngineSampleTexture.h"

struct SceneDrawData {
	EditorState* editor_state;
	uint2 previous_texel_size;
};

void SceneUIDestroy(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	// Determine the sandbox index
	unsigned int sandbox_index = GetWindowNameIndex(system->GetWindowName(system->GetWindowIndexFromBorder(dockspace, border_index)));
	// Disable the viewport rendering
	DisableSandboxViewportRendering(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
}

void SceneUISetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory) {
	unsigned int index = *(unsigned int*)stack_memory;

	SceneDrawData* data = (SceneDrawData*)function::OffsetPointer(stack_memory, sizeof(unsigned int));

	memset(data, 0, sizeof(*data));
	data->editor_state = editor_state;

	descriptor.draw = SceneUIWindowDraw;

	descriptor.destroy_action = SceneUIDestroy;
	descriptor.destroy_action_data = editor_state;

	CapacityStream<char> window_name(function::OffsetPointer(data, sizeof(*data)), 0, 128);
	GetSceneUIWindowName(index, window_name);

	descriptor.window_name = window_name;
	descriptor.window_data = data;
	descriptor.window_data_size = sizeof(*data);
}

struct SceneActionData {
	ECS_INLINE bool IsTheSameData(const SceneActionData* other) const {
		return other != nullptr && sandbox_index == other->sandbox_index;
	}

	SceneDrawData* draw_data;
	unsigned int sandbox_index;
};

void SceneRotationAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SceneActionData* data = (SceneActionData*)_data;
	if (mouse->IsPressed(ECS_MOUSE_RIGHT)) {
		mouse->EnableRawInput();
	}
	else if (mouse->IsReleased(ECS_MOUSE_RIGHT)) {
		mouse->DisableRawInput();
	}
	else if (mouse->IsDown(ECS_MOUSE_RIGHT)) {
		system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
	}

	EditorState* editor_state = data->draw_data->editor_state;

	EDITOR_SANDBOX_VIEWPORT current_viewport = GetSandboxCurrentViewport(editor_state, data->sandbox_index);

	if (mouse->IsDown(ECS_MOUSE_RIGHT)) {
		float rotation_factor = 75.0f;
		float2 mouse_position = system->GetNormalizeMousePosition();
		float2 delta = system->GetMouseDelta(mouse_position);

		if (keyboard->IsDown(ECS_KEY_LEFT_SHIFT)) {
			rotation_factor = 15.0f;
		}
		else if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
			rotation_factor = 200.0f;
		}

		float3 rotation = { delta.y * rotation_factor, delta.x * rotation_factor, 0.0f };
		// If the rotation has changed, rerender the sandbox
		if (rotation.x != 0.0f || rotation.y != 0.0f) {
			RotateSandboxCamera(editor_state, data->sandbox_index, rotation);
			RenderSandbox(editor_state, data->sandbox_index, current_viewport);
		}
	}
}

void SceneTranslationAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SceneActionData* data = (SceneActionData*)_data;
	if (mouse->IsPressed(ECS_MOUSE_MIDDLE)) {
		mouse->EnableRawInput();
	}
	else if (mouse->IsReleased(ECS_MOUSE_MIDDLE)) {
		mouse->DisableRawInput();
	}
	else if (mouse->IsDown(ECS_MOUSE_MIDDLE)) {
		system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
	}

	EditorState* editor_state = data->draw_data->editor_state;
	EDITOR_SANDBOX_VIEWPORT current_viewport = GetSandboxCurrentViewport(editor_state, data->sandbox_index);

	if (mouse->IsDown(ECS_MOUSE_MIDDLE)) {
		float2 mouse_position = system->GetNormalizeMousePosition();
		float2 delta = system->GetMouseDelta(mouse_position);

		float3 camera_rotation = GetSandboxCameraPoint(editor_state, data->sandbox_index).rotation;
		float3 right_vector = GetRightVector(camera_rotation);
		float3 up_vector = GetUpVector(camera_rotation);

		float translation_factor = 10.0f;

		if (keyboard->IsDown(ECS_KEY_LEFT_SHIFT)) {
			translation_factor = 2.5f;
		}
		else if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
			translation_factor = 30.0f;
		}

		float3 translation = right_vector * float3::Splat(-delta.x * translation_factor) + up_vector * float3::Splat(delta.y * translation_factor);
		if (translation.x != 0.0f || translation.y != 0.0f || translation.z != 0.0f) {
			TranslateSandboxCamera(editor_state, data->sandbox_index, translation);
			RenderSandbox(editor_state, data->sandbox_index, current_viewport);
		}
	}
}

// Returns true if the position has changed and it is preventing from using the RuntimeGraphics()
// Else false
bool DetermineHoveredElement(SceneDrawData* data, unsigned int sandbox_index, uint2 hovered_texel_offset, Entity* hovered_entity) {
	// We need the immediate context - if there is no one loading something, then perform the detection
	if (!EditorStateHasFlag(data->editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
		// Redermine the hovered entity
		RenderTargetView instanced_framebuffer = GetSandboxInstancedFramebuffer(data->editor_state, sandbox_index);
		// The instance identifier is the entity index at the moment
		unsigned int instance_identifier = GetInstanceFromFramebuffer(data->editor_state->RuntimeGraphics(), instanced_framebuffer, hovered_texel_offset);
		*hovered_entity = { instance_identifier };

		return false;
	}
	else {
		return true;
	}
}

void SceneZoomAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SceneActionData* data = (SceneActionData*)_data;

	EditorState* editor_state = data->draw_data->editor_state;
	
	// Zoom start
	int scroll_delta = mouse->GetScrollDelta();
	if (scroll_delta != 0) {
		EDITOR_SANDBOX_VIEWPORT current_viewport = GetSandboxCurrentViewport(editor_state, data->sandbox_index);
		float factor = 0.015f;

		if (keyboard->IsDown(ECS_KEY_LEFT_SHIFT)) {
			factor = 0.004f;
		}
		else if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
			factor = 0.06f;
		}

		float3 camera_rotation = GetSandboxCameraPoint(editor_state, data->sandbox_index).rotation;
		float3 forward_vector = GetForwardVector(camera_rotation);

		TranslateSandboxCamera(editor_state, data->sandbox_index, forward_vector * float3::Splat(scroll_delta * factor));
		RenderSandbox(editor_state, data->sandbox_index, current_viewport);
	}
	// Zoom end
}

void SceneLeftClickableAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SceneActionData* data = (SceneActionData*)_data;

	EditorState* editor_state = data->draw_data->editor_state;
	unsigned int sandbox_index = data->sandbox_index;

	if (IsClickableTrigger(action_data)) {
		uint2 hovered_texel_offset = system->GetMousePositionHoveredWindowTexelPosition();
		Entity hovered_entity;
		bool should_wait = DetermineHoveredElement(data->draw_data, sandbox_index, hovered_texel_offset, &hovered_entity);
		if (should_wait) {
			// At the moment don't do anything
		}
		else {
			// Now commit the selection
			if (IsSandboxEntityValid(editor_state, sandbox_index, hovered_entity)) {
				ChangeSandboxSelectedEntities(editor_state, sandbox_index, { &hovered_entity, 1 });
			}
			else {
				// No entity was hovered, clear the selection
				ClearSandboxSelectedEntities(editor_state, sandbox_index);
			}
		}
	}
}

void SceneUIWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	SceneDrawData* data = (SceneDrawData*)window_data;
	EditorState* editor_state = data->editor_state;

	unsigned int sandbox_index = GetWindowNameIndex(drawer.system->GetWindowName(drawer.window_index));
	EDITOR_SANDBOX_VIEWPORT current_viewport = GetSandboxCurrentViewport(editor_state, sandbox_index);
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	if (initialize) {
		data->previous_texel_size = { 0, 0 };
		// Enable the rendering of the viewport
		EnableSandboxViewportRendering(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
	}
	else {
		bool multiple_graphics_module = false;
		unsigned int sandbox_graphics_module_index = GetSandboxGraphicsModule(editor_state, sandbox_index, &multiple_graphics_module);
		bool has_graphics = sandbox_graphics_module_index != -1 && !multiple_graphics_module;
		if (has_graphics) {
			ResizeSandboxTextures(editor_state, drawer, current_viewport, &data->previous_texel_size);
			DisplaySandboxTexture(editor_state, drawer, current_viewport);

			SceneActionData hoverable_data;
			hoverable_data.draw_data = data;
			hoverable_data.sandbox_index = sandbox_index;

			// Add the handlers for rotation, translation, zoom
			UIActionHandler rotation_handler = { SceneRotationAction, &hoverable_data, sizeof(hoverable_data) };
			drawer.SetWindowClickable(&rotation_handler, ECS_MOUSE_RIGHT);
			UIActionHandler translation_handler = { SceneTranslationAction, &hoverable_data, sizeof(hoverable_data) };
			drawer.SetWindowClickable(&translation_handler, ECS_MOUSE_MIDDLE);
			UIActionHandler zoom_handler = { SceneZoomAction, &hoverable_data, sizeof(hoverable_data) };
			drawer.SetWindowHoverable(&zoom_handler);
			UIActionHandler selection_handler = { SceneLeftClickableAction, &hoverable_data, sizeof(hoverable_data) };
			drawer.SetWindowClickable(&selection_handler);

			DisplayGraphicsModuleRecompilationWarning(editor_state, sandbox_index, sandbox_graphics_module_index, drawer);
		}
		else {
			DisplayNoGraphicsModule(drawer, multiple_graphics_module);
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

unsigned int SceneUITargetSandbox(const EditorState* editor_state, unsigned int window_index)
{
	Stream<char> window_name = editor_state->ui_system->GetWindowName(window_index);
	Stream<char> prefix = SCENE_WINDOW_NAME;
	if (window_name.StartsWith(prefix)) {
		return GetWindowNameIndex(window_name);
	}
	else {
		return -1;
	}
}
