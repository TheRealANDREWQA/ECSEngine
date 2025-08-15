#include "editorpch.h"
#include "ECSEngineSampleTexture.h"
#include "ECSEngineComponents.h"
#include "ECSEngineMath.h"
#include "ECSEngineForEach.h"
#include "Scene.h"
#include "../Editor/EditorState.h"
#include "HelperWindows.h"
#include "Common.h"
#include "../Editor/EditorPalette.h"
#include "../Modules/Module.h"
#include "RenderingCommon.h"
#include "../Sandbox/SandboxEntityOperations.h"
#include "../Editor/EditorInputMapping.h"
#include "../Sandbox/SandboxScene.h"
#include "../Sandbox/Sandbox.h"
#include "../Sandbox/SandboxModule.h"
#include "../Sandbox/SandboxFile.h"
#include "../Sandbox/SandboxProfiling.h"
#include "DragTargets.h"
#include "../Assets/PrefabFile.h"
#include "../Project/ProjectFolders.h"

// These defined the bounds under which the mouse
// is considered that it clicked and not selected yet
#define CLICK_SELECTION_MARGIN_X 0.01f
#define CLICK_SELECTION_MARGIN_Y 0.01f

struct HandleSelectedEntitiesTransformUpdateDescriptor {
	EditorState* editor_state;
	unsigned int sandbox_handle;
	UISystem* system;
	unsigned int window_index;
	float2 mouse_position;
	float3* translation_midpoint;
	QuaternionScalar rotation_midpoint;
	TransformToolDrag* tool_drag;
	ECS_TRANSFORM_TOOL tool_to_use;

	// If this is set to true, the ray will be cast at the objects location instead of the mouse'
	bool launch_at_object_position = false;
	// If this is specified, this will add the rotation delta to this quaternion
	QuaternionScalar* rotation_delta = nullptr;
	// If this is specified, this will add the scale delta to the value
	float3* scale_delta = nullptr;
};

static void AddSelectedEntitiesComponentIfMissing(EditorState* editor_state, unsigned int sandbox_handle, ECS_TRANSFORM_TOOL tool) {
	Component component_id = TransformToolComponentID(tool);
	Stream<char> component_name = TransformToolComponentName(tool);

	Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_handle);
	for (size_t index = 0; index < selected_entities.size; index++) {
		const void* component = GetSandboxEntityComponentEx(editor_state, sandbox_handle, selected_entities[index], component_id, false);
		if (component == nullptr) {
			// Add the component to the entity
			AddSandboxEntityComponent(editor_state, sandbox_handle, selected_entities[index], component_name);
		}
	}
}

// Returns true if a modification was made, else false (can be used to update the render viewports)
static bool HandleSelectedEntitiesTransformUpdate(const HandleSelectedEntitiesTransformUpdateDescriptor* descriptor) {
	EditorState* editor_state = descriptor->editor_state;
	unsigned int sandbox_handle = descriptor->sandbox_handle;
	Keyboard* keyboard = descriptor->system->m_keyboard;

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	Camera camera = GetSandboxCamera(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
	int2 unclampped_texel_position = descriptor->system->GetWindowTexelPositionEx(descriptor->window_index, descriptor->mouse_position);
	uint2 viewport_dimensions = descriptor->system->GetWindowTexelSize(descriptor->window_index);

	switch (descriptor->tool_to_use) {
	case ECS_TRANSFORM_TRANSLATION:
	{
		float3 ray_direction = ViewportToWorldRayDirection(&camera, viewport_dimensions, unclampped_texel_position);
		if (descriptor->launch_at_object_position) {
			static int2 last_position = int2(0, 0);
			int2 mouse_delta = descriptor->system->GetTexelMouseDelta();
			if (!descriptor->tool_drag->translation.WasFirstCalled()) {
				ray_direction = Normalize(*descriptor->translation_midpoint - camera.translation);
				last_position = PositionToViewportTexels(&camera, viewport_dimensions, *descriptor->translation_midpoint);
			}
			else {
				if (mouse_delta != int2(0, 0)) {
					int2 new_position = last_position + mouse_delta;
					last_position = new_position;

					ray_direction = ViewportToWorldRayDirection(&camera, viewport_dimensions, new_position);
				}
				else {
					ray_direction = descriptor->tool_drag->translation.last_successful_direction;
				}
			}
		}

		float3 translation_delta = HandleTranslationToolDelta(
			&camera,
			*descriptor->translation_midpoint,
			descriptor->rotation_midpoint,
			&descriptor->tool_drag->translation,
			ray_direction
		);

		if (translation_delta != float3::Splat(0.0f)) {
			TranslateSandboxSelectedEntities(editor_state, sandbox_handle, translation_delta);
			// Also translate the midpoint along
			*descriptor->translation_midpoint += translation_delta;
			return true;
		}
	}
	break;
	case ECS_TRANSFORM_ROTATION:
	{
		float factor = GetKeyboardModifierValue(keyboard, 0.5f);
		QuaternionScalar rotation_delta = HandleRotationToolDeltaCircleMapping(
			&camera,
			*descriptor->translation_midpoint,
			descriptor->rotation_midpoint,
			&descriptor->tool_drag->rotation,
			viewport_dimensions,
			unclampped_texel_position,
			factor
		);

		if (rotation_delta != QuaternionIdentityScalar()) {
			RotateSandboxSelectedEntities(editor_state, sandbox_handle, rotation_delta);

			if (descriptor->rotation_delta != nullptr) {
				*descriptor->rotation_delta = (*descriptor->rotation_delta) * rotation_delta;
			}
			return true;
		}
	}
	break;
	case ECS_TRANSFORM_SCALE:
	{
		float factor = GetKeyboardModifierValue(keyboard, 0.006f);
		float3 scale_delta = HandleScaleToolDelta(
			&camera,
			*descriptor->translation_midpoint,
			descriptor->rotation_midpoint,
			&descriptor->tool_drag->scale,
			descriptor->system->GetTexelMouseDelta(),
			factor
		);

		if (scale_delta != float3::Splat(0.0f)) {
			ScaleSandboxSelectedEntities(editor_state, sandbox_handle, scale_delta);

			if (descriptor->scale_delta != nullptr) {
				*descriptor->scale_delta += scale_delta;
			}
			return true;
		}
	}
	break;
	}

	return false;
}

struct SceneDrawData {
	EditorState* editor_state;
	uint2 previous_texel_size;
	
	// Keep the list of copied entities here since we need to deallocate it when the window is destroyed
	Stream<Entity> copied_entities;
};

static void SceneUIDestroy(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	SceneDrawData* draw_data = (SceneDrawData*)_additional_data;

	// Determine the sandbox index
	unsigned int sandbox_handle = GetWindowNameIndex(system->GetWindowName(window_index));
	// Disable the viewport rendering
	DisableSandboxViewportRendering(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);

	// Release the copied_entities buffer
	draw_data->copied_entities.Deallocate(editor_state->EditorAllocator());
}

struct ScenePrivateActionData {
	// This is the only field that needs to be initialized from the outside
	EditorState* editor_state;

	// Internal fields
	TransformToolDrag drag_tool;
	
	union {
		// These are used when axes transform are triggered
		struct {
			// This needs to be calculated once when rotating and scaling and updated when translating
			float3 translation_midpoint;
			// This needs to be calculated once and then kept the same across calls
			QuaternionScalar rotation_midpoint;
		};
		// These are used when the camera wasd movement is activated
		struct {
			float3 camera_wasd_initial_translation;
			float3 camera_wasd_initial_rotation;
		};
	};

	// This is the value calculated when the keyboard tool was activated
	float3 original_translation_midpoint;
	QuaternionScalar rotation_delta;
	// This is the total scale change
	float3 scale_delta;

	// This id is used to know when we should handle the Ctrl shortcuts
	unsigned int basic_operations_focus_id;
};

// The disable modifiers will make the rotation not change based on shift/ctrl
// Returns true if the camera was rotated
static bool HandleCameraRotation(EditorState* editor_state, unsigned int sandbox_handle, bool disable_modifiers, bool disable_file_write) {
	UISystem* system = editor_state->ui_system;
	float rotation_factor = 150.0f;
	float2 mouse_position = system->GetNormalizeMousePosition();
	float2 delta = system->GetMouseDelta(mouse_position);

	if (!disable_modifiers) {
		rotation_factor *= GetKeyboardModifierValueCustom(editor_state->Keyboard(), 3.0f);
	}

	float3 rotation = { delta.y * rotation_factor, delta.x * rotation_factor, 0.0f };
	// If the rotation has changed, rerender the sandbox
	if (rotation.x != 0.0f || rotation.y != 0.0f) {
		RotateSandboxCamera(editor_state, sandbox_handle, rotation, EDITOR_SANDBOX_VIEWPORT_SCENE, disable_file_write);
		return true;
	}
	return false;
}

static void ResetTransformToolSelectedAxesAndRerender(EditorState* editor_state, unsigned int sandbox_handle) {
	ResetSandboxTransformToolSelectedAxes(editor_state, sandbox_handle);
	GetSandbox(editor_state, sandbox_handle)->transform_display_axes = false;
	RenderSandbox(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
}

static void ResetCameraWasd(EditorState* editor_state, unsigned int sandbox_handle) {
	// End the camera movement
	GetSandbox(editor_state, sandbox_handle)->is_camera_wasd_movement = false;
	// Disable the mouse raw input and make the cursor visible again
	editor_state->Mouse()->DisableRawInput();
	editor_state->Mouse()->SetCursorVisibility(true);

	// Re-enable the control and shift buttons
	editor_state->input_mapping.ReenableKey(ECS_KEY_LEFT_SHIFT);
	editor_state->input_mapping.ReenableKey(ECS_KEY_LEFT_CTRL);
}

// Cancels the display axes, if present, and restores the position/rotation/scale of the currently
// Selected elements to their original transform. Returns true if it did cancel the transform tool, else false
static bool CancelAndRestoreTransformTool(EditorState* editor_state, unsigned int sandbox_handle, ScenePrivateActionData* data) {
	const EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	if (sandbox->transform_display_axes) {
		// Restore the value to the selected entities
		switch (sandbox->transform_keyboard_tool) {
		case ECS_TRANSFORM_TRANSLATION:
		{
			// Restore the translation
			// Calculate the delta between the current translation and the original one
			float3 delta = data->translation_midpoint - data->original_translation_midpoint;
			TranslateSandboxSelectedEntities(editor_state, sandbox_handle, -delta);
		}
		break;
		case ECS_TRANSFORM_ROTATION:
		{
			// Restore the rotation by adding the inverse delta
			QuaternionScalar inverse_rotation = QuaternionInverse(data->rotation_delta);
			RotateSandboxSelectedEntities(editor_state, sandbox_handle, inverse_rotation);
		}
		break;
		case ECS_TRANSFORM_SCALE:
		{
			// Restore the scale by adding the negated delta
			ScaleSandboxSelectedEntities(editor_state, sandbox_handle, -data->scale_delta);
		}
		break;
		}

		ResetTransformToolSelectedAxesAndRerender(editor_state, sandbox_handle);
		return true;
	}
	
	return false;
}

// If the camera WASD movement is currently active, it will cancel it and restores the
// Initial camera position, the position when the movement was started.
// Returns true if it canceled the WASD movement, else false
static bool CancelAndRestoreWASDMovement(EditorState* editor_state, unsigned int sandbox_handle, ScenePrivateActionData* data) {
	if (GetSandbox(editor_state, sandbox_handle)->is_camera_wasd_movement) {
		// End the camera wasd movement and restore the previous translation and rotation
		SetSandboxCameraTranslation(editor_state, sandbox_handle, data->camera_wasd_initial_translation, EDITOR_SANDBOX_VIEWPORT_SCENE, true);
		SetSandboxCameraRotation(editor_state, sandbox_handle, data->camera_wasd_initial_rotation, EDITOR_SANDBOX_VIEWPORT_SCENE);

		ResetCameraWasd(editor_state, sandbox_handle);
		// Rerender the sandbox again
		RenderSandbox(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
		return true;
	}

	return false;
}

// Returns true if a value was changed (the camera's translation or rotation) and implicitly the sandbox is rendered, else false
static bool HandleCameraWASDMovement(EditorState* editor_state, unsigned int sandbox_handle) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	// Check the speed modifiers - since the camera translation will write the sandbox file once
	// We can just modify the camera wasd speed here and it will get updated as well
	const float SPEED_INCREASE_STEP = EDITOR_SANDBOX_CAMERA_WASD_DEFAULT_SPEED * 0.25f;
	bool changed_speed = false;

	bool is_shift_down = editor_state->Keyboard()->IsDown(ECS_KEY_LEFT_SHIFT);
	bool is_ctrl_down = editor_state->Keyboard()->IsDown(ECS_KEY_LEFT_CTRL);
	if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_WASD_INCREASE_SPEED)) {
		if (is_shift_down) {
			sandbox->camera_wasd_speed *= 2.0f;
		}
		else {
			sandbox->camera_wasd_speed += SPEED_INCREASE_STEP;
		}
		changed_speed = true;
	}
	if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_WASD_DECREASE_SPEED)) {
		if (is_shift_down) {
			sandbox->camera_wasd_speed *= 0.5f;
		}
		else {
			sandbox->camera_wasd_speed -= SPEED_INCREASE_STEP;
		}
		changed_speed = true;
	}
	if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_WASD_RESET_SPEED)) {
		sandbox->camera_wasd_speed = EDITOR_SANDBOX_CAMERA_WASD_DEFAULT_SPEED;
		changed_speed = true;
	}
	float speed = sandbox->camera_wasd_speed;
	if (is_shift_down) {
		speed *= 0.2f;
	}
	else if (is_ctrl_down) {
		speed *= 5.0f;
	}

	bool camera_was_rotated = HandleCameraRotation(editor_state, sandbox_handle, true, true);

	// Check the wasd keys
	Camera camera = GetSandboxCamera(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
	float3 camera_forward = camera.GetForwardVector();
	float3 camera_right = camera.GetRightVector();
	float3 delta = WASDController(
		&editor_state->input_mapping, 
		EDITOR_INPUT_WASD_W, 
		EDITOR_INPUT_WASD_A, 
		EDITOR_INPUT_WASD_S,
		EDITOR_INPUT_WASD_D, 
		speed * 0.0002f,
		editor_state->ui_system->GetFrameDeltaTime(),
		camera_forward, 
		camera_right
	);

	// Change the frame pacing to one a little faster
	bool was_camera_changed = false;
	editor_state->ui_system->SetFramePacing(ECS_UI_FRAME_PACING_HIGH);
	if (delta != float3::Splat(0.0f)) {
		TranslateSandboxCamera(editor_state, sandbox_handle, delta, EDITOR_SANDBOX_VIEWPORT_SCENE);
		// Re-render the sandbox as well and increase the frame pacing
		editor_state->ui_system->SetFramePacing(ECS_UI_FRAME_PACING_INSTANT);
		RenderSandbox(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
		was_camera_changed = true;
	}
	else if (camera_was_rotated) {
		// Render the sandbox and save the editor file
		editor_state->ui_system->SetFramePacing(ECS_UI_FRAME_PACING_INSTANT);
		RenderSandbox(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
		SaveEditorSandboxFile(editor_state);
		was_camera_changed = true;
	}
	else if (changed_speed) {
		// We need to write the editor sandbox file in order to preserve this
		SaveEditorSandboxFile(editor_state);
	}

	return was_camera_changed;
}

static void FocusOnSelection(EditorState* editor_state, unsigned int sandbox_handle) {
	Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_handle);
	if (selected_entities.size > 0) {
		unsigned int in_sandbox_graphics_module_index = GetSandboxGraphicsModule(editor_state, sandbox_handle);
		if (in_sandbox_graphics_module_index == -1) {
			// If we can't deduce the graphics module, then don't do anything
			return;
		}

		const EditorSandboxModule* sandbox_module = GetSandboxModule(editor_state, sandbox_handle, in_sandbox_graphics_module_index);
		EDITOR_MODULE_CONFIGURATION configuration = sandbox_module->module_configuration;
		ModuleExtraInformation extra_information = GetModuleExtraInformation(editor_state, sandbox_module->module_index, configuration);

		GraphicsModuleRenderMeshBounds bounds = GetGraphicsModuleRenderMeshBounds(extra_information);
		if (bounds.IsValid()) {
			// Try to retrieve the component and check to see if the field exists
			const Reflection::ReflectionType* render_mesh_type = editor_state->editor_components.GetType(bounds.component);
			if (render_mesh_type != nullptr) {
				// Check the field now
				unsigned int field_index = render_mesh_type->FindField(bounds.field);
				if (field_index != -1) {
					// Check to see that the field is indeed a mesh component
					if (render_mesh_type->fields[field_index].definition == STRING(CoalescedMesh)) {
						// We can now deduce the bounds for all selected meshes
						// Perform the focusing

						bool is_shared_component = editor_state->editor_components.IsSharedComponent(bounds.component);
						Component component = editor_state->editor_components.GetComponentID(bounds.component);

						AABBScalar selection_bounds = ReverseInfiniteAABBScalar();
						for (size_t index = 0; index < selected_entities.size; index++) {
							AABBScalar current_bounds = InfiniteAABBScalar();
							const void* component_data = GetSandboxEntityComponentEx(editor_state, sandbox_handle, selected_entities[index], component, is_shared_component);
							if (component_data) {
								// Verify that the asset pointer is valid as well
								const void** asset_pointer = (const void**)OffsetPointer(component_data, render_mesh_type->fields[field_index].info.pointer_offset);
								if (IsAssetPointerValid(*asset_pointer)) {
									const CoalescedMesh* coalesced_mesh = (const CoalescedMesh*)*asset_pointer;
									current_bounds = coalesced_mesh->mesh.bounds;
								}
							}

							if (!CompareAABBMask(current_bounds, InfiniteAABBScalar())) {
								// We have found a match
								TransformScalar entity_transform = GetSandboxEntityTransform(editor_state, sandbox_handle, selected_entities[index]);
								// Transform the aabb
								current_bounds = TransformAABB(current_bounds, entity_transform.position, QuaternionToMatrix(entity_transform.rotation), entity_transform.scale);
								// Combine the current aabb
								selection_bounds = GetCombinedAABB(selection_bounds, current_bounds);
							}
						}

						if (!CompareAABBMask(selection_bounds, ReverseInfiniteAABBScalar())) {
							Camera scene_camera = GetSandboxCamera(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
							
							// If we have at least one AABB, then focus on it
							float3 camera_position = FocusCameraOnObjectViewSpace(
								&scene_camera,
								selection_bounds,
								0.6f
							);
							if (camera_position != float3::Splat(FLT_MAX)) {
								float3 displacement = camera_position - scene_camera.translation;
								if (displacement != float3::Splat(0.0f)) {
									TranslateSandboxCamera(editor_state, sandbox_handle, displacement, EDITOR_SANDBOX_VIEWPORT_SCENE);
									// We must also re-render the scene
									RenderSandbox(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
								}
							}
						}
					}
					else {
						EditorSetConsoleWarn("Focus: The given field for the render mesh bounds is not a CoalescedMesh");
					}
				}
				else {
					EditorSetConsoleWarn("Focus: Missing field for the render mesh bounds");
				}
			}
			else {
				EditorSetConsoleWarn("Focus: Wrong component for the render mesh bounds");
			}
		}
		else {
			EditorSetConsoleWarn("Focus: Render mesh bounds not set");
		}
	}
}

static void ScenePrivateAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	ScenePrivateActionData* data = (ScenePrivateActionData*)_data;
	SceneDrawData* draw_data = (SceneDrawData*)_additional_data;
	EditorState* editor_state = data->editor_state;

	unsigned int sandbox_handle = GetWindowNameIndex(system->GetWindowName(window_index));
	// Determine if the transform tool needs to be changed
	unsigned int target_sandbox = GetActiveWindowSandbox(editor_state);
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);

	

	// If values are being entered into a field don't change the tool
	if (target_sandbox == sandbox_handle) {
		// Check to see if the camera wasd movement is activated
		if (sandbox->is_camera_wasd_movement) {
			HandleCameraWASDMovement(editor_state, sandbox_handle);
		}
		// Check for camera wasd activation first. If the right click camera movement is active,
		// Then don't trigger any of the following actions
		else if (!sandbox->is_camera_right_click_movement) {
			if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_CAMERA_WALK)) {
				if (sandbox->is_camera_wasd_movement) {
					ResetCameraWasd(editor_state, sandbox_handle);
				}
				else {
					// Enable the mouse ray input and disable the cursor
					mouse->EnableRawInput();
					mouse->SetCursorVisibility(false);
					sandbox->is_camera_wasd_movement = true;

					// Initialize the camera translation/rotation
					OrientedPoint camera_point = GetSandboxCameraPoint(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
					data->camera_wasd_initial_translation = camera_point.position;
					data->camera_wasd_initial_rotation = camera_point.rotation;
					// We need this to make the camera movement smoother
					system->SetFramePacing(ECS_UI_FRAME_PACING_INSTANT);

					// Disable the left control and shift actions
					editor_state->input_mapping.DisableKey(ECS_KEY_LEFT_SHIFT);
					editor_state->input_mapping.DisableKey(ECS_KEY_LEFT_CTRL);

					// Disable the axes if they are drawn
					sandbox->transform_display_axes = false;
					ResetSandboxTransformToolSelectedAxes(editor_state, sandbox_handle);
				}
			}
			else {
				// Check the focus on object
				if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_FOCUS_OBJECT)) {
					FocusOnSelection(editor_state, sandbox_handle);
				}

				// Check the display statistics mapping - only if we are the active window
				// Since the Game UI window will check for this as well
				if (window_index == system->GetActiveWindow() && editor_state->input_mapping.IsTriggered(EDITOR_INPUT_SANDBOX_STATISTICS_TOGGLE)) {
					InvertSandboxStatisticsDisplay(editor_state, sandbox_handle);
				}

				ECS_TRANSFORM_TOOL current_tool = sandbox->transform_tool;
				if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_CHANGE_TO_TRANSLATION_TOOL)) {
					sandbox->transform_tool = ECS_TRANSFORM_TRANSLATION;
				}
				else if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_CHANGE_TO_ROTATION_TOOL)) {
					sandbox->transform_tool = ECS_TRANSFORM_ROTATION;
				}
				else if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_CHANGE_TO_SCALE_TOOL)) {
					sandbox->transform_tool = ECS_TRANSFORM_SCALE;
				}

				// COUNT means no rerender, SCENE only scene, RUNTIME both scene and the runtime
				EDITOR_SANDBOX_VIEWPORT trigger_rerender_viewport = EDITOR_SANDBOX_VIEWPORT_COUNT;
				if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_CHANGE_TRANSFORM_SPACE)) {
					sandbox->transform_space = InvertTransformSpace(sandbox->transform_space);
					trigger_rerender_viewport = EDITOR_SANDBOX_VIEWPORT_SCENE;
				}

				auto initiate_transform = [&](ECS_TRANSFORM_TOOL tool) {
					// Check to see if the same transform was pressed to change the space
					// Don't reset the active axis if the tool is already this one and the keyboard axes are activated
					if (sandbox->transform_keyboard_tool == tool && sandbox->transform_display_axes) {
						data->drag_tool.SetSpace(InvertTransformSpace(data->drag_tool.GetSpace()));
						// If this the rotation tool, we also need to recalculate the rotation midpoint
						if (tool == ECS_TRANSFORM_ROTATION) {
							Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_handle);
							GetSandboxEntitiesMidpointWithGizmos(editor_state, sandbox_handle, selected_entities, &data->translation_midpoint, &data->rotation_midpoint);
						}
					}
					else {
						data->drag_tool.SetSpace(sandbox->transform_space);
						ResetSandboxTransformToolSelectedAxes(editor_state, sandbox_handle);
					}
					sandbox->transform_keyboard_space = data->drag_tool.GetSpace();
					trigger_rerender_viewport = EDITOR_SANDBOX_VIEWPORT_SCENE;
					sandbox->transform_display_axes = true;
					sandbox->transform_keyboard_tool = tool;

					// Add the component type if it is missing
					AddSelectedEntitiesComponentIfMissing(editor_state, sandbox_handle, tool);
				};

				// For each initiate transform check to see if we need to change from 
				if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_INITIATE_TRANSLATION)) {
					initiate_transform(ECS_TRANSFORM_TRANSLATION);
				}
				else if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_INITIATE_ROTATION)) {
					initiate_transform(ECS_TRANSFORM_ROTATION);
				}
				else if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_INITIATE_SCALE)) {
					initiate_transform(ECS_TRANSFORM_SCALE);
				}

				// Check to see if the initiated transform mode is selected
				if (sandbox->transform_display_axes) {
					auto change_selected_axis = [&](ECS_AXIS axis) {
						ResetSandboxTransformToolSelectedAxes(editor_state, sandbox_handle);
						sandbox->transform_tool_selected[axis] = true;
						trigger_rerender_viewport = EDITOR_SANDBOX_VIEWPORT_SCENE;

						// Reinitialize the drag tool
						// The initialize will overwrite the previously set space,
						// So, store it beforehand and re-apply it
						ECS_TRANSFORM_SPACE transform_space = data->drag_tool.GetSpace();
						switch (sandbox->transform_keyboard_tool) {
						case ECS_TRANSFORM_TRANSLATION:
							data->drag_tool.translation.Initialize();
							break;
						case ECS_TRANSFORM_ROTATION:
							data->drag_tool.rotation.InitializeCircle();
							break;
						case ECS_TRANSFORM_SCALE:
							data->drag_tool.scale.Initialize();
							break;
						default:
							ECS_ASSERT(false, "Invalid keyboard transform tool");
						}
						data->drag_tool.SetSpace(transform_space);

						// Get the rotation and translation midpoints for the selected entities
						Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_handle);
						GetSandboxEntitiesMidpointWithGizmos(editor_state, sandbox_handle, selected_entities, &data->translation_midpoint, &data->rotation_midpoint);
						data->drag_tool.SetAxis(axis);
						data->scale_delta = float3::Splat(0.0f);
						data->original_translation_midpoint = data->translation_midpoint;
						data->rotation_delta = QuaternionIdentityScalar();
					};

					if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_AXIS_X)) {
						change_selected_axis(ECS_AXIS_X);
					}
					else if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_AXIS_Y)) {
						change_selected_axis(ECS_AXIS_Y);
					}
					else if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_AXIS_Z)) {
						change_selected_axis(ECS_AXIS_Z);
					}

					// Check to see if we have active keyboard action
					unsigned char axes_select_count = 0;
					ECS_AXIS axis = ECS_AXIS_COUNT;
					for (size_t index = 0; index < ECS_AXIS_COUNT; index++) {
						if (sandbox->transform_tool_selected[index]) {
							axis = (ECS_AXIS)index;
							axes_select_count++;
						}
					}

					// We have just one axis selected
					if (axes_select_count == 1) {
						HandleSelectedEntitiesTransformUpdateDescriptor update_descriptor;
						update_descriptor.editor_state = editor_state;
						update_descriptor.mouse_position = mouse_position;
						update_descriptor.rotation_midpoint = sandbox->transform_keyboard_space == ECS_TRANSFORM_LOCAL_SPACE ? data->rotation_midpoint : QuaternionIdentityScalar();
						update_descriptor.sandbox_handle = sandbox_handle;
						update_descriptor.system = system;
						update_descriptor.tool_drag = &data->drag_tool;
						update_descriptor.translation_midpoint = &data->translation_midpoint;
						update_descriptor.window_index = window_index;
						update_descriptor.tool_to_use = sandbox->transform_keyboard_tool;
						update_descriptor.launch_at_object_position = true;
						update_descriptor.rotation_delta = &data->rotation_delta;
						update_descriptor.scale_delta = &data->scale_delta;

						bool should_rerender = HandleSelectedEntitiesTransformUpdate(&update_descriptor);
						if (should_rerender) {
							trigger_rerender_viewport = EDITOR_SANDBOX_VIEWPORT_RUNTIME;
						}
					}
				}

				if (current_tool != sandbox->transform_tool || trigger_rerender_viewport != EDITOR_SANDBOX_VIEWPORT_COUNT) {
					if (trigger_rerender_viewport == EDITOR_SANDBOX_VIEWPORT_RUNTIME) {
						RenderSandboxViewports(editor_state, target_sandbox);
					}
					else {
						RenderSandbox(editor_state, target_sandbox, EDITOR_SANDBOX_VIEWPORT_SCENE);
					}
					system->SetFramePacing(ECS_UI_FRAME_PACING_INSTANT);
				}
			}
		}

		// Remove all copied entities which are no longer valid
		Stream<Entity>& copied_entities = draw_data->copied_entities;
		for (size_t index = 0; index < copied_entities.size; index++) {
			if (!IsSandboxEntityValid(editor_state, sandbox_handle, copied_entities[index])) {
				copied_entities.RemoveSwapBack(index);
				// Decrement the index as well
				index--;
			}
		}

		// Handle the Ctrl+C,X,V, D and Delete actions
		data->basic_operations_focus_id = editor_state->shortcut_focus.IncrementOrRegisterForAction(EDITOR_SHORTCUT_FOCUS_SANDBOX_BASIC_OPERATIONS, sandbox_handle, EDITOR_SHORTCUT_FOCUS_PRIORITY0, data->basic_operations_focus_id);

		// Only if we are the active shortcut handler we should perform this operations
		if (editor_state->shortcut_focus.IsIDActive(EDITOR_SHORTCUT_FOCUS_SANDBOX_BASIC_OPERATIONS, sandbox_handle, data->basic_operations_focus_id)) {
			// Returns true if there are entities to be deleted, else false
			auto delete_current_selection = [&]() {
				Stream<Entity> current_selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_handle);
				for (size_t index = 0; index < current_selected_entities.size; index++) {
					DeleteSandboxEntity(editor_state, sandbox_handle, current_selected_entities[index]);
				}
				return current_selected_entities.size > 0;
			};


			bool control_action_trigger_rerender = false;
			// The left shift must not be pressed
			if (keyboard->IsDown(ECS_KEY_LEFT_CTRL) && keyboard->IsUp(ECS_KEY_LEFT_SHIFT)) {
				if (keyboard->IsPressed(ECS_KEY_C)) {
					// Make a new allocation and deallocate the old
					Stream<Entity> current_selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_handle);
					copied_entities.Deallocate(editor_state->EditorAllocator());
					copied_entities.InitializeAndCopy(editor_state->EditorAllocator(), current_selected_entities);
				}
				else if (keyboard->IsPressed(ECS_KEY_X)) {
					// Delete the selected entities
					control_action_trigger_rerender = delete_current_selection();
				}
				else if (keyboard->IsPressed(ECS_KEY_V)) {
					for (size_t index = 0; index < copied_entities.size; index++) {
						CopySandboxEntity(editor_state, sandbox_handle, copied_entities[index]);
					}
					control_action_trigger_rerender = copied_entities.size > 0;
				}
				else if (keyboard->IsPressed(ECS_KEY_D)) {
					Stream<Entity> current_selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_handle);
					if (current_selected_entities.size > 0) {
						for (size_t index = 0; index < current_selected_entities.size; index++) {
							// Also replace the selected entities with the new one
							current_selected_entities[index] = CopySandboxEntity(editor_state, sandbox_handle, current_selected_entities[index]);
						}
						// We need to signal the selected entities counter
						SignalSandboxSelectedEntitiesCounter(editor_state, sandbox_handle);
						control_action_trigger_rerender = true;
					}
				}
			}

			if (keyboard->IsPressed(ECS_KEY_DELETE)) {
				control_action_trigger_rerender = delete_current_selection();
			}
			if (control_action_trigger_rerender) {
				// We need to re-render both views
				RenderSandboxViewports(editor_state, sandbox_handle);
			}
		}
	}

	// If the user has clicked somewhere and we have active axes, disable them
	if (mouse->IsPressed(ECS_MOUSE_LEFT)) {
		if (sandbox->transform_display_axes) {
			ResetTransformToolSelectedAxesAndRerender(editor_state, sandbox_handle);
		}
		else if (sandbox->is_camera_wasd_movement) {
			ResetCameraWasd(editor_state, sandbox_handle);
		}
	}
}

void SceneUISetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, CapacityStream<void>* stack_memory) {
	unsigned int index = *(unsigned int*)stack_memory->buffer;
	ECS_ASSERT(index <= MAX_SCENE_WINDOWS, "The maximum allowed count of scene windows has been exceeded!");

	SceneDrawData* data = (SceneDrawData*)stack_memory->Reserve<SceneDrawData>();

	memset(data, 0, sizeof(*data));
	data->editor_state = editor_state;

	descriptor.draw = SceneUIWindowDraw;

	ScenePrivateActionData* private_data = stack_memory->Reserve<ScenePrivateActionData>();
	private_data->editor_state = editor_state;
	// Set this to -1 to signal that we don't have an id yet
	private_data->basic_operations_focus_id = -1;
	descriptor.private_action = ScenePrivateAction;
	descriptor.private_action_data = private_data;
	descriptor.private_action_data_size = sizeof(*private_data);

	descriptor.destroy_action = SceneUIDestroy;
	descriptor.destroy_action_data = editor_state;

	const size_t MAX_WINDOW_NAME_SIZE = 128;
	CapacityStream<char> window_name(stack_memory->Reserve(MAX_WINDOW_NAME_SIZE), 0, MAX_WINDOW_NAME_SIZE);
	GetSceneUIWindowName(index, window_name);

	descriptor.window_name = window_name;
	descriptor.window_data = data;
	descriptor.window_data_size = sizeof(*data);
}

struct SceneActionData {
	ECS_INLINE bool IsTheSameData(const SceneActionData* other) const {
		return other != nullptr && sandbox_handle == other->sandbox_handle;
	}

	SceneDrawData* draw_data;
	unsigned int sandbox_handle;
	bool is_disabled;
};

static void SceneRotationAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SceneActionData* data = (SceneActionData*)_data;
	EditorState* editor_state = data->draw_data->editor_state;
	
	// If the axes were enabled, don't perform the action

	if (mouse->IsPressed(ECS_MOUSE_RIGHT)) {
		EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_handle);
		ScenePrivateActionData* window_private_data = (ScenePrivateActionData*)system->GetWindowPrivateHandlerData(window_index);
		data->is_disabled = CancelAndRestoreTransformTool(editor_state, data->sandbox_handle, window_private_data) || CancelAndRestoreWASDMovement(editor_state, data->sandbox_handle, window_private_data);
		if (!data->is_disabled) {
			mouse->EnableRawInput();
			mouse->SetCursorVisibility(false);
			// Make the pacing instant such that we won't have a big frame difference
			// That can cause the camera to snap
			system->SetFramePacing(ECS_UI_FRAME_PACING_INSTANT);
			sandbox->is_camera_right_click_movement = true;
		}
	}

	if (!data->is_disabled) {
		if (mouse->IsReleased(ECS_MOUSE_RIGHT)) {
			mouse->DisableRawInput();
			mouse->SetCursorVisibility(true);
			GetSandbox(editor_state, data->sandbox_handle)->is_camera_right_click_movement = false;
		}
		else if (mouse->IsDown(ECS_MOUSE_RIGHT)) {
			system->SetFramePacing(ECS_UI_FRAME_PACING_INSTANT);
		}

		if (mouse->IsHeld(ECS_MOUSE_RIGHT)) {
			// This was changed to allow for movement as well, since it makes navigating quite a bit easier
			HandleCameraWASDMovement(editor_state, data->sandbox_handle);
			
			//bool was_rotated = HandleCameraRotation(editor_state, data->sandbox_handle, false, false);
			/*if (was_rotated) {
				RenderSandbox(editor_state, data->sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE, { 0, 0 }, true);
			}*/
		}
	}
}

static void SceneTranslationAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SceneActionData* data = (SceneActionData*)_data;
	EditorState* editor_state = data->draw_data->editor_state;
	if (mouse->IsPressed(ECS_MOUSE_MIDDLE)) {
		EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_handle);
		
		ScenePrivateActionData* private_data = (ScenePrivateActionData*)system->GetWindowPrivateHandlerData(window_index);
		data->is_disabled = CancelAndRestoreTransformTool(editor_state, data->sandbox_handle, private_data) || CancelAndRestoreWASDMovement(editor_state, data->sandbox_handle, private_data)
			|| sandbox->is_camera_right_click_movement;
		if (!data->is_disabled) {
			mouse->EnableRawInput();
		}
	}
	
	if (!data->is_disabled) {
		if (mouse->IsReleased(ECS_MOUSE_MIDDLE)) {
			mouse->DisableRawInput();
		}
		else if (mouse->IsDown(ECS_MOUSE_MIDDLE)) {
			system->SetFramePacing(ECS_UI_FRAME_PACING_INSTANT);
		}

		if (mouse->IsHeld(ECS_MOUSE_MIDDLE)) {
			if (mouse_delta.x != 0.0f || mouse_delta.y != 0.0f) {
				Camera scene_camera = GetSandboxCamera(editor_state, data->sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
				float3 right_vector = scene_camera.GetRightVector();
				float3 up_vector = scene_camera.GetUpVector();

				float translation_factor = GetKeyboardModifierValueCustom(keyboard, 4.0f, 10.0f);
				float3 translation = right_vector * float3::Splat(-mouse_delta.x * translation_factor) + up_vector * float3::Splat(mouse_delta.y * translation_factor);
				TranslateSandboxCamera(editor_state, data->sandbox_handle, translation, EDITOR_SANDBOX_VIEWPORT_SCENE);
				RenderSandbox(editor_state, data->sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE, { 0, 0 }, true);
			}
		}
	}
}

static void SceneZoomAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SceneActionData* data = (SceneActionData*)_data;

	EditorState* editor_state = data->draw_data->editor_state;
	const EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_handle);
	
	if (!sandbox->is_camera_wasd_movement && !sandbox->is_camera_right_click_movement) {
		int scroll_delta = mouse->GetScrollDelta();
		if (scroll_delta != 0) {
			float factor = GetKeyboardModifierValueCustomBoth(keyboard, 0.1f, 4.0f, 0.015f);

			float3 camera_rotation = GetSandboxCameraPoint(editor_state, data->sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE).rotation;
			float3 forward_vector = GetSandboxCamera(editor_state, data->sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE).GetForwardVector();
			//float3 forward_vector = RotateVectorMatrixLow(GetForwardVector(), MatrixRotation(camera_rotation));

			TranslateSandboxCamera(editor_state, data->sandbox_handle, forward_vector * float3::Splat(scroll_delta * factor), EDITOR_SANDBOX_VIEWPORT_SCENE);
			RenderSandbox(editor_state, data->sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE, { 0, 0 }, true);
		}
	}
}

struct SceneLeftClickableActionData {
	EditorState* editor_state;
	unsigned int sandbox_handle;
	uint2 click_texel_position;
	float2 click_ui_position;
	bool is_selection_mode;
	bool was_keyboard_transform_on_click;

	ECS_AXIS tool_axis;
	// This is cached such that it doesn't need to be calculated each time
	float3 gizmo_translation_midpoint;
	QuaternionScalar gizmo_rotation_midpoint;
	TransformToolDrag transform_drag;
	
	CPUInstanceFramebuffer cpu_framebuffer;

	// This is used for the selection mode to add/disable entities
	Stream<Entity> original_selection;
};

static void SceneLeftClickableAction(ActionData* action_data) {
	// We need to do a trick here in order to prevent multiple render operations to be
	// transmitted at the same time and cause useless redraws
	// Disable the viewport rendering for that sandbox on press and enable it
	// before each manual call inside of here

	UI_UNPACK_ACTION_DATA;

	SceneLeftClickableActionData* data = (SceneLeftClickableActionData*)_data;

	EditorState* editor_state = data->editor_state;
	unsigned int sandbox_handle = data->sandbox_handle;

	// Check to see if the mouse moved
	uint2 hovered_texel_offset = system->GetWindowTexelPositionClamped(window_index, mouse_position);

	if (mouse->IsPressed(ECS_MOUSE_LEFT)) {
		// Disable for this sandbox the selectable axes from keyboard hotkeys
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
		sandbox->transform_display_axes = false;
		ResetSandboxTransformToolSelectedAxes(editor_state, sandbox_handle);

		if (!data->was_keyboard_transform_on_click) {
			data->click_ui_position = mouse_position;
			data->click_texel_position = system->GetMousePositionHoveredWindowTexelPosition();
			data->is_selection_mode = false;
			data->original_selection.InitializeAndCopy(editor_state->EditorAllocator(), GetSandboxSelectedEntities(editor_state, sandbox_handle));
			data->cpu_framebuffer = { nullptr, {0, 0} };

			DisableSandboxViewportRendering(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);

			if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
				RenderTargetView instanced_view = GetSandboxInstancedFramebuffer(editor_state, sandbox_handle);
				data->cpu_framebuffer = TransferInstancesFramebufferToCPUAndAllocate(editor_state->RuntimeGraphics(), instanced_view, ECS_MALLOC_ALLOCATOR);

				// Get an initial entity selected and see if a gizmo was selected
				// Only a single selection
				ECS_STACK_CAPACITY_STREAM(unsigned int, selected_entity_instance, 1);
				uint2 bottom_right_position = hovered_texel_offset;

				// In case the values are already at their limit, do not go overboard
				if (bottom_right_position.x < data->cpu_framebuffer.dimensions.x) {
					bottom_right_position.x++;
				}
				if (bottom_right_position.y < data->cpu_framebuffer.dimensions.y) {
					bottom_right_position.y++;
				}
				GetInstancesFromFramebufferFilteredCPU(data->cpu_framebuffer, hovered_texel_offset, bottom_right_position, &selected_entity_instance);
				// Check for the case nothing is selected
				Entity selected_entity = selected_entity_instance.size == 0 ? (unsigned int)-1 : selected_entity_instance[0];
				// Check to see if this a gizmo Entity
				EditorSandboxEntitySlot entity_slot = FindSandboxVirtualEntitySlot(editor_state, sandbox_handle, selected_entity);
				if (entity_slot.slot_type != EDITOR_SANDBOX_ENTITY_SLOT_COUNT) {
					switch (entity_slot.slot_type) {
					case EDITOR_SANDBOX_ENTITY_SLOT_TRANSFORM_X:
						data->tool_axis = ECS_AXIS_X;
						break;
					case EDITOR_SANDBOX_ENTITY_SLOT_TRANSFORM_Y:
						data->tool_axis = ECS_AXIS_Y;
						break;
					case EDITOR_SANDBOX_ENTITY_SLOT_TRANSFORM_Z:
						data->tool_axis = ECS_AXIS_Z;
						break;
					}

					if (data->tool_axis != ECS_AXIS_COUNT) {
						ResetSandboxTransformToolSelectedAxes(editor_state, sandbox_handle);
						sandbox->transform_tool_selected[data->tool_axis] = true;

						ECS_TRANSFORM_TOOL transform_tool = sandbox->transform_tool;

						data->transform_drag.Initialize(transform_tool);
						data->transform_drag.SetAxis(data->tool_axis);
						data->transform_drag.SetSpace(sandbox->transform_space);

						// Add the component type if it is missing
						AddSelectedEntitiesComponentIfMissing(editor_state, sandbox_handle, transform_tool);

						// If the entity is missing the corresponding component, add it
						Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_handle);

						// Calculate the midpoint here such that it won't need to be calculated each frame
						GetSandboxEntitiesMidpointWithGizmos(
							editor_state, 
							sandbox_handle, 
							selected_entities, 
							&data->gizmo_translation_midpoint, 
							&data->gizmo_rotation_midpoint
						);

						mouse->ActivateWrap(editor_state->UIGraphics()->GetWindowSize());
					}
				}
			}
		}
		else {
			// Render the viewport once since we need to update the visuals that the keyboard axes are disabled
			RenderSandbox(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
		}
	}
	else if (!data->was_keyboard_transform_on_click) {
		if (data->tool_axis == ECS_AXIS_COUNT) {
			// Check to see if the RuntimeGraphics is available for copying to the CPU the values
			if (data->cpu_framebuffer.values == nullptr) {
				if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
					RenderTargetView instanced_view = GetSandboxInstancedFramebuffer(editor_state, sandbox_handle);
					data->cpu_framebuffer = TransferInstancesFramebufferToCPUAndAllocate(editor_state->RuntimeGraphics(), instanced_view, ECS_MALLOC_ALLOCATOR);
				}
			}

			// Check to see if the mouse moved
			if (!data->is_selection_mode) {
				float2 mouse_difference = BasicTypeAbsoluteDifference(mouse_position, data->click_ui_position);
				if (mouse_difference.x > CLICK_SELECTION_MARGIN_X || mouse_difference.y > CLICK_SELECTION_MARGIN_Y) {
					data->is_selection_mode = true;
				}
			}

			if (data->cpu_framebuffer.values != nullptr) {
				if (data->is_selection_mode) {
					system->SetFramePacing(ECS_UI_FRAME_PACING_INSTANT);
					ECS_STACK_CAPACITY_STREAM(unsigned int, selected_entities, ECS_KB * 16);
					RenderTargetView instanced_view = GetSandboxInstancedFramebuffer(editor_state, sandbox_handle);
					uint2 top_left = hovered_texel_offset;
					uint2 bottom_right = hovered_texel_offset;
					// In case some sort of error happened because the mouse is out of the window bounds,
					// Do not use the click positions
					if (data->click_texel_position.x != -1 && data->click_texel_position.y != -1) {
						top_left = BasicTypeMin(hovered_texel_offset, data->click_texel_position);
						bottom_right = BasicTypeMax(hovered_texel_offset, data->click_texel_position);
					}

					if (top_left.x != bottom_right.x && top_left.y != bottom_right.y) {
						GetInstancesFromFramebufferFilteredCPU(data->cpu_framebuffer, top_left, bottom_right, &selected_entities);
						Stream<Entity> selected_entities_stream = { selected_entities.buffer, selected_entities.size };
						FilterSandboxEntitiesValid(editor_state, sandbox_handle, &selected_entities_stream);

						if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
							ChangeSandboxSelectedEntities(editor_state, sandbox_handle, data->original_selection);

							// Reduce the selection by these entities
							for (size_t index = 0; index < selected_entities_stream.size; index++) {
								// This checks to see that the entity exists and 
								RemoveSandboxSelectedEntity(editor_state, sandbox_handle, selected_entities_stream[index]);
							}
							ChangeInspectorEntitySelection(editor_state, sandbox_handle);
						}
						else if (keyboard->IsDown(ECS_KEY_LEFT_SHIFT)) {
							// Add to the sxisting selection
							ChangeSandboxSelectedEntities(editor_state, sandbox_handle, data->original_selection);

							// Add to the selection these entities that do not exist
							for (size_t index = 0; index < selected_entities_stream.size; index++) {
								if (!IsSandboxEntitySelected(editor_state, sandbox_handle, selected_entities_stream[index])) {
									AddSandboxSelectedEntity(editor_state, sandbox_handle, selected_entities_stream[index]);
								}
							}
							ChangeInspectorEntitySelection(editor_state, sandbox_handle);
						}
						else {
							if (selected_entities_stream.size > 0) {
								// We can reinterpret the unsigned ints as entities 
								ChangeSandboxSelectedEntities(editor_state, sandbox_handle, selected_entities_stream);
							}
							else {
								ClearSandboxSelectedEntities(editor_state, sandbox_handle);
							}
							ChangeInspectorEntitySelection(editor_state, sandbox_handle);
						}
					}

					// We must clamp the scale such that it doesn't go outside the window
					float2 window_position = system->GetWindowPosition(window_index);
					float2 window_scale = system->GetWindowScale(window_index);
					Rectangle2D window_rectangle = { window_position, window_position + window_scale };

					float2 selection_top_left = BasicTypeMin(mouse_position, data->click_ui_position);
					float2 selection_bottom_right = BasicTypeMax(mouse_position, data->click_ui_position);
					selection_top_left = ClampPointToRectangle(selection_top_left, window_rectangle);
					selection_bottom_right = ClampPointToRectangle(selection_bottom_right, window_rectangle);

					float2 selection_scale = selection_bottom_right - selection_top_left;

					Color selection_color = EDITOR_SELECT_COLOR;
					selection_color.alpha = 150;
					system->SetSprite(
						dockspace,
						border_index,
						ECS_TOOLS_UI_TEXTURE_MASK,
						selection_top_left,
						selection_scale,
						buffers,
						selection_color,
						{ 0.0f, 0.0f },
						{ 1.0f, 1.0f },
						ECS_UI_DRAW_SYSTEM
					);

					float2 border_scale = system->GetPixelSize() * 2.0f;
					CreateSolidColorRectangleBorder<false>(
						selection_top_left,
						selection_scale,
						border_scale,
						selection_color,
						buffers
					);
				}
				else if (mouse->IsReleased(ECS_MOUSE_LEFT)) {
					// Only a single selection
					RenderTargetView instanced_view = GetSandboxInstancedFramebuffer(editor_state, sandbox_handle);
					ECS_STACK_CAPACITY_STREAM(unsigned int, selected_entity_instance, 1);
					GetInstancesFromFramebufferFilteredCPU(data->cpu_framebuffer, hovered_texel_offset, hovered_texel_offset + uint2(1, 1), &selected_entity_instance);
					// Check for the case nothing is selected
					Entity selected_entity = selected_entity_instance.size == 0 ? (unsigned int)-1 : selected_entity_instance[0];
					// Check to see if this a gizmo Entity
					EditorSandboxEntitySlot entity_slot = FindSandboxVirtualEntitySlot(editor_state, sandbox_handle, selected_entity);
					if (entity_slot.slot_type == EDITOR_SANDBOX_ENTITY_SLOT_COUNT) {
						// We have selected an actual entity, not a gizmo or some other pseudo entity
						if (keyboard->IsDown(ECS_KEY_LEFT_CTRL) || keyboard->IsDown(ECS_KEY_LEFT_SHIFT)) {
							if (IsSandboxEntityValid(editor_state, sandbox_handle, selected_entity)) {
								// See if the entity exists or not
								if (IsSandboxEntitySelected(editor_state, sandbox_handle, selected_entity)) {
									RemoveSandboxSelectedEntity(editor_state, sandbox_handle, selected_entity);
								}
								else {
									AddSandboxSelectedEntity(editor_state, sandbox_handle, selected_entity);
								}
								ChangeInspectorEntitySelection(editor_state, sandbox_handle);
							}
						}
						else {
							if (IsSandboxEntityValid(editor_state, sandbox_handle, selected_entity)) {
								ChangeSandboxSelectedEntities(editor_state, sandbox_handle, { &selected_entity, 1 });
							}
							else {
								ClearSandboxSelectedEntities(editor_state, sandbox_handle);
							}
							ChangeInspectorEntitySelection(editor_state, sandbox_handle);
						}
					}
				}
			}
		}
		else {
			// Transform tool selection here
			HandleSelectedEntitiesTransformUpdateDescriptor update_descriptor;
			update_descriptor.editor_state = editor_state;
			update_descriptor.mouse_position = mouse_position;
			update_descriptor.rotation_midpoint = data->gizmo_rotation_midpoint;
			update_descriptor.sandbox_handle = sandbox_handle;
			update_descriptor.system = system;
			update_descriptor.tool_drag = &data->transform_drag;
			update_descriptor.translation_midpoint = &data->gizmo_translation_midpoint;
			update_descriptor.window_index = window_index;
			update_descriptor.tool_to_use = GetSandbox(editor_state, sandbox_handle)->transform_tool;

			bool should_render_runtime = HandleSelectedEntitiesTransformUpdate(&update_descriptor);
			if (should_render_runtime) {
				RenderSandbox(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
				// Also, set the sandbox as dirty
				SetSandboxSceneDirty(editor_state, sandbox_handle);
			}
		}

		if (mouse->IsReleased(ECS_MOUSE_LEFT)) {
			data->original_selection.Deallocate(editor_state->EditorAllocator());
			if (data->cpu_framebuffer.values != nullptr) {
				data->cpu_framebuffer.Deallocate(ECS_MALLOC_ALLOCATOR);
			}

			// We need to reset any selected tool info
			EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
			ResetSandboxTransformToolSelectedAxes(editor_state, sandbox_handle);
			mouse->DeactivateWrap();
		}

		EnableSandboxViewportRendering(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
		// We need to render one more time in order for the transform tool to appear normal
		RenderSandbox(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
		system->SetFramePacing(ECS_UI_FRAME_PACING_INSTANT);
		if (!mouse->IsReleased(ECS_MOUSE_LEFT)) {
			DisableSandboxViewportRendering(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
		}
	}
}

void SceneUIWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	drawer.DisableZoom();
	drawer.DisablePaddingForRenderSliders();

	ECS_STACK_CAPACITY_STREAM(Stream<char>, acquire_drag, 1);
	acquire_drag[0] = PREFAB_DRAG_NAME;
	acquire_drag.size = acquire_drag.capacity;
	drawer.PushDragDrop(SCENE_UI_DRAG_NAME, acquire_drag, true, EDITOR_GREEN_COLOR, BORDER_DRAG_THICKNESS);
	drawer.HandleAcquireDrag(UI_CONFIG_SYSTEM_DRAW, drawer.GetRegionPosition(), drawer.GetRegionScale());

	SceneDrawData* data = (SceneDrawData*)window_data;
	EditorState* editor_state = data->editor_state;

	unsigned int sandbox_handle = GetWindowNameIndex(drawer.system->GetWindowName(drawer.window_index));
	EDITOR_SANDBOX_VIEWPORT current_viewport = EDITOR_SANDBOX_VIEWPORT_SCENE;
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_handle);
	if (initialize) {
		data->previous_texel_size = { 0, 0 };
		// Enable the rendering of the viewport
		EnableSandboxViewportRendering(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
	}
	else {
		bool multiple_graphics_module = false;
		unsigned int sandbox_graphics_module_index = GetSandboxGraphicsModule(editor_state, sandbox_handle, &multiple_graphics_module);
		bool has_graphics = sandbox_graphics_module_index != -1 && !multiple_graphics_module;
		if (has_graphics) {
			ResizeSandboxTextures(editor_state, drawer, current_viewport, &data->previous_texel_size);
			DisplaySandboxTexture(editor_state, drawer, current_viewport);

			SceneActionData hoverable_data;
			hoverable_data.draw_data = data;
			hoverable_data.sandbox_handle = sandbox_handle;

			// Add the handlers for rotation, translation, zoom
			// Handle these at the system level, such that they don't interfere with the UI rendering
			UIActionHandler rotation_handler = { SceneRotationAction, &hoverable_data, sizeof(hoverable_data), ECS_UI_DRAW_SYSTEM };
			drawer.SetWindowClickable(&rotation_handler, ECS_MOUSE_RIGHT);
			UIActionHandler translation_handler = { SceneTranslationAction, &hoverable_data, sizeof(hoverable_data), ECS_UI_DRAW_SYSTEM };
			drawer.SetWindowClickable(&translation_handler, ECS_MOUSE_MIDDLE);
			UIActionHandler zoom_handler = { SceneZoomAction, &hoverable_data, sizeof(hoverable_data), ECS_UI_DRAW_SYSTEM };
			drawer.SetWindowHoverable(&zoom_handler);

			SceneLeftClickableActionData left_clickable_data;
			left_clickable_data.editor_state = editor_state;
			left_clickable_data.sandbox_handle = sandbox_handle;
			left_clickable_data.click_ui_position = { FLT_MAX, FLT_MAX };
			left_clickable_data.is_selection_mode = false;
			left_clickable_data.tool_axis = ECS_AXIS_COUNT;
			left_clickable_data.was_keyboard_transform_on_click = sandbox->transform_display_axes;
			// We need to handle the event at the system level, such as to not affect the UI rendering
			UIActionHandler selection_handler = { SceneLeftClickableAction, &left_clickable_data, sizeof(left_clickable_data), ECS_UI_DRAW_SYSTEM };
			drawer.SetWindowClickable(&selection_handler);

			DisplayGraphicsModuleRecompilationWarning(editor_state, sandbox_handle, sandbox_graphics_module_index, drawer);
		}
		else {
			DisplayNoGraphicsModule(drawer, multiple_graphics_module);
		}
	}

	// Display the crash message if necessary
	DisplayCrashedSandbox(drawer, editor_state, sandbox_handle);
	// Display the compiling message if necessary
	DisplayCompilingSandbox(drawer, editor_state, sandbox_handle);
	// Display the replay icon if necessary
	DisplayReplayActiveSandbox(drawer, editor_state, sandbox_handle);

	// There is one last thing. To display buttons for scene prefab controls
	if (HasFlag(sandbox->flags, EDITOR_SANDBOX_FLAG_PREFAB)) {
		struct SavePrefabChangesData {
			EditorState* editor_state;
			unsigned int sandbox_handle;
		};

		auto save_prefab_changes = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			SavePrefabChangesData* data = (SavePrefabChangesData*)_data;
			EditorSandbox* sandbox = GetSandbox(data->editor_state, data->sandbox_handle);
			// The scene path of this sandbox is actually the path to the prefab
			Entity prefab_entity = GetPrefabEntityFromSingle();
			
			// Construct the absolute path to the prefab, since that is relative to the project.
			ECS_STACK_CAPACITY_STREAM(wchar_t, absolute_prefab_file_path, ECS_KB);
			GetProjectAssetsFolder(data->editor_state, absolute_prefab_file_path);
			absolute_prefab_file_path.AddAssert(ECS_OS_PATH_SEPARATOR);
			absolute_prefab_file_path.AddStreamAssert(sandbox->scene_path);

			bool success = SavePrefabFile(data->editor_state, data->sandbox_handle, prefab_entity, absolute_prefab_file_path);
			if (success) {
				// Destroy this window since we succeeded - the rest of the windows will be removed
				// By the event handler
				system->DestroyWindowEx(window_index);
			}
			else {
				// We shouldn't destroy the window here since we failed saving and the user will
				// Lose the changes if we destroyed the window
				ECS_FORMAT_TEMP_STRING(message, "Failed to save prefab {#} file", sandbox->scene_path);
				EditorSetConsoleError(message);
			}
		};

		auto cancel_prefab_editing = [](ActionData* action_data) {
			UI_UNPACK_ACTION_DATA;

			// To cancel, we simply have to destroy this window
			system->DestroyWindowEx(window_index);
		};

		SavePrefabChangesData save_change_data = { editor_state, sandbox_handle };
		UIDrawerRowLayout row_layout = drawer.GenerateRowLayout();
		row_layout.SetVerticalAlignment(ECS_UI_ALIGN_BOTTOM);
		row_layout.AddElement(UI_CONFIG_WINDOW_DEPENDENT_SIZE, { 0.0f, 0.0f });
		row_layout.AddElement(UI_CONFIG_WINDOW_DEPENDENT_SIZE, { 0.0f, 0.0f });

		size_t button_configuration = 0;
		UIDrawConfig config;
		row_layout.GetTransform(config, button_configuration);		
		drawer.Button(button_configuration | UI_CONFIG_LATE_DRAW, config, "Save", { save_prefab_changes, &save_change_data, sizeof(save_change_data), ECS_UI_DRAW_SYSTEM });

		button_configuration = 0;
		config.flag_count = 0;
		row_layout.GetTransform(config, button_configuration);
		drawer.Button(button_configuration | UI_CONFIG_LATE_DRAW, config, "Cancel", { cancel_prefab_editing, nullptr, 0, ECS_UI_DRAW_SYSTEM });
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
	// Include the temporary sandboxes as well
	DestroyIndexedWindows(editor_state, SCENE_WINDOW_NAME, GetSandboxCount(editor_state), MAX_SCENE_WINDOWS);
}

void GetSceneUIWindowName(unsigned int index, CapacityStream<char>& name) {
	name.AddStreamAssert(SCENE_WINDOW_NAME);
	ConvertIntToChars(name, index);
}

unsigned int GetSceneUIWindowIndex(const EditorState* editor_state, unsigned int sandbox_handle)
{
	ECS_STACK_CAPACITY_STREAM(char, window_name, 512);
	GetSceneUIWindowName(sandbox_handle, window_name);
	return editor_state->ui_system->GetWindowFromName(window_name);
}

bool DisableSceneUIRendering(EditorState* editor_state, unsigned int sandbox_handle)
{
	unsigned int scene_window_index = GetSceneUIWindowIndex(editor_state, sandbox_handle);
	if (scene_window_index != -1) {
		bool is_scene_visible = editor_state->ui_system->IsWindowVisible(scene_window_index);
		if (is_scene_visible) {
			DisableSandboxViewportRendering(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
			return true;
		}
	}
	return false;
}

bool EnableSceneUIRendering(EditorState* editor_state, unsigned int sandbox_handle, bool must_be_visible)
{
	unsigned int scene_window_index = GetSceneUIWindowIndex(editor_state, sandbox_handle);
	if (scene_window_index != -1) {
		if (must_be_visible) {
			bool is_scene_visible = editor_state->ui_system->IsWindowVisible(scene_window_index);
			if (is_scene_visible) {
				EnableSandboxViewportRendering(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
				return true;
			}
		}
		else {
			EnableSandboxViewportRendering(editor_state, sandbox_handle, EDITOR_SANDBOX_VIEWPORT_SCENE);
			return true;
		}
	}
	return false;
}

void FocusSceneUIOnSelection(EditorState* editor_state, unsigned int sandbox_handle)
{
	FocusOnSelection(editor_state, sandbox_handle);
}

void UpdateSceneUIWindowName(EditorState* editor_state, unsigned int sandbox_handle, Stream<char> new_name) {
	UpdateUIWindowAggregateName(editor_state, SCENE_WINDOW_NAME, GetSandbox(editor_state, sandbox_handle)->name, new_name);
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
