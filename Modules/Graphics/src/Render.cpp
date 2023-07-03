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

ECS_THREAD_TASK(PerformTask) {
	//static size_t value = 0;
	//for (size_t index = 0; index < 100000; index++) {
		//value *= index;
	//}

	//Camera camera;
	//camera.SetPerspectiveProjection();
}

struct BasicDrawForEachData {
	const Camera* camera;
	Matrix camera_matrix;
};

template<bool has_translation, bool has_rotation, bool has_scale>
void BasicDrawForEach(ForEachEntityFunctorData* for_each_data) {
	BasicDrawForEachData* data = (BasicDrawForEachData*)for_each_data->data;

	World* world = for_each_data->world;
	DebugDrawer* debug_drawer = world->debug_drawer;
	Graphics* graphics = world->graphics;
	DebugDrawer* drawer = world->debug_drawer;

	const RenderMesh* mesh = (const RenderMesh*)for_each_data->shared_components[0];
	if (IsAssetPointerValid(mesh->material) && IsAssetPointerValid(mesh->mesh)) {

		float3 translation_value = { 0.0f, 0.0f, 0.0f };
		float3 rotation_value = { 0.0f, 0.0f, 0.0f };
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
			matrix_rotation = QuaternionRotationMatrix(rotation_value);
		}
		if constexpr (has_scale) {
			const Scale* scale = (const Scale*)for_each_data->unique_components[component_indices[SCALE]];
			scale_value = scale->value;
			matrix_scale = MatrixScale(scale_value);
		}

		Matrix object_matrix;

		if constexpr (has_translation) {
			if constexpr (has_rotation) {
				object_matrix = MatrixTR(matrix_translation, matrix_rotation);
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

		graphics->BindRasterizerState(debug_drawer->rasterizer_states[ECS_DEBUG_RASTERIZER_SOLID]);

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

		HighlightObjectElement element;
		element.is_coalesced = false;
		element.mesh = &mesh->mesh->mesh;
		element.gpu_mvp_matrix = mvp_matrix;
		HighlightObject(graphics, ECS_COLOR_RED, { &element, 1 });

		float3 camera_distance = translation_value - data->camera->translation;
		float distance = Length(camera_distance).First();

		/*drawer->DrawAxes(
			translation_value,
			rotation_value,
			GetConstantObjectSizeInPerspective(data->camera->fov, distance, 0.25f),
			AxisXColor(),
			AxisYColor(),
			AxisZColor(),
			{ false, true }
		);
		graphics->EnableDepth();*/
	}
}

template<bool schedule_element>
ECS_THREAD_TASK(RenderTask) {
	if constexpr (!schedule_element) {
		world->graphics->ClearRenderTarget(world->graphics->GetBoundRenderTarget(), ColorFloat(0.5f, 0.6f, 1.0f, 1.0f));

		Camera camera;

		if (GetRuntimeCamera(world->system_manager, camera)) {
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

		world->graphics->GetContext()->Flush();
	}
}

ECS_THREAD_TASK_TEMPLATE_BOOL(RenderTask);

ECS_THREAD_TASK(RenderTaskInitialize) {
	// Create all the necessary shaders, samplers, and other Graphics pipeline objects
	
}