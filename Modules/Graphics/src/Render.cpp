#include "pch.h"
#include "Render.h"
#include "GraphicsComponents.h"
#include "ECSEngineComponents.h"
#include "ECSEngineRuntime.h"
#include "ECSEngineShaderApplicationStage.h"
#include "ECSEngineDebugDrawer.h"
#include "ECSEngineCBufferTags.h"
#include "ECSEngineMath.h"
#include "Debug.h"

using namespace ECSEngine;

#define GIZMO_THICKNESS 4

ECS_THREAD_TASK(InitializeDrawPipeline) {
	world->graphics->ClearRenderTarget(world->graphics->GetBoundRenderTarget(), ColorFloat(0.25f, 0.3f, 0.5f, 1.0f));
	world->graphics->ClearDepth(world->graphics->GetBoundDepthStencil());
	world->graphics->EnableDepth();
	world->graphics->DisableAlphaBlending();
}

ECS_INLINE unsigned int GizmoRenderIndex(unsigned int instance_index, bool extra_thick = true) {
	return GenerateRenderInstanceValue(instance_index, extra_thick ? ECS_GENERATE_INSTANCE_FRAMEBUFFER_MAX_PIXEL_THICKNESS : GIZMO_THICKNESS);
}

struct DrawMeshTaskData {
	Matrix camera_matrix;
	float3 camera_translation;
	const GraphicsDebugData* debug_data;
};

template<bool check_for_debug>
static void DrawMeshTask(
	ForEachEntityData* for_each_data,
	const RenderMesh* render_mesh,
	const Translation* translation,
	const Rotation* rotation,
	const Scale* scale
) {
	DrawMeshTaskData* data = (DrawMeshTaskData*)for_each_data->user_data;

	if constexpr (check_for_debug) {
		if (data->debug_data->entities_table.Find(for_each_data->entity) != -1) {
			// Skip the entry if it is drawn as debug
			return;
		}
	}

	Graphics* graphics = for_each_data->world->graphics;

	if (render_mesh->Validate()) {
		float3 translation_value = { 0.0f, 0.0f, 0.0f };
		float4 rotation_value = QuaternionIdentityScalar();
		float3 scale_value = { 1.0f, 1.0f, 1.0f };
		Matrix matrix_translation;
		Matrix matrix_rotation;
		Matrix matrix_scale;

		if (translation != nullptr) {
			translation_value = translation->value;
			matrix_translation = MatrixTranslation(translation_value);
		}
		else {
			matrix_translation = MatrixIdentity();
		}

		if (rotation != nullptr) {
			rotation_value = rotation->value;
			matrix_rotation = QuaternionToMatrix(rotation_value);
		}
		else {
			matrix_rotation = MatrixIdentity();
		}

		if (scale != nullptr) {
			scale_value = scale->value;
			matrix_scale = MatrixScale(scale_value);
		}
		else {
			matrix_scale = MatrixScale(float3::Splat(0.999f));
			//matrix_scale = MatrixIdentity();
		}

		Matrix object_matrix = MatrixTRS(matrix_translation, matrix_rotation, matrix_scale);
		Matrix mvp_matrix = object_matrix * data->camera_matrix;

		object_matrix = MatrixGPU(object_matrix);
		mvp_matrix = MatrixGPU(mvp_matrix);

		float3 camera_position = data->camera_translation;
		const void* injected_values[ECS_CB_INJECT_TAG_COUNT];
		injected_values[ECS_CB_INJECT_CAMERA_POSITION] = &camera_position;
		injected_values[ECS_CB_INJECT_MVP_MATRIX] = &mvp_matrix;
		injected_values[ECS_CB_INJECT_OBJECT_MATRIX] = &object_matrix;

		BindConstantBufferInjectedCB(graphics, render_mesh->material, injected_values);

		graphics->BindMesh(render_mesh->mesh->mesh);
		graphics->BindMaterial(*render_mesh->material);

		graphics->DrawCoalescedMeshCommand(*render_mesh->mesh);
	}
}

template<bool schedule_element>
ECS_THREAD_TASK(DrawMeshes) {
	auto kernel = ForEachEntityCommit<
		schedule_element,
		QueryRead<RenderMesh>,
		QueryOptional<QueryRead<Translation>>,
		QueryOptional<QueryRead<Rotation>>,
		QueryOptional<QueryRead<Scale>>
	>(thread_id, world);

	if constexpr (!schedule_element) {
		CameraCached camera;
		if (GetWorldCamera(world, camera)) {
			Matrix camera_matrix = camera.GetViewProjectionMatrix();
			float3 camera_translation = camera.translation;
			world->debug_drawer->UpdateCameraMatrix(camera_matrix);

			const GraphicsDebugData* debug_data = GetGraphicsDebugData(world);
			DrawMeshTaskData draw_data = { camera_matrix, camera_translation, debug_data };
			auto draw_function = DrawMeshTask<false>;
			if (debug_data != nullptr && debug_data->groups.size > 0) {
				draw_function = DrawMeshTask<true>;
			}
			kernel.Function(draw_function, &draw_data, sizeof(draw_data));
		}
	}
}

ECS_THREAD_TASK_TEMPLATE_BOOL(DrawMeshes);

template<bool schedule_element>
ECS_THREAD_TASK(DrawSelectables) {
	if constexpr (!schedule_element) {
		SystemManager* system_manager = world->system_manager;
		EntityManager* entity_manager = world->entity_manager;
		ECS_EDITOR_RUNTIME_TYPE runtime_type = GetEditorRuntimeType(system_manager);
		if (runtime_type != ECS_EDITOR_RUNTIME_TYPE_COUNT) {
			CameraCached camera;
			if (GetWorldCamera(world, camera)) {
				Color select_color = GetEditorRuntimeSelectColor(system_manager);
				Stream<Entity> selected_entities = GetEditorRuntimeSelectedEntities(system_manager);

				Stream<TransformGizmo> transform_gizmos;
				GetEditorExtraTransformGizmos(system_manager, &transform_gizmos);

				if (selected_entities.size + transform_gizmos.size > 0) {
					Matrix camera_matrix = camera.GetViewProjectionMatrix();
					world->debug_drawer->UpdateCameraMatrix(camera_matrix);
					// Highlight the entities - if they have meshes
					size_t valid_entities = 0;
					for (size_t index = 0; index < selected_entities.size; index++) {
						const RenderMesh* render_mesh = nullptr;
						if (entity_manager->ExistsEntity(selected_entities[index])) {
							render_mesh = entity_manager->TryGetComponent<RenderMesh>(selected_entities[index]);
						}
						valid_entities += render_mesh != nullptr && render_mesh->Validate();
					}

					ECSTransformToolEx transform_tool = GetEditorRuntimeTransformToolEx(system_manager);

					float3 translation_midpoint = { 0.0f, 0.0f, 0.0f };
					QuaternionScalar rotation_midpoint = QuaternionAverageCumulatorInitializeScalar();
					size_t total_valid_count = valid_entities + transform_gizmos.size;
					if (total_valid_count > 0) {
						HighlightObjectElement* highlight_elements = nullptr;
						size_t allocate_size = sizeof(highlight_elements[0]) * valid_entities;
						bool stack_allocated = false;
						if (allocate_size < ECS_KB * 64) {
							// Use stack allocation
							highlight_elements = (HighlightObjectElement*)ECS_STACK_ALLOC(allocate_size);
							stack_allocated = true;
						}
						else {
							highlight_elements = (HighlightObjectElement*)world->memory->Allocate(allocate_size);
						}

						if (valid_entities > 0) {
							for (size_t index = 0; index < selected_entities.size; index++) {
								const RenderMesh* render_mesh = entity_manager->TryGetComponent<RenderMesh>(selected_entities[index]);
								if (render_mesh != nullptr && render_mesh->Validate()) {
									highlight_elements[index].base.is_submesh = false;
									highlight_elements[index].base.mesh = &render_mesh->mesh->mesh;

									Matrix entity_matrix;
									const Translation* translation = entity_manager->TryGetComponent<Translation>(selected_entities[index]);
									const Rotation* rotation = entity_manager->TryGetComponent<Rotation>(selected_entities[index]);
									const Scale* scale = entity_manager->TryGetComponent<Scale>(selected_entities[index]);

									if (translation != nullptr) {
										Matrix translation_matrix = MatrixTranslation(translation->value);
										if (rotation != nullptr) {
											Matrix rotation_matrix = QuaternionToMatrix(rotation->value);
											if (scale != nullptr) {
												entity_matrix = MatrixTRS(translation_matrix, rotation_matrix, MatrixScale(scale->value));
											}
											else {
												entity_matrix = MatrixTR(translation_matrix, rotation_matrix);
											}
											if (transform_tool.space == ECS_TRANSFORM_LOCAL_SPACE) {
												QuaternionAddToAverageStep(&rotation_midpoint, rotation->value);
											}
										}
										else if (scale != nullptr) {
											entity_matrix = MatrixTS(translation_matrix, MatrixScale(scale->value));
										}
										else {
											entity_matrix = translation_matrix;
										}
										translation_midpoint += translation->value;
									}
									else if (rotation != nullptr) {
										Matrix rotation_matrix = QuaternionToMatrix(rotation->value);
										if (scale != nullptr) {
											entity_matrix = MatrixRS(rotation_matrix, MatrixScale(scale->value));
										}
										else {
											entity_matrix = rotation_matrix;
										}
										if (transform_tool.space == ECS_TRANSFORM_LOCAL_SPACE) {
											QuaternionAddToAverageStep(&rotation_midpoint, rotation->value);
										}
									}
									else if (scale != nullptr) {
										entity_matrix = MatrixScale(scale->value);
									}
									else {
										entity_matrix = MatrixIdentity();
									}

									highlight_elements[index].base.gpu_mvp_matrix = MatrixMVPToGPU(entity_matrix, camera_matrix);
								}
							}


							HighlightObject(world->graphics, select_color, { highlight_elements, valid_entities });
						}

						// Now the transform gizmos
						for (size_t index = 0; index < transform_gizmos.size; index++) {
							translation_midpoint += transform_gizmos[index].position;
							QuaternionAddToAverageStep(&rotation_midpoint, transform_gizmos[index].rotation);
						}

						if (!stack_allocated) {
							world->memory->Deallocate(highlight_elements);
						}

						translation_midpoint /= float3::Splat((float)total_valid_count);
						if (transform_tool.space == ECS_TRANSFORM_LOCAL_SPACE) {
							rotation_midpoint = QuaternionAverageFromCumulator(rotation_midpoint, total_valid_count);
						}
						else {
							rotation_midpoint = QuaternionIdentityScalar();
						}
						DebugDrawer* debug_drawer = world->debug_drawer;

						float3 camera_distance = translation_midpoint - camera.translation;
						float distance = Length(camera_distance);			

						float constant_viewport_size = GetConstantObjectSizeInPerspective(camera.fov, distance, 0.2f);

						// Now display the aggregate tool
						DebugDrawOptions debug_options;
						debug_options.ignore_depth = true;
						debug_options.wireframe = false;

						Color gray_color = Color(100, 100, 100);
						Color transform_colors[ECS_AXIS_COUNT] = {
								AxisXColor(),
								AxisYColor(),
								AxisZColor()
						};

						if (transform_tool.is_selected[ECS_AXIS_X]) {
							transform_colors[ECS_AXIS_Y] = gray_color;
							transform_colors[ECS_AXIS_Z] = gray_color;
						}
						else if (transform_tool.is_selected[ECS_AXIS_Y]) {
							transform_colors[ECS_AXIS_X] = gray_color;
							transform_colors[ECS_AXIS_Z] = gray_color;
						}
						else if (transform_tool.is_selected[ECS_AXIS_Z]) {
							transform_colors[ECS_AXIS_X] = gray_color;
							transform_colors[ECS_AXIS_Y] = gray_color;
						}

						if (transform_tool.display_axes) {
							// Just draw a cross at the midpoint with a very large length
							// Such that it will cover the entire screen - this info
							// doesn't need instance thickness x, y, z
							DebugOOBBCrossInfo info;
							memcpy(&info.color_x, &transform_colors[ECS_AXIS_X], sizeof(Color) * ECS_AXIS_COUNT);
							debug_drawer->AddOOBBCross(
								translation_midpoint,
								rotation_midpoint,
								10000.0f,
								pow(constant_viewport_size, 0.33f),
								false,
								&info,
								debug_options
							);
						}
						else if (transform_tool.tool != ECS_TRANSFORM_COUNT) {
							// Display the gizmo at the midpoint
							switch (transform_tool.tool) {
							case ECS_TRANSFORM_TRANSLATION:
							{
								DebugAxesInfo axes_info;
								axes_info.instance_thickness_x = GizmoRenderIndex(transform_tool.entity_ids[ECS_AXIS_X]);
								axes_info.instance_thickness_y = GizmoRenderIndex(transform_tool.entity_ids[ECS_AXIS_Y]);
								axes_info.instance_thickness_z = GizmoRenderIndex(transform_tool.entity_ids[ECS_AXIS_Z]);
								axes_info.color_x = transform_colors[ECS_AXIS_X];
								axes_info.color_y = transform_colors[ECS_AXIS_Y];
								axes_info.color_z = transform_colors[ECS_AXIS_Z];
								debug_drawer->AddAxes(
									translation_midpoint, 
									rotation_midpoint,
									constant_viewport_size,
									&axes_info,
									debug_options
								);
							}
								break;
							case ECS_TRANSFORM_ROTATION:
							{
								QuaternionScalar x_rotation = RotateQuaternion(QuaternionForAxisZScalar(90.0f), rotation_midpoint);
								debug_options.instance_thickness = GizmoRenderIndex(transform_tool.entity_ids[ECS_AXIS_X]);
								debug_drawer->AddCircle(
									translation_midpoint,
									x_rotation,
									constant_viewport_size,
									transform_colors[ECS_AXIS_X],
									debug_options
								);

								QuaternionScalar y_rotation = rotation_midpoint;
								debug_options.instance_thickness = GizmoRenderIndex(transform_tool.entity_ids[ECS_AXIS_Y]);
								debug_drawer->AddCircle(
									translation_midpoint,
									y_rotation,
									constant_viewport_size,
									transform_colors[ECS_AXIS_Y],
									debug_options
								);

								debug_options.instance_thickness = GizmoRenderIndex(transform_tool.entity_ids[ECS_AXIS_Z]);
								QuaternionScalar z_rotation = RotateQuaternion(QuaternionForAxisXScalar(90.0f), rotation_midpoint);
								debug_drawer->AddCircle(
									translation_midpoint,
									z_rotation,
									constant_viewport_size,
									transform_colors[ECS_AXIS_Z],
									debug_options
								);
							}
								break;
							case ECS_TRANSFORM_SCALE:
							{
								DebugOOBBCrossInfo info;
								info.color_x = transform_colors[ECS_AXIS_X];
								info.color_y = transform_colors[ECS_AXIS_Y];
								info.color_z = transform_colors[ECS_AXIS_Z];
								info.instance_thickness_x = GizmoRenderIndex(transform_tool.entity_ids[ECS_AXIS_X]);
								info.instance_thickness_y = GizmoRenderIndex(transform_tool.entity_ids[ECS_AXIS_Y]);
								info.instance_thickness_z = GizmoRenderIndex(transform_tool.entity_ids[ECS_AXIS_Z]);
								debug_drawer->AddOOBBCross(
									translation_midpoint,
									rotation_midpoint,
									constant_viewport_size * 0.45f,
									constant_viewport_size,
									true,
									&info,
									debug_options
								);
							}
								break;
							default:
								ECS_ASSERT(false, "ECS_TRANSFORM_TOOL invalid value");
							}
						}
					}
				}
			}
		}
	}
	else {
		ECS_REGISTER_FOR_EACH_COMPONENTS(QueryRead<RenderMesh>, QueryRead<Translation>, QueryRead<Rotation>, QueryRead<Scale>);
	}
}

ECS_THREAD_TASK_TEMPLATE_BOOL(DrawSelectables);

template<bool schedule_element>
ECS_THREAD_TASK(FlushRenderCommands) {
	if constexpr (!schedule_element) {
		GraphicsPipelineRenderState render_state = world->debug_drawer->GetPreviousRenderState();
		world->debug_drawer->DrawAll(1.0f);
		world->debug_drawer->RestorePreviousRenderState(&render_state);
		world->graphics->GetContext()->Flush();
	}
}

ECS_THREAD_TASK_TEMPLATE_BOOL(FlushRenderCommands);

struct DrawInstancedFramebufferMeshTaskData {
	ResizableStream<GenerateInstanceFramebufferElement>* elements;
	Matrix camera_matrix;
};

static void DrawInstancedFramebufferMeshTask(
	const ForEachEntityData* for_each_data,
	const RenderMesh* render_mesh,
	const Translation* translation,
	const Rotation* rotation,
	const Scale* scale
) {
	DrawInstancedFramebufferMeshTaskData* data = (DrawInstancedFramebufferMeshTaskData*)for_each_data->user_data;

	World* world = for_each_data->world;
	Graphics* graphics = world->graphics;

	if (render_mesh->Validate()) {
		float3 translation_value = { 0.0f, 0.0f, 0.0f };
		float4 rotation_value = QuaternionIdentityScalar();
		float3 scale_value = { 1.0f, 1.0f, 1.0f };
		Matrix matrix_translation;
		Matrix matrix_rotation;
		Matrix matrix_scale;

		if (translation != nullptr) {
			translation_value = translation->value;
			matrix_translation = MatrixTranslation(translation_value);
		}
		else {
			matrix_translation = MatrixIdentity();
		}

		if (rotation != nullptr) {
			rotation_value = rotation->value;
			matrix_rotation = QuaternionToMatrix(rotation_value);
		}
		else {
			matrix_rotation = MatrixIdentity();
		}

		if (scale != nullptr) {
			scale_value = scale->value;
			matrix_scale = MatrixScale(scale_value);
		}
		else {
			matrix_scale = MatrixIdentity();
		}

		Matrix gpu_mvp_matrix = MatrixMVPToGPU(matrix_translation, matrix_rotation, matrix_scale, data->camera_matrix);

		GenerateInstanceFramebufferElement element;
		element.base.is_submesh = false;
		element.base.mesh = &render_mesh->mesh->mesh;
		element.base.gpu_mvp_matrix = gpu_mvp_matrix;
		element.instance_index = for_each_data->entity.index;
		data->elements->Add(element);
	}
}

template<bool schedule_element>
ECS_THREAD_TASK(DrawInstancedFramebuffer) {
	auto kernel = ForEachEntityCommit<
		schedule_element,
		QueryRead<RenderMesh>,
		QueryOptional<QueryRead<Translation>>,
		QueryOptional<QueryRead<Rotation>>,
		QueryOptional<QueryRead<Scale>>
	>(thread_id, world);

	if constexpr (!schedule_element) {
		SystemManager* system_manager = world->system_manager;
		ECS_EDITOR_RUNTIME_TYPE runtime_type = GetEditorRuntimeType(system_manager);
		if (runtime_type != ECS_EDITOR_RUNTIME_TYPE_COUNT) {
			CameraCached camera;
			if (GetWorldCamera(world, camera)) {
				GraphicsBoundViews instanced_views;
				if (GetEditorRuntimeInstancedFramebuffer(system_manager, &instanced_views)) {
					// Render the elements
					ResizableStream<GenerateInstanceFramebufferElement> elements;
					elements.Initialize(world->memory, 0);
					Matrix camera_matrix = camera.GetViewProjectionMatrix();

					DrawInstancedFramebufferMeshTaskData draw_instanced_mesh_data = { &elements, camera_matrix };
					kernel.Function(DrawInstancedFramebufferMeshTask, &draw_instanced_mesh_data, sizeof(draw_instanced_mesh_data));

					// Get the pipeline state before rendering the instances
					GraphicsPipelineState pipeline_state = world->graphics->GetPipelineState();

					GenerateInstanceFramebuffer(world->graphics, elements.ToStream(), instanced_views.target, instanced_views.depth_stencil, true);
					world->graphics->BindRenderTargetView(instanced_views.target, instanced_views.depth_stencil);
					world->debug_drawer->DrawAll(0.0f, ECS_DEBUG_SHADER_OUTPUT_ID);
					
					world->graphics->RestorePipelineState(&pipeline_state);

					elements.FreeBuffer();
				}
			}
		}
	}
}

ECS_THREAD_TASK_TEMPLATE_BOOL(DrawInstancedFramebuffer);

ECS_THREAD_TASK(RecalculateCamera) {
	CameraComponent* camera_component = world->entity_manager->TryGetGlobalComponent<CameraComponent>();
	if (camera_component != nullptr) {
		camera_component->value.Recalculate();
	}
}