#pragma once
#include "Export.h"

template<bool schedule_element>
ECS_THREAD_TASK(DebugDrawPhysicsIslands);

void SetPhysicsIslandDraw(World* world, bool set);