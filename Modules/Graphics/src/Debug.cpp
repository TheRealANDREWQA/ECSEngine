#include "pch.h"
#include "Debug.h"
#include "ECSEngineWorld.h"

struct DebugData {
	ResizableStream<GraphicsDebugSolidGroup> groups;
};

void GraphicsDebugSetSolidGroups(World* world, Stream<GraphicsDebugSolidGroup> groups) {

}

void GraphicsDebugResetSolidGroups(World* world) {

}