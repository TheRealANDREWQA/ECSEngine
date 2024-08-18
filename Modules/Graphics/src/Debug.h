#pragma once
#include "ECSEngineContainersCommon.h"
#include "ECSEngineEntities.h"
#include "ECSEngineColor.h"
#include "ECSEngineTaskTypes.h"

using namespace ECSEngine;

namespace ECSEngine {
	struct World;
	struct ModuleTaskFunctionData;
}

struct GraphicsDebugSolidGroup {
	Stream<Entity> entities;
	Color color;
	bool randomize_color = true;
};

struct GraphicsDebugData {
	// Use a private allocator for ourselves
	MemoryManager allocator;
	// This is a table that only accelerates boolean questions regarding whether or not an entity is in debug draw mode or not
	HashTableEmpty<Entity, HashFunctionPowerOfTwo> entities_table;
	ResizableStream<GraphicsDebugSolidGroup> groups;
	// These are predetermined colors that can be assigned to groups based on their index
	Stream<Color> colors;
	// This is a vertex buffer that is used in conjunction with the default graphics solid color
	// Shader helper and it stores the information about the meshes that need to be rendered
	VertexBuffer instanced_vertex_buffer;
};

// Returns nullptr if there is no such data
GraphicsDebugData* GetGraphicsDebugData(World* world);

// Returns nullptr if there is no such data
const GraphicsDebugData* GetGraphicsDebugData(const World* world);

void GraphicsDebugInitialize(World* world);

// Adds a new group to the debug draw - the entities must not belong to an existing group
void GraphicsDebugAddSolidGroup(World* world, GraphicsDebugSolidGroup group);

// Overrides the existing groups with the ones given
void GraphicsDebugSetSolidGroups(World* world, Stream<GraphicsDebugSolidGroup> groups);

// Clears the debug draw groups
void GraphicsDebugResetSolidGroups(World* world);

void RegisterGraphicsDebugTasks(ModuleTaskFunctionData* task_data);