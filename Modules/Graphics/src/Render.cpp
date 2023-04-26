#include "pch.h"
#include "Render.h"
#include "Components.h"
#include "ECSEngineComponents.h"
#include "ECSEngineRuntime.h"
#include "ECSEngineShaderApplicationStage.h"

using namespace ECSEngine;

ECS_THREAD_TASK(PerformTask) {
	//static size_t value = 0;
	//for (size_t index = 0; index < 100000; index++) {
		//value *= index;
	//}
}

template<bool schedule_element>
ECS_THREAD_TASK(RenderTask) {
	Camera camera;
	Matrix camera_matrix = camera.GetViewProjectionMatrix();

	if (GetRuntimeCamera(world->system_manager, camera)) {
		world->graphics->ClearRenderTarget(world->graphics->GetBoundRenderTarget(), ColorFloat(1.0f, 0.5f, 1.0f, 1.0f));
		ForEachEntityCommit<QueryRead<RenderMesh>, QueryRead<Translation>>::Function(world, [camera_matrix](
			ForEachEntityData* for_each_data, 
			const RenderMesh& mesh, 
			Translation translation
		) {
			Graphics* graphics = for_each_data->world->graphics;
			if (mesh.material != nullptr) {
				Matrix object_matrix = MatrixTranslation(translation.value);
				ConstantBuffer temp_buffer = Shaders::CreatePBRVertexConstants(graphics, true);
				Matrix mvp_matrix = object_matrix * camera_matrix;
				Shaders::SetPBRVertexConstants(temp_buffer, graphics, object_matrix, mvp_matrix);

				graphics->BindMesh(mesh.mesh->mesh);
				graphics->BindMaterial(*mesh.material);

				graphics->BindVertexConstantBuffer(temp_buffer);
				graphics->DrawSubmeshCommand(mesh.mesh->submeshes[0]);
				temp_buffer.Release();
			}
		});
		//world->task_manager->AddDynamicTaskGroup(WITH_NAME(PerformTask), nullptr, std::thread::hardware_concurrency(), 0);
		world->graphics->GetContext()->Flush();
	}
}

ECS_THREAD_TASK_TEMPLATE_BOOL(RenderTask);

ECS_THREAD_TASK(RenderTaskInitialize) {
	// Create all the necessary shaders, samplers, and other Graphics pipeline objects
	
}