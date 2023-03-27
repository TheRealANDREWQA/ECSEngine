#include "pch.h"
#include "Render.h"
#include "Components.h"

using namespace ECSEngine;

ECS_THREAD_TASK(PerformTask) {
	//static size_t value = 0;
	//for (size_t index = 0; index < 100000; index++) {
		//value *= index;
	//}
}

template<bool schedule_element>
ECS_THREAD_TASK(RenderTask) {
	//ForEachEntityCommit<RenderMesh>::Function(world, [world](const RenderMesh& mesh) {
		
	//});
	//world->task_manager->AddDynamicTaskGroup(WITH_NAME(PerformTask), nullptr, std::thread::hardware_concurrency(), 0);
	world->graphics->ClearRenderTarget(world->graphics->GetBoundRenderTarget(), ColorFloat(0.7f, 0.2f, 0.3f, 1.0f));
	world->graphics->GetContext()->Flush();
}

ECS_THREAD_TASK_TEMPLATE_BOOL(RenderTask);

ECS_THREAD_TASK(RenderTaskInitialize) {
	// Create all the necessary shaders, samplers, and other Graphics pipeline objects
	
}