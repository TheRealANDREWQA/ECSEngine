#pragma once
#include "Export.h"

namespace ECSEngine {
	struct ModuleTaskFunctionData;
	struct World;
}

void RegisterDebugTasks(ECSEngine::ModuleTaskFunctionData* task_data);

void SetPhysicsIslandDraw(ECSEngine::World* world, bool set);