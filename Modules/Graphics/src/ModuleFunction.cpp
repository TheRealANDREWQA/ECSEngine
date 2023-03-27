#include "pch.h"
#include "ModuleFunction.h"
#include "Render.h"

using namespace ECSEngine;

void ModuleTaskFunction(ModuleTaskFunctionData* data) {
	TaskSchedulerElement element;
	element.initialize_task_function = RenderTaskInitialize;
	element.task_group = ECS_THREAD_TASK_FINALIZE_LATE;
	element.task_function = RenderTask<false>;
	element.task_name = STRING(RenderTask);

	data->tasks->Add(element);
	//ECS_REGISTER_FOR_EACH_TASK(element, RenderTask, data);
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