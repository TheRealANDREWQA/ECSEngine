#pragma once
#include "ECSEngineContainersCommon.h"
#include "ECSEngineEntities.h"
#include "ECSEngineColor.h"

using namespace ECSEngine;

namespace ECSEngine {
	struct World;
}

struct GraphicsDebugSolidGroup {
	Stream<Entity> entities;
	Color color;
};

void GraphicsDebugSetSolidGroups(World* world, Stream<GraphicsDebugSolidGroup> groups);

void GraphicsDebugResetSolidGroups(World* world);