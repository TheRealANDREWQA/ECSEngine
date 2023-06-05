#include "pch.h"
#include "Render.h"
#include "Components.h"
#include "ECSEngineComponents.h"
#include "ECSEngineRuntime.h"
#include "ECSEngineShaderApplicationStage.h"
#include "ECSEngineDebugDrawer.h"
#include "ECSEngineCBufferTags.h"

using namespace ECSEngine;

ECS_THREAD_TASK(PerformTask) {
	//static size_t value = 0;
	//for (size_t index = 0; index < 100000; index++) {
		//value *= index;
	//}

	//Camera camera;
	//camera.SetPerspectiveProjection();
}

template<bool schedule_element>
ECS_THREAD_TASK(RenderTask) {
	if constexpr (!schedule_element) {
		world->graphics->ClearRenderTarget(world->graphics->GetBoundRenderTarget(), ColorFloat(0.5f, 0.6f, 1.0f, 1.0f));

		Camera camera;

		if (GetRuntimeCamera(world->system_manager, camera)) {
			Graphics* graphics = world->graphics;
			DebugDrawer* drawer = world->debug_drawer;

			camera.translation.z -= 5.0f;
			Matrix camera_matrix = camera.GetViewProjectionMatrix();
			//camera.SetOrthographicProjection(graphics->GetWindowSize().x, graphics->GetWindowSize().y, 0.0005f, 1000.0f);
			Matrix perspective_camera = camera.GetViewProjectionMatrix();

			drawer->UpdateCameraMatrix(perspective_camera);
			drawer->DrawAxes({ 0.0f, 5.0f, 0.0f }, { 45.0f, 0.0f, 45.0f }, 20.0f);

			ForEachEntityCommit<QueryRead<RenderMesh>, QueryRead<Translation>>::Function(world, [camera, camera_matrix](
				ForEachEntityData* for_each_data,
				const RenderMesh& mesh,
				Translation translation
				) {
					Timer timer;

					World* world = for_each_data->world;
					DebugDrawer* debug_drawer = world->debug_drawer;

					debug_drawer->UpdateCameraMatrix(camera_matrix);

					/*debug_drawer->AddAxes({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f, AxisXColor(), AxisYColor(), AxisZColor());
					debug_drawer->AddAxes({ 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f, AxisXColor(), AxisYColor(), AxisZColor());
					debug_drawer->AddAxes({ 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f, AxisXColor(), AxisYColor(), AxisZColor());
					debug_drawer->AddAxes({ 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f, AxisXColor(), AxisYColor(), AxisZColor());
					debug_drawer->AddAxes({ 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f, AxisXColor(), AxisYColor(), AxisZColor());
					debug_drawer->DrawAxesDeck(1.0f);*/

					/*debug_drawer->DrawSphere({ 0.0f, 0.0f, 0.0f }, 1.0f, ColorFloat(0.5f, 0.2f, 1.0f));
					debug_drawer->DrawSphere({ 0.0f, 0.0f, 0.0f }, 2.0f, ColorFloat(0.5f, 0.5f, 1.0f));
					debug_drawer->DrawSphere({ 0.0f, 0.0f, 0.0f }, 3.0f, ColorFloat(0.5f, 0.8f, 1.0f));
					debug_drawer->DrawSphere({ 0.0f, 0.0f, 0.0f }, 4.0f, ColorFloat(0.8f, 0.2f, 1.0f));
					debug_drawer->DrawSphere({ 0.0f, 0.0f, 0.0f }, 5.0f, ColorFloat(0.5f, 0.2f, 0.5f));
					debug_drawer->DrawSphere({ 0.0f, 0.0f, 0.0f }, 6.0f, ColorFloat(0.5f, 0.2f, 0.0f));*/

					/*debug_drawer->AddSphere({ 0.0f, 0.0f, 0.0f }, 1.0f, ColorFloat(0.5f, 0.2f, 1.0f));
					debug_drawer->AddSphere({ 0.0f, 0.0f, 0.0f }, 2.0f, ColorFloat(0.5f, 0.5f, 1.0f));
					debug_drawer->AddSphere({ 0.0f, 0.0f, 0.0f }, 3.0f, ColorFloat(0.5f, 0.8f, 1.0f));
					debug_drawer->AddSphere({ 0.0f, 0.0f, 0.0f }, 4.0f, ColorFloat(0.8f, 0.2f, 1.0f));
					debug_drawer->AddSphere({ 0.0f, 0.0f, 0.0f }, 5.0f, ColorFloat(0.5f, 0.2f, 0.5f));
					debug_drawer->AddSphere({ 0.0f, 0.0f, 0.0f }, 6.0f, ColorFloat(0.5f, 0.2f, 0.0f));
					debug_drawer->DrawSphereDeck(1.0f);*/

					Graphics* graphics = for_each_data->world->graphics;
					if (IsAssetPointerValid(mesh.material) && IsAssetPointerValid(mesh.mesh)) {
						graphics->BindRasterizerState(debug_drawer->rasterizer_states[ECS_DEBUG_RASTERIZER_SOLID]);
						Matrix object_matrix = MatrixTranslation(translation.value);
						Matrix mvp_matrix = object_matrix * camera_matrix;

						object_matrix = MatrixTranspose(object_matrix);
						mvp_matrix = MatrixTranspose(mvp_matrix);

						float3 camera_position = camera.translation;
						const void* injected_values[ECS_CB_INJECT_TAG_COUNT];
						injected_values[ECS_CB_INJECT_CAMERA_POSITION] = &camera_position;
						injected_values[ECS_CB_INJECT_MVP_MATRIX] = &mvp_matrix;
						injected_values[ECS_CB_INJECT_OBJECT_MATRIX] = &object_matrix;

						BindConstantBufferInjectedCB(graphics, mesh.material, injected_values);

						graphics->BindMesh(mesh.mesh->mesh);
						graphics->BindMaterial(*mesh.material);

						graphics->EnableDepth();
						graphics->DrawCoallescedMeshCommand(*mesh.mesh);
					}

					//size_t duration = timer.GetDuration(ECS_TIMER_DURATION_US);
					//ECS_FORMAT_TEMP_STRING(message, "Duration: {#}", duration);
					//GetConsole()->Info(message);
					//OutputDebugStringA()
				});
			//world->task_manager->AddDynamicTaskGroup(WITH_NAME(PerformTask), nullptr, std::thread::hardware_concurrency(), 0);
			//world->graphics->GetContext()->Flush();
		}

		world->graphics->GetContext()->Flush();
	}
}

ECS_THREAD_TASK_TEMPLATE_BOOL(RenderTask);

ECS_THREAD_TASK(RenderTaskInitialize) {
	// Create all the necessary shaders, samplers, and other Graphics pipeline objects
	
}