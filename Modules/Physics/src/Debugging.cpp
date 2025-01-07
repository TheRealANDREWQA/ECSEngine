#include "pch.h"
#include "Debugging.h"
#include "SolverData.h"
#include "ECSEngineWorld.h"
#include "ECSEngineModule.h"
#include "ECSEngineForEach.h"
#include "Graphics/src/Debug.h"

#define DEBUG_DATA_STRING "PhysicsDebugData"

struct DebugData {
	bool should_draw;
};

static ECS_THREAD_TASK(DebugDrawPhysicsIslands) {
	// We don't perform the rendering ourselves, let the Graphics debugging utility do that
	DebugData* data = (DebugData*)world->system_manager->GetData(DEBUG_DATA_STRING);
	if (world->keyboard->IsDown(ECS_KEY_LEFT_CTRL) && world->keyboard->IsPressed(ECS_KEY_I)) {
		data->should_draw = !data->should_draw;
		if (!data->should_draw) {
			// Reset the debug draws
			GraphicsDebugResetSolidGroups(world);
		}
	}

	if (data->should_draw) {
		const SolverData* solver_data = world->entity_manager->GetGlobalComponent<SolverData>();
		// Do a prepass to determine the entity counts, allocate them from the debug allocator
		// And then fill the buffers

		unsigned int max_island_count = solver_data->island_manager.islands.GetMaxValidHandle();
		// These are the counts for the handle values - not for the island indices
		unsigned int* island_entity_counts = (unsigned int*)world->entity_manager->AllocateTemporaryBuffer(sizeof(unsigned int) * max_island_count);
		memset(island_entity_counts, 0, sizeof(unsigned int) * (size_t)max_island_count);
		solver_data->island_manager.entity_table.ForEachConst([island_entity_counts](IslandHandle handle, Entity entity) {
			island_entity_counts[handle.handle]++;
		});

		GraphicsDebugData* graphics_data = GetGraphicsDebugData(world);

		// Here we allocate the entity buffers
		size_t island_count = solver_data->island_manager.islands.set.size;
		Stream<GraphicsDebugSolidGroup> graphics_groups;
		graphics_groups.Initialize(world->entity_manager->TemporaryAllocator(), island_count);
		for (size_t index = 0; index < island_count; index++) {
			unsigned int count = island_entity_counts[solver_data->island_manager.islands.GetHandleFromIndex(index)];
			Entity* entity_buffer = GraphicsDebugAllocateEntities(graphics_data, count);
			graphics_groups[index].entities.InitializeFromBuffer(entity_buffer, count);
			graphics_groups[index].entities.size = 0;
			graphics_groups[index].randomize_color = true;
		}

		// Go through the entity table once more and gather the entities themselves
		solver_data->island_manager.entity_table.ForEachConst([&graphics_groups, solver_data](IslandHandle handle, Entity entity) {
			unsigned int island_index = solver_data->island_manager.islands.GetIndexFromHandle(handle.handle);
			graphics_groups[island_index].entities.Add(entity);
		});

		for (size_t index = 0; index < island_count; index++) {
			ECS_CRASH_CONDITION(graphics_groups[index].entities.size == island_entity_counts[solver_data->island_manager.islands.GetHandleFromIndex(index)], "Graphics group count did not match!");
		}
		GraphicsDebugSetSolidGroupsAllocated(world, graphics_data, graphics_groups);
	}
}

static void DebugDrawInitialize(World* world, StaticThreadTaskInitializeInfo* info) {
	DebugData debug_data = { false };
	world->system_manager->BindDataUnique(DEBUG_DATA_STRING, &debug_data, sizeof(debug_data));
}

void RegisterDebugTasks(ModuleTaskFunctionData* task_data) {
	TaskSchedulerElement schedule_element;
	schedule_element.task_group = ECS_THREAD_TASK_SIMULATE_LATE;
	schedule_element.initialize_task_function = DebugDrawInitialize;
	ECS_SET_SCHEDULE_TASK_FUNCTION(schedule_element, DebugDrawPhysicsIslands);
	task_data->tasks->AddAssert(&schedule_element);
}

void SetPhysicsIslandDraw(World* world, bool set) {
	DebugData* data = (DebugData*)world->system_manager->GetData(DEBUG_DATA_STRING);
	data->should_draw = set;
}