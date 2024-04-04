#pragma once
#include "ECSEngineComponents.h"
#include "ECSEngineForEach.h"

using namespace ECSEngine;

template<bool schedule_element>
ECS_THREAD_TASK(IntegratePositions);

template<bool schedule_element>
ECS_THREAD_TASK(IntegrateVelocities);

void AddSolverCommonTasks(ModuleTaskFunctionData* data);