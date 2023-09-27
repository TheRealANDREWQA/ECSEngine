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
#include "../Sandbox/SandboxEntityOperations.h"
#include "ECSEngineSampleTexture.h"
#include "ECSEngineComponents.h"
#include "../Editor/EditorInputMapping.h"
#include "../Sandbox/SandboxScene.h"
#include "../Sandbox/Sandbox.h"
#include "../Sandbox/SandboxModule.h"
#include "../Sandbox/SandboxFile.h"

// These defined the bounds under which the mouse
// is considered that it clicked and not selected yet
#define CLICK_SELECTION_MARGIN_X 0.01f
#define CLICK_SELECTION_MARGIN_Y 0.01f

struct HandleSelectedEntitiesTransformUpdateDescriptor {
	EditorState* editor_state;
	unsigned int sandbox_index;
	UISystem* system;
	unsigned int window_index;
	float2 mouse_position;
	float3* translation_midpoint;
	Quaternion rotation_midpoint;
	TransformToolDrag* tool_drag;
	ECS_TRANSFORM_TOOL tool_to_use;

	// If this is set to true, the ray will be cast at the objects location instead of the mouse'
	bool launch_at_object_position = false;
	// If this is specified, this will add the rotation delta to this quaternion
	Quaternion* rotation_delta = nullptr;
	// If this is specified, this will add the scale delta to the value
	float3* scale_delta = nullptr;
};

static void AddSelectedEntitiesComponentIfMissing(EditorState* editor_state, unsigned int sandbox_index, ECS_TRANSFORM_TOOL tool) {
	Component component_id = TransformToolComponentID(tool);
	Stream<char> component_name = TransformToolComponentName(tool);

	Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
	for (size_t index = 0; index < selected_entities.size; index++) {
		const void* component = GetSandboxEntityComponentEx(editor_state, sandbox_index, selected_entities[index], component_id, false);
		if (component == nullptr) {
			// Add the component to the entity
			AddSandboxEntityComponent(editor_state, sandbox_index, selected_entities[index], component_name);
		}
	}
}

// Returns true if a modification was made, else false (can be used to update the render viewports)
static bool HandleSelectedEntitiesTransformUpdate(const HandleSelectedEntitiesTransformUpdateDescriptor* descriptor) {
	EditorState* editor_state = descriptor->editor_state;
	unsigned int sandbox_index = descriptor->sandbox_index;
	Keyboard* keyboard = descriptor->system->m_keyboard;

	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	Camera camera = GetSandboxCamera(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
	int2 unclampped_texel_position = descriptor->system->GetWindowTexelPositionEx(descriptor->window_index, descriptor->mouse_position);
	uint2 viewport_dimensions = descriptor->system->GetWindowTexelSize(descriptor->window_index);

	Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
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
			for (size_t index = 0; index < selected_entities.size; index++) {
				Translation* translation = GetSandboxEntityComponent<Translation>(editor_state, sandbox_index, selected_entities[index]);
				translation->value += translation_delta;
			}
			// Also translate the midpoint along
			*descriptor->translation_midpoint += translation_delta;
			return true;
		}
	}
	break;
	case ECS_TRANSFORM_ROTATION:
	{
		float factor = 0.5f;
		if (keyboard->IsDown(ECS_KEY_LEFT_SHIFT)) {
			factor *= 0.2f;
		}
		else if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
			factor *= 5.0f;
		}
		Rotation* first_rotation = GetSandboxEntityComponent<Rotation>(editor_state, sandbox_index, selected_entities[0]);
		float4 rotation_delta = HandleRotationToolDeltaCircleMapping(
			&camera,
			*descriptor->translation_midpoint,
			descriptor->rotation_midpoint,
			&descriptor->tool_drag->rotation,
			viewport_dimensions,
			unclampped_texel_position,
			factor
		);

		if (rotation_delta != QuaternionIdentity().StorageLow()) {
			for (size_t index = 0; index < selected_entities.size; index++) {
				Rotation* rotation = GetSandboxEntityComponent<Rotation>(editor_state, sandbox_index, selected_entities[index]);
				Quaternion original_quat = Quaternion(rotation->value);
				// We need to use local rotation regardless of the transform space
				// The transform tool takes care of the correct rotation delta
				Quaternion combined_quaternion = AddLocalRotation(original_quat, rotation_delta);
				rotation->value = combined_quaternion.StorageLow();
			}

			if (descriptor->rotation_delta != nullptr) {
				*descriptor->rotation_delta = AddLocalRotation(*descriptor->rotation_delta, rotation_delta);
			}
			return true;
		}
	}
	break;
	case ECS_TRANSFORM_SCALE:
	{
		float factor = 0.004f;
		if (keyboard->IsDown(ECS_KEY_LEFT_SHIFT)) {
			factor *= 0.2f;
		}
		else if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
			factor *= 5.0f;
		}

		float3 scale_delta = HandleScaleToolDelta(
			&camera,
			*descriptor->translation_midpoint,
			descriptor->rotation_midpoint,
			&descriptor->tool_drag->scale,
			descriptor->system->GetTexelMouseDelta(),
			factor
		);

		if (scale_delta != float3::Splat(0.0f)) {
			for (size_t index = 0; index < selected_entities.size; index++) {
				Scale* scale = GetSandboxEntityComponent<Scale>(editor_state, sandbox_index, selected_entities[index]);
				scale->value += scale_delta;
			}

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
	unsigned int sandbox_index = GetWindowNameIndex(system->GetWindowName(system->GetWindowIndexFromBorder(dockspace, border_index)));
	// Disable the viewport rendering
	DisableSandboxViewportRendering(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);

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
			Quaternion rotation_midpoint;
		};
		// These are used when the camera wasd movement is activated
		struct {
			float3 camera_wasd_initial_translation;
			float3 camera_wasd_initial_rotation;
		};
	};

	// This is the value calculated when the keyboard tool was activated
	float3 original_translation_midpoint;
	Quaternion rotation_delta;
	// This is the total scale change
	float3 scale_delta;
};

// The disable modifiers will make the rotation not change based on shift/ctrl
// Returns true if the camera was rotated
static bool HandleCameraRotation(EditorState* editor_state, unsigned int sandbox_index, bool disable_modifiers, bool disable_file_write) {
	UISystem* system = editor_state->ui_system;
	float rotation_factor = 150.0f;
	float2 mouse_position = system->GetNormalizeMousePosition();
	float2 delta = system->GetMouseDelta(mouse_position);

	if (!disable_modifiers) {
		if (editor_state->Keyboard()->IsDown(ECS_KEY_LEFT_SHIFT)) {
			rotation_factor *= 0.33f;
		}
		else if (editor_state->Keyboard()->IsDown(ECS_KEY_LEFT_CTRL)) {
			rotation_factor *= 3.0f;
		}
	}

	float3 rotation = { delta.y * rotation_factor, delta.x * rotation_factor, 0.0f };
	// If the rotation has changed, rerender the sandbox
	if (rotation.x != 0.0f || rotation.y != 0.0f) {
		RotateSandboxCamera(editor_state, sandbox_index, rotation, EDITOR_SANDBOX_VIEWPORT_SCENE, disable_file_write);
		return true;
	}
	return false;
}

// For increase/decrease speed
static void HandleCameraWASDMovement(EditorState* editor_state, unsigned int sandbox_index) {
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	// Check the speed modifiers - since the camera translation will write the sandbox file once
	// We can just modify the camera wasd speed here and it will get updated as well
	const float SPEED_INCREASE_STEP = EDITOR_SANDBOX_CAMERA_WASD_DEFAULT_SPEED * 0.25f;
	bool changed_speed = false;

	bool is_shift_down = editor_state->Keyboard()->IsDown(ECS_KEY_LEFT_SHIFT);
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

	bool camera_was_rotated = HandleCameraRotation(editor_state, sandbox_index, true, true);

	// Check the wasd keys
	Camera camera = GetSandboxCamera(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
	float3 camera_forward = camera.GetForwardVector().AsFloat3Low();
	float3 camera_right = camera.GetRightVector().AsFloat3Low();
	float3 delta = WASDController(
		&editor_state->input_mapping, 
		EDITOR_INPUT_WASD_W, 
		EDITOR_INPUT_WASD_A, 
		EDITOR_INPUT_WASD_S,
		EDITOR_INPUT_WASD_D, 
		sandbox->camera_wasd_speed * editor_state->ui_system->GetFrameDeltaTime() * 0.0002f,
		camera_forward, 
		camera_right
	);

	// Change the frame pacing to one a little faster
	editor_state->ui_system->m_frame_pacing = std::max(editor_state->ui_system->m_frame_pacing, (unsigned short)ECS_UI_FRAME_PACING_HIGH);
	if (delta != float3::Splat(0.0f)) {
		TranslateSandboxCamera(editor_state, sandbox_index, delta, EDITOR_SANDBOX_VIEWPORT_SCENE);
		// Re-render the sandbox as well and increase the frame pacing
		editor_state->ui_system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
		RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
	}
	else if (camera_was_rotated) {
		// Render the sandbox and save the editor file
		editor_state->ui_system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
		RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
		SaveEditorSandboxFile(editor_state);
	}
	else if (changed_speed) {
		// We need to write the editor sandbox file in order to preserve this
		SaveEditorSandboxFile(editor_state);
	}
}

static void FocusOnSelection(EditorState* editor_state, unsigned int sandbox_index) {
	Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
	if (selected_entities.size > 0) {
		unsigned int in_sandbox_graphics_module_index = GetSandboxGraphicsModule(editor_state, sandbox_index);
		if (in_sandbox_graphics_module_index == -1) {
			// If we can't deduce the graphics module, then don't do anything
			return;
		}

		const EditorSandboxModule* sandbox_module = GetSandboxModule(editor_state, sandbox_index, in_sandbox_graphics_module_index);
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

						AABB selection_bounds = ReverseInfiniteAABB();
						for (size_t index = 0; index < selected_entities.size; index++) {
							AABB current_bounds = InfiniteAABB();
							const void* component_data = GetSandboxEntityComponentEx(editor_state, sandbox_index, selected_entities[index], component, is_shared_component);
							if (component_data) {
								// Verify that the asset pointer is valid as well
								const void** asset_pointer = (const void**)function::OffsetPointer(component_data, render_mesh_type->fields[field_index].info.pointer_offset);
								if (IsAssetPointerValid(*asset_pointer)) {
									const CoalescedMesh* coalesced_mesh = (const CoalescedMesh*)*asset_pointer;
									current_bounds = coalesced_mesh->mesh.bounds;
								}
							}

							if (!CompareAABB(current_bounds, InfiniteAABB())) {
								// We have found a match
								Transform entity_transform = GetSandboxEntityTransform(editor_state, sandbox_index, selected_entities[index]);
								// Transform the aabb
								current_bounds = TransformAABB(current_bounds, entity_transform.position, QuaternionToMatrixLow(entity_transform.rotation), entity_transform.scale);
								// Combine the current aabb
								selection_bounds = GetCombinedAABB(selection_bounds, current_bounds);
							}
						}

						if (!CompareAABB(selection_bounds, ReverseInfiniteAABB())) {
							Camera scene_camera = GetSandboxCamera(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);

							// If we have at least one AABB, then focus on it
							float3 camera_position = FocusCameraOnObjectViewSpace(
								&scene_camera,
								float3::Splat(0.0f),
								QuaternionIdentity(),
								float3::Splat(1.0f),
								selection_bounds.ToStorage(),
								{ 1.0f, 0.0f }
							);
							if (camera_position != float3::Splat(FLT_MAX)) {
								float3 displacement = camera_position - scene_camera.translation;
								if (displacement != float3::Splat(0.0f)) {
									TranslateSandboxCamera(editor_state, sandbox_index, displacement, EDITOR_SANDBOX_VIEWPORT_SCENE);
									// We must also re-render the scene
									RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
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

	unsigned int sandbox_index = GetWindowNameIndex(system->GetWindowName(system->GetWindowIndexFromBorder(dockspace, border_index)));
	// Determine if the transform tool needs to be changed
	unsigned int target_sandbox = GetActiveWindowSandbox(editor_state);
	EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
	// If values are being entered into a field don't change the tool
	if (target_sandbox == sandbox_index && !editor_state->Keyboard()->IsCaptureCharacters()) {
		// Check to see if the camera wasd movement is activated
		if (sandbox->is_camera_wasd_movement) {
			HandleCameraWASDMovement(editor_state, sandbox_index);
		}
		else {
			// Check for camera wasd activation first
			if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_CAMERA_WALK)) {
				// Enable the mouse ray input and disable the cursor
				mouse->EnableRawInput();
				mouse->SetCursorVisibility(false);
				sandbox->is_camera_wasd_movement = true;

				// Initialize the camera translation/rotation
				OrientedPoint camera_point = GetSandboxCameraPoint(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
				data->camera_wasd_initial_translation = camera_point.position;
				data->camera_wasd_initial_rotation = camera_point.rotation;
				// We need this to make the camera movement smoother
				system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;

				// Disable the axes if they are drawn
				sandbox->transform_display_axes = false;
				ResetSandboxTransformToolSelectedAxes(editor_state, sandbox_index);
			}
			else {
				// Check the focus on object
				if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_FOCUS_OBJECT)) {
					FocusOnSelection(editor_state, sandbox_index);
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
							Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
							GetSandboxEntitiesMidpoint(editor_state, sandbox_index, selected_entities, &data->translation_midpoint, &data->rotation_midpoint);
						}
					}
					else {
						data->drag_tool.SetSpace(sandbox->transform_space);
						ResetSandboxTransformToolSelectedAxes(editor_state, sandbox_index);
					}
					sandbox->transform_keyboard_space = data->drag_tool.GetSpace();
					trigger_rerender_viewport = EDITOR_SANDBOX_VIEWPORT_SCENE;
					sandbox->transform_display_axes = true;
					sandbox->transform_keyboard_tool = tool;

					// Add the component type if it is missing
					AddSelectedEntitiesComponentIfMissing(editor_state, sandbox_index, tool);
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
					auto change_selected_axis = [&](ECS_TRANSFORM_TOOL_AXIS axis) {
						ResetSandboxTransformToolSelectedAxes(editor_state, sandbox_index);
						sandbox->transform_tool_selected[axis] = true;
						trigger_rerender_viewport = EDITOR_SANDBOX_VIEWPORT_SCENE;

						// Reinitialize the drag tool
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

						// Get the rotation and translation midpoints for the selected entities
						Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
						GetSandboxEntitiesMidpoint(editor_state, sandbox_index, selected_entities, &data->translation_midpoint, &data->rotation_midpoint);
						data->drag_tool.SetAxis(axis);
						data->scale_delta = float3::Splat(0.0f);
						data->original_translation_midpoint = data->translation_midpoint;
						data->rotation_delta = QuaternionIdentity();
					};

					if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_AXIS_X)) {
						change_selected_axis(ECS_TRANSFORM_AXIS_X);
					}
					else if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_AXIS_Y)) {
						change_selected_axis(ECS_TRANSFORM_AXIS_Y);
					}
					else if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_AXIS_Z)) {
						change_selected_axis(ECS_TRANSFORM_AXIS_Z);
					}

					// Check to see if we have active keyboard action
					unsigned char axes_select_count = 0;
					ECS_TRANSFORM_TOOL_AXIS axis = ECS_TRANSFORM_AXIS_COUNT;
					for (size_t index = 0; index < ECS_TRANSFORM_AXIS_COUNT; index++) {
						if (sandbox->transform_tool_selected[index]) {
							axis = (ECS_TRANSFORM_TOOL_AXIS)index;
							axes_select_count++;
						}
					}

					// We have just one axis selected
					if (axes_select_count == 1) {
						HandleSelectedEntitiesTransformUpdateDescriptor update_descriptor;
						update_descriptor.editor_state = editor_state;
						update_descriptor.mouse_position = mouse_position;
						update_descriptor.rotation_midpoint = sandbox->transform_keyboard_space == ECS_TRANSFORM_LOCAL_SPACE ? data->rotation_midpoint : QuaternionIdentity();
						update_descriptor.sandbox_index = sandbox_index;
						update_descriptor.system = system;
						update_descriptor.tool_drag = &data->drag_tool;
						update_descriptor.translation_midpoint = &data->translation_midpoint;
						update_descriptor.window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
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
				}
			}
		}

		// Remove all copied entities which are no longer valid
		Stream<Entity>& copied_entities = draw_data->copied_entities;
		for (size_t index = 0; index < copied_entities.size; index++) {
			if (!IsSandboxEntityValid(editor_state, sandbox_index, copied_entities[index])) {
				copied_entities.RemoveSwapBack(index);
				// Decrement the index as well
				index--;
			}
		}

		// Returns true if there are entities to be deleted, else false
		auto delete_current_selection = [&]() {
			Stream<Entity> current_selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
			for (size_t index = 0; index < current_selected_entities.size; index++) {
				DeleteSandboxEntity(editor_state, sandbox_index, current_selected_entities[index]);
			}
			return current_selected_entities.size > 0;
		};

		bool control_action_trigger_rerender = false;
		// Handle the Ctrl+C,X,V, D and Delete actions as well
		if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
			if (keyboard->IsPressed(ECS_KEY_C)) {
				// Make a new allocation and deallocate the old
				Stream<Entity> current_selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
				copied_entities.Resize(editor_state->EditorAllocator(), current_selected_entities.size, false, true);
				copied_entities.CopyOther(current_selected_entities);
			}
			else if (keyboard->IsPressed(ECS_KEY_X)) {
				// Delete the selected entities
				control_action_trigger_rerender = delete_current_selection();
			}
			else if (keyboard->IsPressed(ECS_KEY_V)) {
				for (size_t index = 0; index < copied_entities.size; index++) {
					CopySandboxEntity(editor_state, sandbox_index, copied_entities[index]);
				}
				control_action_trigger_rerender = copied_entities.size > 0;
			}
			else if (keyboard->IsPressed(ECS_KEY_D)) {
				Stream<Entity> current_selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
				if (current_selected_entities.size > 0) {
					for (size_t index = 0; index < current_selected_entities.size; index++) {
						// Also replace the selected entities with the new one
						current_selected_entities[index] = CopySandboxEntity(editor_state, sandbox_index, current_selected_entities[index]);
					}
					// We need to signal the selected entities counter
					SignalSandboxSelectedEntitiesCounter(editor_state, sandbox_index);
					control_action_trigger_rerender = true;
				}
			}
		}

		if (keyboard->IsPressed(ECS_KEY_DELETE)) {
			control_action_trigger_rerender = delete_current_selection();
		}
		if (control_action_trigger_rerender) {
			// We need to re-render both views
			RenderSandboxViewports(editor_state, sandbox_index);
		}
	}

	auto reset_axes_and_rerender = [&]() {
		ResetSandboxTransformToolSelectedAxes(editor_state, sandbox_index);
		sandbox->transform_display_axes = false;
		// We also need to trigger a re-render
		RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
	};

	auto reset_camera_wasd = [&]() {
		// End the camera movement
		sandbox->is_camera_wasd_movement = false;
		// Disable the mouse raw input and make the cursor visible again
		mouse->DisableRawInput();
		mouse->SetCursorVisibility(true);
	};

	// If the user has clicked somewhere and we have active axes, disable them
	if (mouse->IsPressed(ECS_MOUSE_LEFT)) {
		if (sandbox->transform_display_axes) {
			reset_axes_and_rerender();
		}
		else if (sandbox->is_camera_wasd_movement) {
			reset_camera_wasd();
		}
	}
	else if (mouse->IsPressed(ECS_MOUSE_MIDDLE) || mouse->IsPressed(ECS_MOUSE_RIGHT)) {
		if (sandbox->transform_display_axes) {
			// Restore the value to the selected entities
			Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
			switch (sandbox->transform_keyboard_tool) {
			case ECS_TRANSFORM_TRANSLATION:
			{
				// Restore the translation
				// Calculate the delta between the current translation and the original one
				float3 delta = data->translation_midpoint - data->original_translation_midpoint;

				for (size_t index = 0; index < selected_entities.size; index++) {
					Translation* translation = GetSandboxEntityComponent<Translation>(editor_state, sandbox_index, selected_entities[index]);
					translation->value -= delta;
				}
			}
			break;
			case ECS_TRANSFORM_ROTATION:
			{
				// Restore the rotation by adding the inverse delta
				for (size_t index = 0; index < selected_entities.size; index++) {
					Rotation* rotation = GetSandboxEntityComponent<Rotation>(editor_state, sandbox_index, selected_entities[index]);
					rotation->value = AddLocalRotation(rotation->value, QuaternionInverse(data->rotation_delta)).StorageLow();
				}
			}
			break;
			case ECS_TRANSFORM_SCALE:
			{
				// Restore the scale by adding the negated delta
				for (size_t index = 0; index < selected_entities.size; index++) {
					Scale* scale = GetSandboxEntityComponent<Scale>(editor_state, sandbox_index, selected_entities[index]);
					scale->value -= data->scale_delta;
				}
			}
			break;
			}

			reset_axes_and_rerender();
		}
		else if (sandbox->is_camera_wasd_movement) {
			// End the camera wasd movement and restore the previous translation and rotation
			SetSandboxCameraTranslation(editor_state, sandbox_index, data->camera_wasd_initial_translation, EDITOR_SANDBOX_VIEWPORT_SCENE, true);
			SetSandboxCameraRotation(editor_state, sandbox_index, data->camera_wasd_initial_rotation, EDITOR_SANDBOX_VIEWPORT_SCENE);

			// Rerender the sandbox again
			reset_camera_wasd();
			RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
		}
	}
}

void SceneUISetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory) {
	unsigned int index = *(unsigned int*)stack_memory;

	SceneDrawData* data = (SceneDrawData*)function::OffsetPointer(stack_memory, sizeof(unsigned int));

	memset(data, 0, sizeof(*data));
	data->editor_state = editor_state;

	descriptor.draw = SceneUIWindowDraw;

	ScenePrivateActionData* private_data = (ScenePrivateActionData*)function::OffsetPointer(data, sizeof(*data));
	private_data->editor_state = editor_state;
	descriptor.private_action = ScenePrivateAction;
	descriptor.private_action_data = private_data;
	descriptor.private_action_data_size = sizeof(*private_data);

	descriptor.destroy_action = SceneUIDestroy;
	descriptor.destroy_action_data = editor_state;

	CapacityStream<char> window_name(function::OffsetPointer(private_data, sizeof(*private_data)), 0, 128);
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
	bool is_disabled;
};

static void SceneRotationAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SceneActionData* data = (SceneActionData*)_data;
	EditorState* editor_state = data->draw_data->editor_state;
	
	// If the axes were enabled, don't perform the action

	if (mouse->IsPressed(ECS_MOUSE_RIGHT)) {
		EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_index);
		data->is_disabled = sandbox->transform_display_axes || sandbox->is_camera_wasd_movement;
		if (!data->is_disabled) {
			mouse->EnableRawInput();
			// Make the pacing instant such that we won't have a big frame difference
			// That can cause the camera to snap
			system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
		}
	}
	
	if (!data->is_disabled) {
		if (mouse->IsReleased(ECS_MOUSE_RIGHT)) {
			mouse->DisableRawInput();
		}
		else if (mouse->IsDown(ECS_MOUSE_RIGHT)) {
			system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
		}


		if (mouse->IsHeld(ECS_MOUSE_RIGHT)) {
			bool was_rotated = HandleCameraRotation(editor_state, data->sandbox_index, false, false);
			if (was_rotated) {
				RenderSandbox(editor_state, data->sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE, { 0, 0 }, true);
			}
		}
	}
}

static void SceneTranslationAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SceneActionData* data = (SceneActionData*)_data;
	EditorState* editor_state = data->draw_data->editor_state;
	if (mouse->IsPressed(ECS_MOUSE_MIDDLE)) {
		EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_index);
		data->is_disabled = sandbox->transform_display_axes || sandbox->is_camera_wasd_movement;
		if (!data->is_disabled) {
			mouse->EnableRawInput();
		}
	}
	
	if (!data->is_disabled) {
		if (mouse->IsReleased(ECS_MOUSE_MIDDLE)) {
			mouse->DisableRawInput();
		}
		else if (mouse->IsDown(ECS_MOUSE_MIDDLE)) {
			system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
		}

		if (mouse->IsHeld(ECS_MOUSE_MIDDLE)) {
			if (mouse_delta.x != 0.0f || mouse_delta.y != 0.0f) {
				Camera scene_camera = GetSandboxCamera(editor_state, data->sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
				float3 right_vector = scene_camera.GetRightVector().AsFloat3Low();
				float3 up_vector = scene_camera.GetUpVector().AsFloat3Low();

				float translation_factor = 10.0f;

				if (keyboard->IsDown(ECS_KEY_LEFT_SHIFT)) {
					translation_factor = 2.5f;
				}
				else if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
					translation_factor = 30.0f;
				}

				float3 translation = right_vector * float3::Splat(-mouse_delta.x * translation_factor) + up_vector * float3::Splat(mouse_delta.y * translation_factor);
				TranslateSandboxCamera(editor_state, data->sandbox_index, translation, EDITOR_SANDBOX_VIEWPORT_SCENE);
				RenderSandbox(editor_state, data->sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE, { 0, 0 }, true);
			}
		}
	}
}

static void SceneZoomAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SceneActionData* data = (SceneActionData*)_data;

	EditorState* editor_state = data->draw_data->editor_state;
	const EditorSandbox* sandbox = GetSandbox(editor_state, data->sandbox_index);
	
	if (!sandbox->is_camera_wasd_movement) {
		int scroll_delta = mouse->GetScrollDelta();
		if (scroll_delta != 0) {
			float factor = 0.015f;

			if (keyboard->IsDown(ECS_KEY_LEFT_SHIFT)) {
				factor = 0.004f;
			}
			else if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
				factor = 0.06f;
			}

			float3 camera_rotation = GetSandboxCameraPoint(editor_state, data->sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE).rotation;
			float3 forward_vector = GetSandboxCamera(editor_state, data->sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE).GetForwardVector().AsFloat3Low();
			//float3 forward_vector = RotateVectorMatrixLow(GetForwardVector(), MatrixRotation(camera_rotation));

			TranslateSandboxCamera(editor_state, data->sandbox_index, forward_vector * float3::Splat(scroll_delta * factor), EDITOR_SANDBOX_VIEWPORT_SCENE);
			RenderSandbox(editor_state, data->sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE, { 0, 0 }, true);
		}
	}
}

struct SceneLeftClickableActionData {
	EditorState* editor_state;
	unsigned int sandbox_index;
	uint2 click_texel_position;
	float2 click_ui_position;
	bool is_selection_mode;
	bool was_keyboard_transform_on_click;

	ECS_TRANSFORM_TOOL_AXIS tool_axis;
	// This is cached such that it doesn't need to be calculated each time
	float3 gizmo_translation_midpoint;
	Quaternion gizmo_rotation_midpoint;
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
	unsigned int sandbox_index = data->sandbox_index;

	// Check to see if the mouse moved
	unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
	uint2 hovered_texel_offset = system->GetWindowTexelPositionClamped(window_index, mouse_position);

	if (mouse->IsPressed(ECS_MOUSE_LEFT)) {
		// Disable for this sandbox the selectable axes from keyboard hotkeys
		EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
		data->was_keyboard_transform_on_click = sandbox->transform_display_axes;
		sandbox->transform_display_axes = false;
		ResetSandboxTransformToolSelectedAxes(editor_state, sandbox_index);

		if (!data->was_keyboard_transform_on_click) {
			data->click_ui_position = mouse_position;
			data->click_texel_position = system->GetMousePositionHoveredWindowTexelPosition();
			data->is_selection_mode = false;
			data->original_selection.InitializeAndCopy(editor_state->EditorAllocator(), GetSandboxSelectedEntities(editor_state, sandbox_index));
			data->cpu_framebuffer = { nullptr, {0, 0} };
			mouse->EnableRawInput();

			DisableSandboxViewportRendering(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);

			if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
				RenderTargetView instanced_view = GetSandboxInstancedFramebuffer(editor_state, sandbox_index);
				data->cpu_framebuffer = TransferInstancesFramebufferToCPUAndAllocate(editor_state->RuntimeGraphics(), instanced_view, { nullptr });

				// Get an initial entity selected and see if a gizmo was selected
				// Only a single selection
				ECS_STACK_CAPACITY_STREAM(unsigned int, selected_entity_instance, 1);
				GetInstancesFromFramebufferFilteredCPU(data->cpu_framebuffer, hovered_texel_offset, hovered_texel_offset + uint2(1, 1), &selected_entity_instance);
				// Check for the case nothing is selected
				Entity selected_entity = selected_entity_instance.size == 0 ? (unsigned int)-1 : selected_entity_instance[0];
				// Check to see if this a gizmo Entity
				EDITOR_SANDBOX_ENTITY_SLOT entity_slot = FindSandboxVirtualEntitySlotType(editor_state, sandbox_index, selected_entity);
				if (entity_slot != EDITOR_SANDBOX_ENTITY_SLOT_COUNT) {
					switch (entity_slot) {
					case EDITOR_SANDBOX_ENTITY_SLOT_TRANSFORM_X:
						data->tool_axis = ECS_TRANSFORM_AXIS_X;
						break;
					case EDITOR_SANDBOX_ENTITY_SLOT_TRANSFORM_Y:
						data->tool_axis = ECS_TRANSFORM_AXIS_Y;
						break;
					case EDITOR_SANDBOX_ENTITY_SLOT_TRANSFORM_Z:
						data->tool_axis = ECS_TRANSFORM_AXIS_Z;
						break;
					}

					if (data->tool_axis != ECS_TRANSFORM_AXIS_COUNT) {
						ResetSandboxTransformToolSelectedAxes(editor_state, sandbox_index);
						sandbox->transform_tool_selected[data->tool_axis] = true;

						ECS_TRANSFORM_TOOL transform_tool = sandbox->transform_tool;

						data->transform_drag.Initialize(transform_tool);
						data->transform_drag.SetAxis(data->tool_axis);
						data->transform_drag.SetSpace(sandbox->transform_space);

						// Add the component type if it is missing
						AddSelectedEntitiesComponentIfMissing(editor_state, sandbox_index, transform_tool);

						// If the entity is missing the corresponding component, add it
						Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);

						// Calculate the midpoint here such that it won't need to be calculated each frame
						GetSandboxEntitiesMidpoint(editor_state, sandbox_index, selected_entities, &data->gizmo_translation_midpoint, &data->gizmo_rotation_midpoint);
					}
				}
			}
		}
		else {
			// Render the viewport once since we need to update the visuals that the keyboard axes are disabled
			RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
		}
	}
	else if (!data->was_keyboard_transform_on_click) {
		if (data->tool_axis == ECS_TRANSFORM_AXIS_COUNT) {
			// Check to see if the RuntimeGraphics is available for copying to the CPU the values
			if (data->cpu_framebuffer.values == nullptr) {
				if (!EditorStateHasFlag(editor_state, EDITOR_STATE_PREVENT_RESOURCE_LOADING)) {
					RenderTargetView instanced_view = GetSandboxInstancedFramebuffer(editor_state, sandbox_index);
					data->cpu_framebuffer = TransferInstancesFramebufferToCPUAndAllocate(editor_state->RuntimeGraphics(), instanced_view, { nullptr });
				}
			}

			// Check to see if the mouse moved
			unsigned int window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
			uint2 hovered_texel_offset = system->GetWindowTexelPositionClamped(window_index, mouse_position);
			if (!data->is_selection_mode) {
				float2 mouse_difference = AbsoluteDifference(mouse_position, data->click_ui_position);
				if (mouse_difference.x > CLICK_SELECTION_MARGIN_X || mouse_difference.y > CLICK_SELECTION_MARGIN_Y) {
					data->is_selection_mode = true;
				}
			}

			if (data->cpu_framebuffer.values != nullptr) {
				if (data->is_selection_mode) {
					system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
					ECS_STACK_CAPACITY_STREAM(unsigned int, selected_entities, ECS_KB * 16);
					RenderTargetView instanced_view = GetSandboxInstancedFramebuffer(editor_state, sandbox_index);
					uint2 top_left = BasicTypeMin(hovered_texel_offset, data->click_texel_position);
					uint2 bottom_right = BasicTypeMax(hovered_texel_offset, data->click_texel_position);

					if (top_left.x != bottom_right.x && top_left.y != bottom_right.y) {
						GetInstancesFromFramebufferFilteredCPU(data->cpu_framebuffer, top_left, bottom_right, &selected_entities);
						Stream<Entity> selected_entities_stream = { selected_entities.buffer, selected_entities.size };
						FilterSandboxEntitiesValid(editor_state, sandbox_index, &selected_entities_stream);

						if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
							ChangeSandboxSelectedEntities(editor_state, sandbox_index, data->original_selection);

							// Reduce the selection by these entities
							for (size_t index = 0; index < selected_entities_stream.size; index++) {
								// This checks to see that the entity exists and 
								RemoveSandboxSelectedEntity(editor_state, sandbox_index, selected_entities_stream[index]);
							}
						}
						else if (keyboard->IsDown(ECS_KEY_LEFT_SHIFT)) {
							// Add to the sxisting selection
							ChangeSandboxSelectedEntities(editor_state, sandbox_index, data->original_selection);

							// Add to the selection these entities that do not exist
							for (size_t index = 0; index < selected_entities_stream.size; index++) {
								if (!IsSandboxEntitySelected(editor_state, sandbox_index, selected_entities_stream[index])) {
									AddSandboxSelectedEntity(editor_state, sandbox_index, selected_entities_stream[index]);
								}
							}
						}
						else {
							if (selected_entities_stream.size > 0) {
								// We can reinterpret the unsigned ints as entities 
								ChangeSandboxSelectedEntities(editor_state, sandbox_index, selected_entities_stream);
							}
							else {
								ClearSandboxSelectedEntities(editor_state, sandbox_index);
							}
						}
					}

					float2 selection_top_left = BasicTypeMin(mouse_position, data->click_ui_position);
					float2 selection_bottom_right = BasicTypeMax(mouse_position, data->click_ui_position);
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
						counts,
						selection_color,
						{ 0.0f, 0.0f },
						{ 1.0f, 1.0f },
						ECS_UI_DRAW_LATE
					);

					float2 border_scale = { ECS_TOOLS_UI_ONE_PIXEL_X * 2, ECS_TOOLS_UI_ONE_PIXEL_Y * 2 };
					CreateSolidColorRectangleBorder<false>(
						selection_top_left,
						selection_scale,
						border_scale,
						selection_color,
						counts,
						buffers
					);
				}
				else if (mouse->IsReleased(ECS_MOUSE_LEFT)) {
					// Only a single selection
					RenderTargetView instanced_view = GetSandboxInstancedFramebuffer(editor_state, sandbox_index);
					ECS_STACK_CAPACITY_STREAM(unsigned int, selected_entity_instance, 1);
					GetInstancesFromFramebufferFilteredCPU(data->cpu_framebuffer, hovered_texel_offset, hovered_texel_offset + uint2(1, 1), &selected_entity_instance);
					// Check for the case nothing is selected
					Entity selected_entity = selected_entity_instance.size == 0 ? (unsigned int)-1 : selected_entity_instance[0];
					// Check to see if this a gizmo Entity
					EDITOR_SANDBOX_ENTITY_SLOT entity_slot = FindSandboxVirtualEntitySlotType(editor_state, sandbox_index, selected_entity);
					if (entity_slot == EDITOR_SANDBOX_ENTITY_SLOT_COUNT) {
						// We have selected an actual entity, not a gizmo or some other pseudo entity
						if (keyboard->IsDown(ECS_KEY_LEFT_CTRL) || keyboard->IsDown(ECS_KEY_LEFT_SHIFT)) {
							if (IsSandboxEntityValid(editor_state, sandbox_index, selected_entity)) {
								// See if the entity exists or not
								if (IsSandboxEntitySelected(editor_state, sandbox_index, selected_entity)) {
									RemoveSandboxSelectedEntity(editor_state, sandbox_index, selected_entity);
								}
								else {
									AddSandboxSelectedEntity(editor_state, sandbox_index, selected_entity);
								}
							}
						}
						else {
							if (IsSandboxEntityValid(editor_state, sandbox_index, selected_entity)) {
								ChangeSandboxSelectedEntities(editor_state, sandbox_index, { &selected_entity, 1 });
							}
							else {
								ClearSandboxSelectedEntities(editor_state, sandbox_index);
							}
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
			update_descriptor.sandbox_index = sandbox_index;
			update_descriptor.system = system;
			update_descriptor.tool_drag = &data->transform_drag;
			update_descriptor.translation_midpoint = &data->gizmo_translation_midpoint;
			update_descriptor.window_index = system->GetWindowIndexFromBorder(dockspace, border_index);
			update_descriptor.tool_to_use = GetSandbox(editor_state, sandbox_index)->transform_tool;

			bool should_render_runtime = HandleSelectedEntitiesTransformUpdate(&update_descriptor);
			if (should_render_runtime) {
				RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_RUNTIME);
			}
		}

		if (mouse->IsReleased(ECS_MOUSE_LEFT)) {
			data->original_selection.Deallocate(editor_state->EditorAllocator());
			if (data->cpu_framebuffer.values != nullptr) {
				data->cpu_framebuffer.Deallocate({ nullptr });
			}
			// We need to reset any selected tool info
			EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
			ResetSandboxTransformToolSelectedAxes(editor_state, sandbox_index);
		}

		EnableSandboxViewportRendering(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
		// We need to render one more time in order for the transform tool to appear normal
		RenderSandbox(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
		system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
		if (!mouse->IsReleased(ECS_MOUSE_LEFT)) {
			DisableSandboxViewportRendering(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
		}
		mouse->DisableRawInput();
	}
}

void SceneUIWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	SceneDrawData* data = (SceneDrawData*)window_data;
	EditorState* editor_state = data->editor_state;

	unsigned int sandbox_index = GetWindowNameIndex(drawer.system->GetWindowName(drawer.window_index));
	EDITOR_SANDBOX_VIEWPORT current_viewport = EDITOR_SANDBOX_VIEWPORT_SCENE;
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

			SceneLeftClickableActionData left_clickable_data;
			left_clickable_data.editor_state = editor_state;
			left_clickable_data.sandbox_index = sandbox_index;
			left_clickable_data.click_ui_position = { FLT_MAX, FLT_MAX };
			left_clickable_data.is_selection_mode = false;
			left_clickable_data.tool_axis = ECS_TRANSFORM_AXIS_COUNT;
			// Set the phase to late to have the selection border be drawn over the main sprite
			UIActionHandler selection_handler = { SceneLeftClickableAction, &left_clickable_data, sizeof(left_clickable_data), ECS_UI_DRAW_LATE };
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
	name.CopyOther(SCENE_WINDOW_NAME);
	function::ConvertIntToChars(name, index);
}

unsigned int GetSceneUIWindowIndex(const EditorState* editor_state, unsigned int sandbox_index)
{
	ECS_STACK_CAPACITY_STREAM(char, window_name, 512);
	GetSceneUIWindowName(sandbox_index, window_name);
	return editor_state->ui_system->GetWindowFromName(window_name);
}

bool DisableSceneUIRendering(EditorState* editor_state, unsigned int sandbox_index)
{
	unsigned int scene_window_index = GetSceneUIWindowIndex(editor_state, sandbox_index);
	if (scene_window_index != -1) {
		bool is_scene_visible = editor_state->ui_system->IsWindowVisible(scene_window_index);
		if (is_scene_visible) {
			DisableSandboxViewportRendering(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
			return true;
		}
	}
	return false;
}

bool EnableSceneUIRendering(EditorState* editor_state, unsigned int sandbox_index)
{
	unsigned int scene_window_index = GetSceneUIWindowIndex(editor_state, sandbox_index);
	if (scene_window_index != -1) {
		bool is_scene_visible = editor_state->ui_system->IsWindowVisible(scene_window_index);
		if (is_scene_visible) {
			EnableSandboxViewportRendering(editor_state, sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
			return true;
		}
	}
	return false;
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
