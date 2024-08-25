#pragma once
#include "Export.h"
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
};

struct GraphicsDebugSolidGroupIterator {
	// Returns the number of entities the current group has
	virtual unsigned int GetCurrentGroupEntityCount() const = 0;

	// Fills the entity buffer with the current's group entities and advances to the next group
	virtual GraphicsDebugSolidGroup FillCurrentGroupAndAdvanced(Entity* entity_buffer) = 0;

	// Returns the number of groups there are in total
	virtual unsigned int GetGroupCount() const = 0;
};

// Returns nullptr if there is no such data
GRAPHICS_API GraphicsDebugData* GetGraphicsDebugData(World* world);

// Returns nullptr if there is no such data
GRAPHICS_API const GraphicsDebugData* GetGraphicsDebugData(const World* world);

// Allocates a buffer of entities from the graphics debug data allocator. Use function GraphicsDebugSetSolidGroupsAllocated
// To set the groups afterwards. Don't use the buffer for something else!
ECS_INLINE Entity* GraphicsDebugAllocateEntities(GraphicsDebugData* data, unsigned int count) {
	return (Entity*)data->allocator.Allocate(sizeof(Entity) * (size_t)count);
}

// Adds a new group to the debug draw - the entities must not belong to an existing group
GRAPHICS_API void GraphicsDebugAddSolidGroup(World* world, GraphicsDebugSolidGroup group);

// Overrides the existing groups with the ones given
GRAPHICS_API void GraphicsDebugSetSolidGroups(World* world, Stream<GraphicsDebugSolidGroup> groups);

// Overrides the existing groups with the ones given by the iterator. This version
// Does not need you to create temporary buffers that are then copied, since the buffers
// Can be allocated and filled in by you.
GRAPHICS_API void GraphicsDebugSetSolidGroupsIterator(World* world, GraphicsDebugSolidGroupIterator* iterator);

// If you allocated the entities using the function GraphicsDebugAllocateEntities, you can call
// This function to set the groups.
GRAPHICS_API void GraphicsDebugSetSolidGroupsAllocated(World* world, GraphicsDebugData* data, Stream<GraphicsDebugSolidGroup> groups);

// Clears the debug draw groups
GRAPHICS_API void GraphicsDebugResetSolidGroups(World* world);

GRAPHICS_API void RegisterGraphicsDebugTasks(ModuleTaskFunctionData* task_data);