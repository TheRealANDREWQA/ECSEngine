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
#include "ECSEngineComponents.h"
#include "../Editor/EditorInputMapping.h"

// These defined the bounds under which the mouse
// is considered that it clicked and not selected yet
#define CLICK_SELECTION_MARGIN_X 0.01f
#define CLICK_SELECTION_MARGIN_Y 0.01f

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

void ScenePrivateAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	EditorState* editor_state = (EditorState*)_data;
	unsigned int sandbox_index = GetWindowNameIndex(system->GetWindowName(system->GetWindowIndexFromBorder(dockspace, border_index)));
	// Determine if the transform tool needs to be changed
	unsigned int target_sandbox = GetActiveWindowSandbox(editor_state);
	// If values are being entered into a field don't change the tool
	if (target_sandbox == sandbox_index && !editor_state->Keyboard()->IsCaptureCharacters()) {
		EditorSandbox* sandbox = GetSandbox(editor_state, target_sandbox);
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

		bool trigger_rerender = false;
		if (editor_state->input_mapping.IsTriggered(EDITOR_INPUT_CHANGE_TRANSFORM_SPACE)) {
			sandbox->transform_space = InvertTransformSpace(sandbox->transform_space);
			trigger_rerender = true;
		}

		auto initiate_transform = [&](ECS_TRANSFORM_TOOL tool) {
			// Check to see if the same transform was pressed to change the space
			if (sandbox->transform_tool == tool && sandbox->transform_display_axes) {
				sandbox->transform_keyboard_press_count++;
			}
			else {
				sandbox->transform_keyboard_press_count = 1;
			}
			trigger_rerender = true;
			sandbox->transform_display_axes = true;
			ResetSandboxTransformToolSelectedAxes(editor_state, sandbox_index);
			sandbox->transform_tool = tool;
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
				trigger_rerender = true;
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
		}

		if (current_tool != sandbox->transform_tool || trigger_rerender) {
			RenderSandbox(editor_state, target_sandbox, EDITOR_SANDBOX_VIEWPORT_SCENE);
		}
	}
}

void SceneUISetDecriptor(UIWindowDescriptor& descriptor, EditorState* editor_state, void* stack_memory) {
	unsigned int index = *(unsigned int*)stack_memory;

	SceneDrawData* data = (SceneDrawData*)function::OffsetPointer(stack_memory, sizeof(unsigned int));

	memset(data, 0, sizeof(*data));
	data->editor_state = editor_state;

	descriptor.draw = SceneUIWindowDraw;

	descriptor.private_action = ScenePrivateAction;
	descriptor.private_action_data = editor_state;

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
			RotateSandboxCamera(editor_state, data->sandbox_index, rotation, EDITOR_SANDBOX_VIEWPORT_SCENE);
			RenderSandbox(editor_state, data->sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
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
	EDITOR_SANDBOX_VIEWPORT current_viewport = GetSandboxActiveViewport(editor_state, data->sandbox_index);

	if (mouse->IsDown(ECS_MOUSE_MIDDLE)) {
		float2 mouse_position = system->GetNormalizeMousePosition();
		float2 delta = system->GetMouseDelta(mouse_position);

		Camera scene_camera = GetSandboxSceneCamera(editor_state, data->sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE);
		float3 right_vector = scene_camera.GetRightVector().AsFloat3Low();
		float3 up_vector = scene_camera.GetUpVector().AsFloat3Low();

		float translation_factor = 10.0f;

		if (keyboard->IsDown(ECS_KEY_LEFT_SHIFT)) {
			translation_factor = 2.5f;
		}
		else if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
			translation_factor = 30.0f;
		}

		float3 translation = right_vector * float3::Splat(-delta.x * translation_factor) + up_vector * float3::Splat(delta.y * translation_factor);
		if (translation.x != 0.0f || translation.y != 0.0f || translation.z != 0.0f) {
			TranslateSandboxCamera(editor_state, data->sandbox_index, translation, EDITOR_SANDBOX_VIEWPORT_SCENE);
			RenderSandbox(editor_state, data->sandbox_index, current_viewport);
		}
	}
}

void SceneZoomAction(ActionData* action_data) {
	UI_UNPACK_ACTION_DATA;

	SceneActionData* data = (SceneActionData*)_data;

	EditorState* editor_state = data->draw_data->editor_state;
	
	// Zoom start
	int scroll_delta = mouse->GetScrollDelta();
	if (scroll_delta != 0) {
		EDITOR_SANDBOX_VIEWPORT current_viewport = GetSandboxActiveViewport(editor_state, data->sandbox_index);
		float factor = 0.015f;

		if (keyboard->IsDown(ECS_KEY_LEFT_SHIFT)) {
			factor = 0.004f;
		}
		else if (keyboard->IsDown(ECS_KEY_LEFT_CTRL)) {
			factor = 0.06f;
		}

		float3 camera_rotation = GetSandboxCameraPoint(editor_state, data->sandbox_index, EDITOR_SANDBOX_VIEWPORT_SCENE).rotation;
		float3 forward_vector = RotateVectorLow(camera_rotation, GetForwardVector());

		TranslateSandboxCamera(editor_state, data->sandbox_index, forward_vector * float3::Splat(scroll_delta * factor), EDITOR_SANDBOX_VIEWPORT_SCENE);
		RenderSandbox(editor_state, data->sandbox_index, current_viewport);
	}
	// Zoom end
}

struct SceneLeftClickableActionData {
	EditorState* editor_state;
	unsigned int sandbox_index;
	uint2 click_texel_position;
	float2 click_ui_position;
	bool is_selection_mode;

	ECS_TRANSFORM_TOOL_AXIS tool_axis;
	// This is cached such that it doesn't need to be calculated each time
	float3 gizmo_translation_midpoint;
	Quaternion gizmo_rotation_midpoint;
	TransformToolDrag transform_drag;
	
	CPUInstanceFramebuffer cpu_framebuffer;

	// This is used for the selection mode to add/disable entities
	Stream<Entity> original_selection;
};

void SceneLeftClickableAction(ActionData* action_data) {
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
		sandbox->transform_display_axes = false;
		sandbox->transform_keyboard_press_count = 0;
		ResetSandboxTransformToolSelectedAxes(editor_state, sandbox_index);

		data->click_ui_position = mouse_position;
		data->click_texel_position = system->GetMousePositionHoveredWindowTexelPosition();
		data->is_selection_mode = false;
		data->original_selection.InitializeAndCopy(editor_state->EditorAllocator(), GetSandboxSelectedEntities(editor_state, sandbox_index));
		data->cpu_framebuffer = { nullptr, {0, 0} };
		mouse->EnableRawInput();

		DisableSandboxViewportRendering(editor_state, sandbox_index);

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
			EDITOR_SANDBOX_ENTITY_SLOT entity_slot = FindSandboxUnusedEntitySlotType(editor_state, sandbox_index, selected_entity);
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

					// If the entity is missing the corresponding component, add it
					Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
					Stream<char> component_name;
					Component component_id;
					switch (transform_tool) {
					case ECS_TRANSFORM_TRANSLATION:
					{
						component_name = STRING(Translation);
						component_id = Translation::ID();
						data->transform_drag.translation.Initialize();
					}
					break;
					case ECS_TRANSFORM_ROTATION:
					{
						component_name = STRING(Rotation);
						component_id = Rotation::ID();
						data->transform_drag.rotation.InitializeCircle();
					}
					break;
					case ECS_TRANSFORM_SCALE:
					{
						component_name = STRING(Scale);
						component_id = Scale::ID();
						data->transform_drag.scale.Initialize();
					}
					break;
					}
					data->transform_drag.SetAxis(data->tool_axis);
					data->transform_drag.SetSpace(sandbox->transform_space);

					// Calculate the midpoint here such that it won't need to be calculated each frame
					for (size_t index = 0; index < selected_entities.size; index++) {
						const void* component = GetSandboxEntityComponentEx(editor_state, sandbox_index, selected_entities[index], component_id, false);
						if (component == nullptr) {
							// Add the component to the entity
							AddSandboxEntityComponent(editor_state, sandbox_index, selected_entities[index], component_name);
						}
					}

					data->gizmo_translation_midpoint = float3::Splat(0.0f);
					data->gizmo_rotation_midpoint = QuaternionAverageCumulatorInitialize();
					for (size_t index = 0; index < selected_entities.size; index++) {
						const Translation* translation = GetSandboxEntityComponent<Translation>(editor_state, sandbox_index, selected_entities[index]);
						if (translation != nullptr) {
							data->gizmo_translation_midpoint += translation->value;
						}
						const Rotation* rotation = GetSandboxEntityComponent<Rotation>(editor_state, sandbox_index, selected_entities[index]);
						if (rotation != nullptr) {
							QuaternionAddToAverageStep(&data->gizmo_rotation_midpoint, rotation->value);
						}
					}
					float3 count_inverse = float3::Splat(1.0f / (float)selected_entities.size);
					data->gizmo_translation_midpoint *= count_inverse;
					data->gizmo_rotation_midpoint = QuaternionAverageFromCumulation(data->gizmo_rotation_midpoint, selected_entities.size);
				}
			}
		}
	}
	else {
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
					EDITOR_SANDBOX_ENTITY_SLOT entity_slot = FindSandboxUnusedEntitySlotType(editor_state, sandbox_index, selected_entity);
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
			EditorSandbox* sandbox = GetSandbox(editor_state, sandbox_index);
			Camera camera = sandbox->camera_parameters[EDITOR_SANDBOX_VIEWPORT_SCENE];
			int2 unclampped_texel_position = system->GetWindowTexelPositionEx(window_index, mouse_position);
			uint2 viewport_dimensions = system->GetWindowTexelSize(window_index);

			Stream<Entity> selected_entities = GetSandboxSelectedEntities(editor_state, sandbox_index);
			switch (sandbox->transform_tool) {
			case ECS_TRANSFORM_TRANSLATION:
			{
				float3 mouse_ray_direction = MouseRayDirection(&camera, viewport_dimensions, unclampped_texel_position);
				float3 translation_delta = HandleTranslationToolDelta(
					&camera,
					data->gizmo_translation_midpoint,
					data->gizmo_rotation_midpoint,
					&data->transform_drag.translation,
					mouse_ray_direction
				);

				for (size_t index = 0; index < selected_entities.size; index++) {
					Translation* translation = GetSandboxEntityComponent<Translation>(editor_state, sandbox_index, selected_entities[index]);
					translation->value += translation_delta;
				}
				// Also translate the midpoint along
				data->gizmo_translation_midpoint += translation_delta;
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
					data->gizmo_translation_midpoint,
					data->gizmo_rotation_midpoint,
					&data->transform_drag.rotation,
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
					data->gizmo_translation_midpoint,
					data->gizmo_rotation_midpoint,
					&data->transform_drag.scale,
					system->GetTexelMouseDelta(),
					factor
				);

				if (scale_delta != float3::Splat(0.0f)) {
					for (size_t index = 0; index < selected_entities.size; index++) {
						Scale* scale = GetSandboxEntityComponent<Scale>(editor_state, sandbox_index, selected_entities[index]);
						scale->value += scale_delta;
					}
				}
			}
			break;
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

		EnableSandboxViewportRendering(editor_state, sandbox_index);
		// We need to render one more time in order for the transform tool to appear normal
		RenderSandbox(editor_state, sandbox_index, GetSandboxActiveViewport(editor_state, sandbox_index));
		system->m_frame_pacing = ECS_UI_FRAME_PACING_INSTANT;
		if (!mouse->IsReleased(ECS_MOUSE_LEFT)) {
			DisableSandboxViewportRendering(editor_state, sandbox_index);
		}
		mouse->DisableRawInput();
	}
}

void SceneUIWindowDraw(void* window_data, UIDrawerDescriptor* drawer_descriptor, bool initialize) {
	UI_PREPARE_DRAWER(initialize);

	SceneDrawData* data = (SceneDrawData*)window_data;
	EditorState* editor_state = data->editor_state;

	unsigned int sandbox_index = GetWindowNameIndex(drawer.system->GetWindowName(drawer.window_index));
	EDITOR_SANDBOX_VIEWPORT current_viewport = GetSandboxActiveViewport(editor_state, sandbox_index);
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
