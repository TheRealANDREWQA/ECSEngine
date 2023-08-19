#include "pch.h"
#include "Render.h"
#include "Components.h"
#include "ECSEngineComponents.h"
#include "ECSEngineRuntime.h"
#include "ECSEngineShaderApplicationStage.h"
#include "ECSEngineDebugDrawer.h"
#include "ECSEngineCBufferTags.h"
#include "ECSEngineMath.h"

using namespace ECSEngine;

#define GIZMO_THICKNESS 4

ECS_INLINE unsigned int GizmoRenderIndex(unsigned int instance_index, bool extra_thick = true) {
	return GenerateRenderInstanceValue(instance_index, extra_thick ? ECS_GENERATE_INSTANCE_FRAMEBUFFER_MAX_PIXEL_THICKNESS : GIZMO_THICKNESS);
}

struct BasicDrawForEachData {
	const Camera* camera;
	Matrix camera_matrix;
};

template<bool has_translation, bool has_rotation, bool has_scale>
void BasicDrawForEach(ForEachEntityFunctorData* for_each_data) {
	BasicDrawForEachData* data = (BasicDrawForEachData*)for_each_data->data;

	World* world = for_each_data->world;
	Graphics* graphics = world->graphics;

	const RenderMesh* mesh = (const RenderMesh*)for_each_data->shared_components[0];
	if (mesh->Validate()) {

		float3 translation_value = { 0.0f, 0.0f, 0.0f };
		float4 rotation_value = { 0.0f, 0.0f, 0.0f, 1.0f };
		float3 scale_value = { 1.0f, 1.0f, 1.0f };
		Matrix matrix_translation;
		Matrix matrix_rotation;
		Matrix matrix_scale;

		enum ORDER {
			TRANSLATION,
			ROTATION,
			SCALE,
			COUNT
		};

		unsigned char component_indices[COUNT] = { 0 };
		if constexpr (has_translation) {
			for (size_t index = TRANSLATION + 1; index < COUNT; index++) {
				component_indices[index]++;
			}
		}
		if constexpr (has_rotation) {
			for (size_t index = ROTATION + 1; index < COUNT; index++) {
				component_indices[index]++;
			}
		}

		if constexpr (has_translation) {
			const Translation* translation = (const Translation*)for_each_data->unique_components[component_indices[TRANSLATION]];
			translation_value = translation->value;
			matrix_translation = MatrixTranslation(translation_value);
		}
		if constexpr (has_rotation) {
			const Rotation* rotation = (const Rotation*)for_each_data->unique_components[component_indices[ROTATION]];
			rotation_value = rotation->value;
			matrix_rotation = QuaternionToMatrixLow(Quaternion(rotation_value));
		}
		if constexpr (has_scale) {
			const Scale* scale = (const Scale*)for_each_data->unique_components[component_indices[SCALE]];
			scale_value = scale->value;
			matrix_scale = MatrixScale(scale_value);
		}

		Matrix object_matrix;

		if constexpr (has_translation) {
			if constexpr (has_rotation) {
				if constexpr (has_scale) {
					object_matrix = MatrixTRS(matrix_translation, matrix_rotation, matrix_scale);
				}
				else {
					object_matrix = MatrixTR(matrix_translation, matrix_rotation);
				}
			}
			else if constexpr (has_scale) {
				object_matrix = MatrixTS(matrix_translation, matrix_scale);
			}
			else {
				object_matrix = matrix_translation;
			}
		}
		else if constexpr (has_rotation) {
			if constexpr (has_scale) {
				object_matrix = MatrixRS(matrix_rotation, matrix_scale);
			}
			else {
				object_matrix = matrix_rotation;
			}
		}
		else if constexpr (has_scale) {
			object_matrix = matrix_scale;
		}
		else {
			object_matrix = MatrixIdentity();
		}

		Matrix mvp_matrix = object_matrix * data->camera_matrix;

		object_matrix = MatrixGPU(object_matrix);
		mvp_matrix = MatrixGPU(mvp_matrix);

		float3 camera_position = data->camera->translation;
		const void* injected_values[ECS_CB_INJECT_TAG_COUNT];
		injected_values[ECS_CB_INJECT_CAMERA_POSITION] = &camera_position;
		injected_values[ECS_CB_INJECT_MVP_MATRIX] = &mvp_matrix;
		injected_values[ECS_CB_INJECT_OBJECT_MATRIX] = &object_matrix;

		BindConstantBufferInjectedCB(graphics, mesh->material, injected_values);

		graphics->BindMesh(mesh->mesh->mesh);
		graphics->BindMaterial(*mesh->material);

		graphics->DrawCoalescedMeshCommand(*mesh->mesh);
	}
}

template<bool schedule_element>
ECS_THREAD_TASK(RenderTask) {
	if constexpr (!schedule_element) {
		world->graphics->ClearRenderTarget(world->graphics->GetBoundRenderTarget(), ColorFloat(0.5f, 0.6f, 1.0f, 1.0f));
		world->graphics->ClearDepth(world->graphics->GetBoundDepthStencil());

		Camera camera;

		if (GetRuntimeCamera(world->system_manager, &camera)) {
			Graphics* graphics = world->graphics;
			DebugDrawer* drawer = world->debug_drawer;

			Matrix camera_matrix = camera.GetViewProjectionMatrix();
			drawer->UpdateCameraMatrix(camera_matrix);

			Component unique_components[3];
			Component exclude_unique_components[3];
			Component shared_components[1];
			shared_components[0] = RenderMesh::ID();
			ComponentSignature shared_signature(shared_components, std::size(shared_components));

			BasicDrawForEachData for_each_data;
			for_each_data.camera_matrix = camera_matrix;
			for_each_data.camera = &camera;

			unique_components[0] = Translation::ID();
			exclude_unique_components[0] = Rotation::ID();
			exclude_unique_components[1] = Scale::ID();
			ForEachEntityCommitFunctor(world, BasicDrawForEach<true, false, false>, &for_each_data, { unique_components, 1 }, shared_signature, { exclude_unique_components, 2 });

			unique_components[0] = Rotation::ID();
			exclude_unique_components[0] = Translation::ID();
			ForEachEntityCommitFunctor(world, BasicDrawForEach<false, true, false>, &for_each_data, { unique_components, 1 }, shared_signature, { exclude_unique_components, 2 });

			unique_components[0] = Scale::ID();
			exclude_unique_components[1] = Rotation::ID();
			ForEachEntityCommitFunctor(world, BasicDrawForEach<false, false, true>, &for_each_data, { unique_components, 1 }, shared_signature, { exclude_unique_components, 2 });

			unique_components[0] = Translation::ID();
			unique_components[1] = Rotation::ID();
			exclude_unique_components[0] = Scale::ID();
			ForEachEntityCommitFunctor(world, BasicDrawForEach<true, true, false>, &for_each_data, { unique_components, 2 }, shared_signature, { exclude_unique_components, 1 });

			unique_components[1] = Scale::ID();
			exclude_unique_components[0] = Rotation::ID();
			ForEachEntityCommitFunctor(world, BasicDrawForEach<true, false, true>, &for_each_data, { unique_components, 2 }, shared_signature, { exclude_unique_components, 1 });

			unique_components[0] = Rotation::ID();
			exclude_unique_components[0] = Translation::ID();
			ForEachEntityCommitFunctor(world, BasicDrawForEach<false, true, true>, &for_each_data, { unique_components, 2 }, shared_signature, { exclude_unique_components, 1 });

			unique_components[0] = Translation::ID();
			unique_components[1] = Rotation::ID();
			unique_components[2] = Scale::ID();
			ForEachEntityCommitFunctor(world, BasicDrawForEach<true, true, true>, &for_each_data, { unique_components, 3 }, shared_signature);
		}
	}
	else {
		ECS_REGISTER_FOR_EACH_COMPONENTS(QueryRead<RenderMesh>, QueryRead<Translation>, QueryRead<Rotation>, QueryRead<Scale>);
	}
}

ECS_THREAD_TASK_TEMPLATE_BOOL(RenderTask);

ECS_THREAD_TASK(RenderTaskInitialize) {
	// Nothing to be performed at the moment
}

template<bool schedule_element>
ECS_THREAD_TASK(RenderSelectables) {
	if constexpr (!schedule_element) {
		SystemManager* system_manager = world->system_manager;
		EntityManager* entity_manager = world->entity_manager;
		if (IsEditorRuntime(system_manager)) {
			Camera camera;
			if (GetRuntimeCamera(system_manager, &camera)) {
				Color select_color = GetEditorRuntimeSelectColor(system_manager);
				Stream<Entity> selected_entities = GetEditorRuntimeSelectedEntities(system_manager);
				if (selected_entities.size > 0) {
					Matrix camera_matrix = camera.GetViewProjectionMatrix();
					// Highlight the entities - if they have meshes
					size_t valid_entities = 0;
					for (size_t index = 0; index < selected_entities.size; index++) {
						const RenderMesh* render_mesh = entity_manager->TryGetComponent<RenderMesh>(selected_entities[index]);
						valid_entities += render_mesh != nullptr && render_mesh->Validate();
					}

					float3 translation_midpoint = { 0.0f, 0.0f, 0.0f };
					Quaternion rotation_midpoint = QuaternionIdentity();
					if (valid_entities > 0) {
						HighlightObjectElement* highlight_elements = nullptr;
						size_t allocate_size = sizeof(highlight_elements[0]) * valid_entities;
						bool stack_allocated = false;
						if (allocate_size < ECS_KB * 64) {
							// Use stack allocation
							highlight_elements = (HighlightObjectElement*)ECS_STACK_ALLOC(allocate_size);
							stack_allocated = true;
						}
						else {
							highlight_elements = (HighlightObjectElement*)world->task_manager->AllocateTempBuffer(thread_id, allocate_size);
						}

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
										Matrix rotation_matrix = QuaternionToMatrixLow(Quaternion(rotation->value));
										if (scale != nullptr) {
											entity_matrix = MatrixTRS(translation_matrix, rotation_matrix, MatrixScale(scale->value));
										}
										else {
											entity_matrix = MatrixTR(translation_matrix, rotation_matrix);
										}
										rotation_midpoint *= Quaternion(rotation->value);
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
									Matrix rotation_matrix = QuaternionToMatrixLow(Quaternion(rotation->value));
									if (scale != nullptr) {
										entity_matrix = MatrixRS(rotation_matrix, MatrixScale(scale->value));
									}
									else {
										entity_matrix = rotation_matrix;
									}
									rotation_midpoint *= Quaternion(rotation->value);
								}
								else if (scale != nullptr) {
									entity_matrix = MatrixScale(scale->value);
								}

								highlight_elements[index].base.gpu_mvp_matrix = MatrixMVPToGPU(entity_matrix, camera_matrix);
							}
						}

						HighlightObject(world->graphics, select_color, { highlight_elements, valid_entities });

						if (!stack_allocated) {
							world->task_manager->ClearThreadAllocator(thread_id);
						}

						translation_midpoint /= float3::Splat((float)valid_entities);
						//rotation_midpoint /= float3::Splat((float)valid_entities);

						// At the moment disable the rotation of the elements
						// Until we implement the World/Local transformation tool

						DebugDrawer* debug_drawer = world->debug_drawer;

						float3 camera_distance = translation_midpoint - camera.translation;
						float distance = Length(Vector8(camera_distance)).First();			

						float constant_viewport_size = GetConstantObjectSizeInPerspective(camera.fov, distance, 0.25f);

						// Now display the aggregate tool
						ECSTransformToolEx transform_tool = GetEditorRuntimeTransformToolEx(system_manager);
						DebugDrawCallOptions debug_options;
						debug_options.ignore_depth = true;
						debug_options.wireframe = false;

						if (transform_tool.tool != ECS_TRANSFORM_COUNT) {
							Color gray_color = Color(100, 100, 100);
							Color transform_colors[ECS_TRANSFORM_AXIS_COUNT] = {
								AxisXColor(),
								AxisYColor(),
								AxisZColor()
							};

							if (transform_tool.is_selected[ECS_TRANSFORM_AXIS_X]) {
								transform_colors[ECS_TRANSFORM_AXIS_Y] = gray_color;
								transform_colors[ECS_TRANSFORM_AXIS_Z] = gray_color;
							}
							else if (transform_tool.is_selected[ECS_TRANSFORM_AXIS_Y]) {
								transform_colors[ECS_TRANSFORM_AXIS_X] = gray_color;
								transform_colors[ECS_TRANSFORM_AXIS_Z] = gray_color;
							}
							else if (transform_tool.is_selected[ECS_TRANSFORM_AXIS_Z]) {
								transform_colors[ECS_TRANSFORM_AXIS_X] = gray_color;
								transform_colors[ECS_TRANSFORM_AXIS_Y] = gray_color;
							}

							// Display the gizmo at the midpoint
							switch (transform_tool.tool) {
							case ECS_TRANSFORM_TRANSLATION:
							{
								DebugAxesInfo axes_info;
								axes_info.instance_thickness_x = GizmoRenderIndex(transform_tool.entity_ids[ECS_TRANSFORM_AXIS_X]);
								axes_info.instance_thickness_y = GizmoRenderIndex(transform_tool.entity_ids[ECS_TRANSFORM_AXIS_Y]);
								axes_info.instance_thickness_z = GizmoRenderIndex(transform_tool.entity_ids[ECS_TRANSFORM_AXIS_Z]);
								axes_info.color_x = transform_colors[ECS_TRANSFORM_AXIS_X];
								axes_info.color_y = transform_colors[ECS_TRANSFORM_AXIS_Y];
								axes_info.color_z = transform_colors[ECS_TRANSFORM_AXIS_Z];
								debug_drawer->AddAxes(
									translation_midpoint, 
									rotation_midpoint.StorageLow(),
									constant_viewport_size,
									&axes_info,
									debug_options
								);
							}
								break;
							case ECS_TRANSFORM_ROTATION:
							{
								Quaternion x_rotation = AddWorldRotation(rotation_midpoint, QuaternionForAxisZ(90.0f));
								debug_options.instance_thickness = GizmoRenderIndex(transform_tool.entity_ids[ECS_TRANSFORM_AXIS_X], true);
								debug_drawer->AddCircle(
									translation_midpoint,
									x_rotation.StorageLow(),
									constant_viewport_size,
									transform_colors[ECS_TRANSFORM_AXIS_X],
									debug_options
								);

								Quaternion y_rotation = rotation_midpoint;
								debug_options.instance_thickness = GizmoRenderIndex(transform_tool.entity_ids[ECS_TRANSFORM_AXIS_Y], true);
								debug_drawer->AddCircle(
									translation_midpoint,
									y_rotation.StorageLow(),
									constant_viewport_size,
									transform_colors[ECS_TRANSFORM_AXIS_Y],
									debug_options
								);

								debug_options.instance_thickness = GizmoRenderIndex(transform_tool.entity_ids[ECS_TRANSFORM_AXIS_Z], true);
								Quaternion z_rotation = AddWorldRotation(rotation_midpoint, QuaternionForAxisX(90.0f));
								debug_drawer->AddCircle(
									translation_midpoint,
									z_rotation.StorageLow(),
									constant_viewport_size,
									transform_colors[ECS_TRANSFORM_AXIS_Z],
									debug_options
								);
							}
								break;
							case ECS_TRANSFORM_SCALE:
							{
								float3 oobb_scale = float3(1.0f, 0.02f, 0.02f) * float3::Splat(constant_viewport_size);

								Quaternion x_rotation = rotation_midpoint;
								debug_options.instance_thickness = GizmoRenderIndex(transform_tool.entity_ids[ECS_TRANSFORM_AXIS_X], true);
								debug_drawer->AddOOBB(
									translation_midpoint,
									x_rotation.StorageLow(),
									oobb_scale,
									transform_colors[ECS_TRANSFORM_AXIS_X],
									debug_options
								);

								Quaternion y_rotation = AddWorldRotation(rotation_midpoint, QuaternionForAxisZ(90.0f));
								debug_options.instance_thickness = GizmoRenderIndex(transform_tool.entity_ids[ECS_TRANSFORM_AXIS_Y], true);
								debug_drawer->AddOOBB(
									translation_midpoint,
									y_rotation.StorageLow(),
									oobb_scale,
									transform_colors[ECS_TRANSFORM_AXIS_Y],
									debug_options
								);

								Quaternion z_rotation = AddWorldRotation(rotation_midpoint, QuaternionForAxisY(90.0f));
								debug_options.instance_thickness = GizmoRenderIndex(transform_tool.entity_ids[ECS_TRANSFORM_AXIS_Z], true);
								debug_drawer->AddOOBB(
									translation_midpoint,
									z_rotation.StorageLow(),
									oobb_scale,
									transform_colors[ECS_TRANSFORM_AXIS_Z],
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

ECS_THREAD_TASK_TEMPLATE_BOOL(RenderSelectables);

ECS_THREAD_TASK(RenderSelectablesInitialize) {
	// Nothing to be performed at the moment
}

template<bool schedule_element>
ECS_THREAD_TASK(RenderFlush) {
	if constexpr (!schedule_element) {
		GraphicsPipelineRenderState render_state = world->debug_drawer->GetPreviousRenderState();
		world->debug_drawer->DrawAll(1.0f);
		world->debug_drawer->RestorePreviousRenderState(&render_state);
		world->graphics->GetContext()->Flush();
	}
}

ECS_THREAD_TASK_TEMPLATE_BOOL(RenderFlush);

struct InstancedFramebufferForEachData {
	const Camera* camera;
	Matrix camera_matrix;
	ResizableStream<GenerateInstanceFramebufferElement>* elements;
};

template<bool has_translation, bool has_rotation, bool has_scale>
void InstancedFramebufferForEach(ForEachEntityFunctorData* for_each_data) {
	InstancedFramebufferForEachData* data = (InstancedFramebufferForEachData*)for_each_data->data;

	World* world = for_each_data->world;
	Graphics* graphics = world->graphics;

	const RenderMesh* mesh = (const RenderMesh*)for_each_data->shared_components[0];
	if (mesh->Validate()) {
		float3 translation_value = { 0.0f, 0.0f, 0.0f };
		//float3 rotation_value = { 0.0f, 0.0f, 0.0f };
		float4 rotation_value = { 0.0f, 0.0f, 0.0f, 1.0f };
		float3 scale_value = { 1.0f, 1.0f, 1.0f };
		Matrix matrix_translation;
		Matrix matrix_rotation;
		Matrix matrix_scale;

		enum ORDER {
			TRANSLATION,
			ROTATION,
			SCALE,
			COUNT
		};

		unsigned char component_indices[COUNT] = { 0 };
		if constexpr (has_translation) {
			for (size_t index = TRANSLATION + 1; index < COUNT; index++) {
				component_indices[index]++;
			}
		}
		if constexpr (has_rotation) {
			for (size_t index = ROTATION + 1; index < COUNT; index++) {
				component_indices[index]++;
			}
		}

		if constexpr (has_translation) {
			const Translation* translation = (const Translation*)for_each_data->unique_components[component_indices[TRANSLATION]];
			translation_value = translation->value;
			matrix_translation = MatrixTranslation(translation_value);
		}
		if constexpr (has_rotation) {
			const Rotation* rotation = (const Rotation*)for_each_data->unique_components[component_indices[ROTATION]];
			rotation_value = rotation->value;
			matrix_rotation = QuaternionToMatrixLow(Quaternion(rotation_value));
		}
		if constexpr (has_scale) {
			const Scale* scale = (const Scale*)for_each_data->unique_components[component_indices[SCALE]];
			scale_value = scale->value;
			matrix_scale = MatrixScale(scale_value);
		}

		Matrix object_matrix;

		if constexpr (has_translation) {
			if constexpr (has_rotation) {
				if constexpr (has_scale) {
					object_matrix = MatrixTRS(matrix_translation, matrix_rotation, matrix_scale);
				}
				else {
					object_matrix = MatrixTR(matrix_translation, matrix_rotation);
				}
			}
			else if constexpr (has_scale) {
				object_matrix = MatrixTS(matrix_translation, matrix_scale);
			}
			else {
				object_matrix = matrix_translation;
			}
		}
		else if constexpr (has_rotation) {
			if constexpr (has_scale) {
				object_matrix = MatrixRS(matrix_rotation, matrix_scale);
			}
			else {
				object_matrix = matrix_rotation;
			}
		}
		else if constexpr (has_scale) {
			object_matrix = matrix_scale;
		}
		else {
			object_matrix = MatrixIdentity();
		}

		Matrix mvp_matrix = object_matrix * data->camera_matrix;
		mvp_matrix = MatrixGPU(mvp_matrix);

		GenerateInstanceFramebufferElement element;
		element.base.is_submesh = false;
		element.base.mesh = &mesh->mesh->mesh;
		element.base.gpu_mvp_matrix = mvp_matrix;
		element.instance_index = for_each_data->entity.index;
		data->elements->Add(element);
	}
}

template<bool schedule_element>
ECS_THREAD_TASK(RenderInstancedFramebuffer) {
	if constexpr (!schedule_element) {
		SystemManager* system_manager = world->system_manager;
		if (IsEditorRuntime(system_manager)) {
			Camera camera;
			if (GetRuntimeCamera(system_manager, &camera)) {
				GraphicsBoundViews instanced_views;
				if (GetEditorRuntimeInstancedFramebuffer(system_manager, &instanced_views)) {
					// Render the elements
					ResizableStream<GenerateInstanceFramebufferElement> elements;
					elements.Initialize(GetAllocatorPolymorphic(world->memory), 0);

					Component unique_components[3];
					Component exclude_unique_components[3];
					Component shared_components[1];
					shared_components[0] = RenderMesh::ID();
					ComponentSignature shared_signature(shared_components, std::size(shared_components));

					InstancedFramebufferForEachData for_each_data;
					for_each_data.camera_matrix = camera.GetViewProjectionMatrix();
					for_each_data.camera = &camera;
					for_each_data.elements = &elements;

					unique_components[0] = Translation::ID();
					exclude_unique_components[0] = Rotation::ID();
					exclude_unique_components[1] = Scale::ID();
					ForEachEntityCommitFunctor(world, InstancedFramebufferForEach<true, false, false>, &for_each_data, { unique_components, 1 }, shared_signature, { exclude_unique_components, 2 });

					unique_components[0] = Rotation::ID();
					exclude_unique_components[0] = Translation::ID();
					ForEachEntityCommitFunctor(world, InstancedFramebufferForEach<false, true, false>, &for_each_data, { unique_components, 1 }, shared_signature, { exclude_unique_components, 2 });

					unique_components[0] = Scale::ID();
					exclude_unique_components[1] = Rotation::ID();
					ForEachEntityCommitFunctor(world, InstancedFramebufferForEach<false, false, true>, &for_each_data, { unique_components, 1 }, shared_signature, { exclude_unique_components, 2 });

					unique_components[0] = Translation::ID();
					unique_components[1] = Rotation::ID();
					exclude_unique_components[0] = Scale::ID();
					ForEachEntityCommitFunctor(world, InstancedFramebufferForEach<true, true, false>, &for_each_data, { unique_components, 2 }, shared_signature, { exclude_unique_components, 1 });

					unique_components[1] = Scale::ID();
					exclude_unique_components[0] = Rotation::ID();
					ForEachEntityCommitFunctor(world, InstancedFramebufferForEach<true, false, true>, &for_each_data, { unique_components, 2 }, shared_signature, { exclude_unique_components, 1 });

					unique_components[0] = Rotation::ID();
					exclude_unique_components[0] = Translation::ID();
					ForEachEntityCommitFunctor(world, InstancedFramebufferForEach<false, true, true>, &for_each_data, { unique_components, 2 }, shared_signature, { exclude_unique_components, 1 });

					unique_components[0] = Translation::ID();
					unique_components[1] = Rotation::ID();
					unique_components[2] = Scale::ID();
					ForEachEntityCommitFunctor(world, InstancedFramebufferForEach<true, true, true>, &for_each_data, { unique_components, 3 }, shared_signature);

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
	else {
		ECS_REGISTER_FOR_EACH_COMPONENTS(QueryRead<RenderMesh>, QueryRead<Translation>, QueryRead<Rotation>, QueryRead<Scale>);
	}
}

ECS_THREAD_TASK_TEMPLATE_BOOL(RenderInstancedFramebuffer);