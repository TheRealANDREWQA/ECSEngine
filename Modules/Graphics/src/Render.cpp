#include "pch.h"
#include "Render.h"
#include "Components.h"
#include "ECSEngineComponents.h"
#include "ECSEngineRuntime.h"
#include "ECSEngineShaderApplicationStage.h"
#include "ECSEngineDebugDrawer.h"

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
	world->graphics->ClearRenderTarget(world->graphics->GetBoundRenderTarget(), ColorFloat(0.5f, 0.6f, 1.0f, 1.0f));

	Camera camera;

	if (GetRuntimeCamera(world->system_manager, camera)) {
		camera.translation.z -= 5.0f;
		Matrix camera_matrix = camera.GetViewProjectionMatrix();

		ForEachEntityCommit<QueryRead<RenderMesh>, QueryRead<Translation>>::Function(world, [camera_matrix](
			ForEachEntityData* for_each_data, 
			const RenderMesh& mesh, 
			Translation translation
		) {
			Timer timer;

			World* world = for_each_data->world;
			DebugDrawer* debug_drawer = world->debug_drawer;

			debug_drawer->UpdateCameraMatrix(camera_matrix);
			
			debug_drawer->AddAxes({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f, AxisXColor(), AxisYColor(), AxisZColor());
			debug_drawer->AddAxes({ 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f, AxisXColor(), AxisYColor(), AxisZColor());
			debug_drawer->AddAxes({ 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f, AxisXColor(), AxisYColor(), AxisZColor());
			debug_drawer->AddAxes({ 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f, AxisXColor(), AxisYColor(), AxisZColor());
			debug_drawer->AddAxes({ 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f, AxisXColor(), AxisYColor(), AxisZColor());
			debug_drawer->DrawAxesDeck(1.0f);
			
			/*debug_drawer->DrawSphere({ 0.0f, 0.0f, 0.0f }, 1.0f, ColorFloat(0.5f, 0.2f, 1.0f));
			debug_drawer->DrawSphere({ 0.0f, 0.0f, 0.0f }, 2.0f, ColorFloat(0.5f, 0.5f, 1.0f));
			debug_drawer->DrawSphere({ 0.0f, 0.0f, 0.0f }, 3.0f, ColorFloat(0.5f, 0.8f, 1.0f));
			debug_drawer->DrawSphere({ 0.0f, 0.0f, 0.0f }, 4.0f, ColorFloat(0.8f, 0.2f, 1.0f));
			debug_drawer->DrawSphere({ 0.0f, 0.0f, 0.0f }, 5.0f, ColorFloat(0.5f, 0.2f, 0.5f));
			debug_drawer->DrawSphere({ 0.0f, 0.0f, 0.0f }, 6.0f, ColorFloat(0.5f, 0.2f, 0.0f));*/

			debug_drawer->AddSphere({ 0.0f, 0.0f, 0.0f }, 1.0f, ColorFloat(0.5f, 0.2f, 1.0f));
			debug_drawer->AddSphere({ 0.0f, 0.0f, 0.0f }, 2.0f, ColorFloat(0.5f, 0.5f, 1.0f));
			debug_drawer->AddSphere({ 0.0f, 0.0f, 0.0f }, 3.0f, ColorFloat(0.5f, 0.8f, 1.0f));
			debug_drawer->AddSphere({ 0.0f, 0.0f, 0.0f }, 4.0f, ColorFloat(0.8f, 0.2f, 1.0f));
			debug_drawer->AddSphere({ 0.0f, 0.0f, 0.0f }, 5.0f, ColorFloat(0.5f, 0.2f, 0.5f));
			debug_drawer->AddSphere({ 0.0f, 0.0f, 0.0f }, 6.0f, ColorFloat(0.5f, 0.2f, 0.0f));
			debug_drawer->DrawSphereDeck(1.0f);

			Graphics* graphics = for_each_data->world->graphics;
			if (IsAssetPointerValid(mesh.material) && IsAssetPointerValid(mesh.mesh)) {
				Matrix object_matrix = MatrixTranslation(translation.value);
				ConstantBuffer temp_buffer = Shaders::CreatePBRVertexConstants(graphics, true);
				Matrix mvp_matrix = object_matrix * camera_matrix;
				Shaders::SetPBRVertexConstants(temp_buffer, graphics, object_matrix, mvp_matrix);

				graphics->BindMesh(mesh.mesh->mesh);
				graphics->BindMaterial(*mesh.material);

				graphics->BindVertexConstantBuffer(temp_buffer);
				graphics->DrawCoallescedMeshCommand(*mesh.mesh);
				
				temp_buffer.Release();
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

ECS_THREAD_TASK_TEMPLATE_BOOL(RenderTask);

ECS_THREAD_TASK(RenderTaskInitialize) {
	// Create all the necessary shaders, samplers, and other Graphics pipeline objects
	
}