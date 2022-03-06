#include "editorpch.h"
#include "EditorWorld.h"
#include "EditorState.h"

using namespace ECSEngine;

// -------------------------------------------------------------------------------------------------------------

void EditorStateInitializeWorld(EditorState* editor_state, unsigned int index)
{
	// Assert that the world has been deallocated before
	ECS_ASSERT(editor_state->worlds[index].is_empty);

	// Create a graphics object that will inherit the graphics object of the editor
	// It must have a separate allocator. At the moment, it cannot be allocated from the global memory manager
	// of the world because that one is created inside the world. Instead, allocate this memory manager from the global
	// memory manager of the editor and make sure that it gets deallocated at the end
	MemoryManager* graphics_memory = (MemoryManager*)malloc(sizeof(MemoryManager));
	*graphics_memory = DefaultGraphicsAllocator(editor_state->GlobalMemoryManager());

	Graphics* graphics = (Graphics*)malloc(sizeof(Graphics));
	new (graphics) Graphics(editor_state->Graphics(), editor_state->viewport_render_target, editor_state->viewport_texture_depth);

	// Create the world and mark it as prepared
	WorldDescriptor descriptor = editor_state->project_descriptor;
	descriptor.graphics = graphics;

	editor_state->worlds[index].world = World(descriptor);
	editor_state->worlds[index].is_empty = false;
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateReleaseWorld(EditorState* editor_state, unsigned int index)
{
	World* world = &editor_state->worlds[index].world;
	// Call the destroy on this world and mark it as unprepared
	DestroyWorld(world);

	// Release the graphics pointer that was previously allocated and its allocator. Make sure to deallocate the entire allocator.
	world->graphics->m_allocator->Free();
	free(world->graphics->m_allocator);
	free(world->graphics);

	editor_state->worlds[index].is_empty = false;
}

// -----------------------------------------------------------------------------------------------------------------

void EditorStateCopyWorld(EditorState* editor_state, unsigned int destination_index, unsigned int source_index)
{
	ECS_ASSERT(!editor_state->worlds[destination_index].is_empty && !editor_state->worlds[source_index].is_empty);

	// Firstly copy the entity data
	editor_state->worlds[destination_index].world.entity_manager->CopyOther(editor_state->worlds[source_index].world.entity_manager);

	// Open shared handles to the graphics resources
	
}

// -------------------------------------------------------------------------------------------------------------

struct EditorStateCopyWorldTaskData {
	EditorState* editor_state;
	unsigned int destination_index;
};

ECS_THREAD_TASK(EditorStateCopyWorldTask) {
	EditorStateCopyWorldTaskData* data = (EditorStateCopyWorldTaskData*)_data;

	// Copy the world - just use the API
	EditorStateCopyWorld(data->editor_state, data->destination_index, data->editor_state->scene_world);
	data->editor_state->copy_world_count.fetch_sub(1, ECS_RELAXED);
}

void EditorStateNextWorld(EditorState* editor_state) {
	// Do a sleep wait for the flag to be cleared
	// The resolution can be lowered or increased as needed
	while (editor_state->copy_world_count.load(ECS_RELAXED) >= EDITOR_SCENE_BUFFERING_COUNT - 1)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	editor_state->copy_world_count.fetch_add(1, ECS_RELAXED);

	EditorStateReleaseWorld(editor_state, editor_state->active_world);
	EditorStateInitializeWorld(editor_state, editor_state->active_world);
	EditorStateAddBackgroundTask(editor_state, ECS_THREAD_TASK_NAME(EditorStateCopyWorldTask, editor_state, 0));
}

// -------------------------------------------------------------------------------------------------------------

struct EditorWorldRuntimeBarrierData {
	bool is_paused;
	bool should_step;
};

// Controls the flow of the runtime - it either blocks the threads or allows them to run
// This does not refer to the editor state because it might be useful for multiple scene replays at a time to allow
// each to do a step or to be stopped
ECS_THREAD_TASK(EditorWorldRuntimeBarrier) {
	EditorWorldRuntimeBarrierData* data = (EditorWorldRuntimeBarrierData*)_data;
	if (data->should_step) {
		data->should_step = false;
	}
	else {
		while (data->is_paused && !data->should_step) {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		if (data->should_step) {
			data->should_step = false;
		}
	}
}

void EditorStateSimulateWorld(World* world)
{
	ThreadTask barrier_task = world->task_manager->GetTask(world->task_manager->GetTaskCount() - 1);
	EditorWorldRuntimeBarrierData* barrier_data = (EditorWorldRuntimeBarrierData*)barrier_task.data;
	barrier_data->should_step = true;
}

// -------------------------------------------------------------------------------------------------------------