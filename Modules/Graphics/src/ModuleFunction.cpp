#include "pch.h"
#include "ModuleFunction.h"
#include "Render.h"
#include "Components.h"

using namespace ECSEngine;

void ModuleTaskFunction(ModuleTaskFunctionData* data) {
	TaskSchedulerElement elements[4];
	
	for (size_t index = 0; index < std::size(elements); index++) {
		elements[index].task_group = ECS_THREAD_TASK_FINALIZE_LATE;
		elements[index].initialize_task_function = nullptr;
	}
	
	elements[0].initialize_task_function = RenderTaskInitialize;
	ECS_REGISTER_FOR_EACH_TASK(elements[0], RenderTask, data);

	elements[1].initialize_task_function = RenderSelectablesInitialize;
	ECS_REGISTER_FOR_EACH_TASK(elements[1], RenderSelectables, data);
	
	ECS_REGISTER_FOR_EACH_TASK(elements[2], RenderInstancedFramebuffer, data);

	ECS_REGISTER_FOR_EACH_TASK(elements[3], RenderFlush, data);
}

#if 0

void ModuleUIFunction(ECSEngine::ModuleUIFunctionData* data) {

}

#endif

#if 0

void ModuleSerializeComponentFunction(ECSEngine::ModuleSerializeComponentFunctionData* data) {

}

#endif

#if 0

void ModuleRegisterLinkComponentFunction(ECSEngine::ModuleRegisterLinkComponentFunctionData* data) {

}

#endif

#if 0

void ModuleSetCurrentWorld(ECSEngine::World* world) {

}

#endif

#if 1

void ModuleRegisterExtraInformationFunction(ECSEngine::ModuleRegisterExtraInformationFunctionData* data) {
	SetGraphicsModuleRenderMeshBounds(data, STRING(RenderMesh), STRING(mesh));
}

#endif