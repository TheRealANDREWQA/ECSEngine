#include "pch.h"
#include "ModuleFunction.h"
#include "Render.h"

using namespace ECSEngine;

void ModuleTaskFunction(ModuleTaskFunctionData* data) {
	TaskSchedulerElement element;
	element.initialize_task_function = RenderTaskInitialize;
	element.task_group = ECS_THREAD_TASK_FINALIZE_LATE;
	ECS_REGISTER_FOR_EACH_TASK(element, RenderTask, data);

	element.initialize_task_function = RenderSelectablesInitialize;
	ECS_REGISTER_FOR_EACH_TASK(element, RenderSelectables, data);

	element.initialize_task_function = nullptr;
	ECS_REGISTER_FOR_EACH_TASK(element, RenderFlush, data);
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