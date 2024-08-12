#include "pch.h"
#include "Debug.h"
#include "ECSEngineWorld.h"

#define GRAPHICS_DEBUG_DATA_STRING "__GraphicsDebugData"
#define ALLOCATOR_CAPACITY ECS_MB
#define ALLOCATOR_BACKUP_CAPACITY ECS_MB
#define RANDOM_COLOR_COUNT ECS_KB * 4
#define ENTITIES_TABLE_INITIAL_CAPACITY ECS_KB

static void GraphicsDebugAddSolidGroup(GraphicsDebugData* data, GraphicsDebugSolidGroup group) {
	if (group.randomize_color) {
		unsigned int entry_index = data->groups.size % RANDOM_COLOR_COUNT;
		group.color = data->colors[entry_index];
	}
	
	// Reallocate the buffer
	group.entities = group.entities.Copy(&data->allocator);
	data->groups.Add(group);

	// Add the entities to the hash table
	for (size_t index = 0; index < group.entities.size; index++) {
		ECS_CRASH_CONDITION(data->entities_table.Find(group.entities[index]) == -1, "Graphics: Debug draw entity {#} already exists!", group.entities[index]);
		data->entities_table.Insert({}, group.entities[index]);
	}
}

static void GraphicsDebugResetSolidGroups(GraphicsDebugData* data) {
	// Deallocate the entity buffers
	for (unsigned int index = 0; index < data->groups.size; index++) {
		data->groups[index].entities.Deallocate(&data->allocator);
	}

	data->groups.Reset();
	// Trim the buffer to reduce memory consumption in case there were many additions
	data->groups.Trim(8);
	// The entity hash table should also be cleared
	if (data->entities_table.GetCapacity() > ENTITIES_TABLE_INITIAL_CAPACITY) {
		data->entities_table.Deallocate(&data->allocator);
		data->entities_table.Initialize(&data->allocator, ENTITIES_TABLE_INITIAL_CAPACITY);
	}
	else {
		data->entities_table.Clear();
	}
}

GraphicsDebugData* GetGraphicsDebugData(World* world) {
	return (GraphicsDebugData*)world->system_manager->TryGetData(GRAPHICS_DEBUG_DATA_STRING);
}

const GraphicsDebugData* GetGraphicsDebugData(const World* world) {
	return (const GraphicsDebugData*)world->system_manager->TryGetData(GRAPHICS_DEBUG_DATA_STRING);
}

void GraphicsDebugInitialize(World* world) {
	GraphicsDebugData debug_data;
	GraphicsDebugData* data = (GraphicsDebugData*)world->system_manager->BindData(GRAPHICS_DEBUG_DATA_STRING, &debug_data, sizeof(debug_data));
	data->allocator = MemoryManager(ALLOCATOR_CAPACITY, ECS_KB * 16, ALLOCATOR_BACKUP_CAPACITY, world->memory);
	
	data->colors.Initialize(&data->allocator, RANDOM_COLOR_COUNT);
	CreateColorizeBuffer(data->colors);

	data->entities_table.Initialize(&data->allocator, ENTITIES_TABLE_INITIAL_CAPACITY);
	data->groups.Initialize(&data->allocator, 8);
}

void GraphicsDebugAddSolidGroup(World* world, GraphicsDebugSolidGroup group) {
	GraphicsDebugData* data = GetGraphicsDebugData(world);
	GraphicsDebugAddSolidGroup(data, group);
}

void GraphicsDebugSetSolidGroups(World* world, Stream<GraphicsDebugSolidGroup> groups) {
	GraphicsDebugData* data = GetGraphicsDebugData(world);
	GraphicsDebugResetSolidGroups(data);
	for (size_t index = 0; index < groups.size; index++) {
		GraphicsDebugAddSolidGroup(data, groups[index]);
	}
}

void GraphicsDebugResetSolidGroups(World* world) {
	GraphicsDebugResetSolidGroups(GetGraphicsDebugData(world));
}

ECS_THREAD_TASK(GraphicsDebugDraw) {
	
}

TaskSchedulerElement GetGraphicsDebugScheduleElement(ModuleTaskFunctionData* task_data) {
	TaskSchedulerElement element;

	element.task_group = ECS_THREAD_TASK_FINALIZE_LATE;

	return element;
}