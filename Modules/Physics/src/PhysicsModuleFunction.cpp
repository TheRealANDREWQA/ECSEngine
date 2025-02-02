#include "pch.h"
#include "PhysicsModuleFunction.h"
#include "ContactManifolds.h"
#include "CollisionDetection/src/FixedGrid.h"
#include "CollisionDetection/src/CollisionDetectionComponents.h"
#include "CollisionDetection/src/GJK.h"
#include "ECSEngineWorld.h"
#include "ECSEngineComponents.h"
#include "ECSEngineForEach.h"
#include "Rigidbody.h"
#include "SolverCommon.h"
#include "ContactConstraint.h"

#include "Debugging.h"
#include "Scripting.h"

using namespace ECSEngine;

ECS_THREAD_TASK(GridHandler) {
	FixedGridHandlerData* data = (FixedGridHandlerData*)_data;
	AddContactPair(world, data->first_identifier, data->second_identifier);
}

static ECS_THREAD_TASK(ChangeHandler) {
	FixedGrid* fixed_grid = world->entity_manager->GetGlobalComponent<FixedGrid>();
	fixed_grid->ChangeHandler(GridHandler, nullptr, 0);
}

void ModuleTaskFunction(ModuleTaskFunctionData* data) {
	TaskSchedulerElement change_handler;
	change_handler.task_group = ECS_THREAD_TASK_FINALIZE_LATE;
	change_handler.initialize_data_task_name = STRING(CollisionBroadphase);
	ECS_REGISTER_TASK(change_handler, ChangeHandler, data);
	
	AddSolverCommonTasks(data);
	AddSolverTasks(data);
	AddScripts(data);
	RegisterDebugTasks(data);
}

#if 0

void ModuleBuildFunctions(ModuleBuildFunctionsData* data) {

}

#endif

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

void ModuleRegisterExtraInformationFunction(ECSEngine::ModuleRegisterExtraInformationFunctionData* data) {
	
}

#endif

#if 0

void ModuleRegisterDebugDrawFunction(ECSEngine::ModuleRegisterDebugDrawFunctionData* data) {

}

#endif

#if 0

void ModuleRegisterDebugDrawTaskElementsFunction(ECSEngine::ModuleRegisterDebugDrawTaskElementsData* data) {

}

#endif

#if 1

void ModuleRegisterComponentFunctionsFunction(ECSEngine::ModuleRegisterComponentFunctionsData* data) {
	AddRigidbodyBuildEntry(data);
}

#endif