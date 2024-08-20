#include "pch.h"
#include "Debug.h"
#include "ECSEngineWorld.h"
#include "GraphicsComponents.h"

#define GRAPHICS_DEBUG_DATA_STRING "GraphicsDebugData"
#define ALLOCATOR_CAPACITY ECS_MB
#define ALLOCATOR_BACKUP_CAPACITY ECS_MB
#define RANDOM_COLOR_COUNT ECS_KB * 4
#define ENTITIES_TABLE_INITIAL_CAPACITY ECS_KB

// Create an iterator for the collection such that it can be used by the ForEach
struct Iterator : IteratorInterface<const Entity> {
	ECS_INLINE Iterator(const GraphicsDebugData* _data) : data(_data), group_index(0), stream_index(0) {}
	
	const Entity* Get() {
		const Entity* entity = data->groups[group_index].entities.buffer + stream_index;
		stream_index++;
		if (stream_index == data->groups[group_index].entities.size) {
			stream_index = 0;
			group_index++;
		}
		return entity;
	}

	bool IsContiguous() const override {
		return false;
	}

	IteratorInterface<const Entity>* CreateSubIteratorImpl(AllocatorPolymorphic allocator, size_t count) override {
		Iterator* iterator = (Iterator*)AllocateEx(allocator, sizeof(Iterator));
		*iterator = Iterator(data);
		iterator->group_index = group_index;
		iterator->stream_index = stream_index;
		stream_index += count;
		while (stream_index >= data->groups[group_index].entities.size) {
			stream_index -= data->groups[group_index].entities.size;
			group_index++;
		}
		return iterator;
	}

	// Computes the remaining count of entities from the current debug data
	void ComputeRemainingCount() {
		remaining_count = 0;
		for (unsigned int index = 0; index < data->groups.size; index++) {
			remaining_count += data->groups[index].entities.size;
		}
	}

	const GraphicsDebugData* data;
	unsigned int group_index;
	unsigned int stream_index;
};

static void GraphicsDebugAddSolidGroup(World* world, GraphicsDebugData* data, GraphicsDebugSolidGroup group) {
	if (group.randomize_color) {
		unsigned int entry_index = data->groups.size % RANDOM_COLOR_COUNT;
		group.color = data->colors[entry_index];
	}
	
	GraphicsDebugData::Group debug_group;
	debug_group.color = group.color;
	// Reallocate the buffer
	debug_group.entities = group.entities.Copy(&data->allocator);
	// Create an instanced vertex buffer for the group
	debug_group.instanced_vertex_buffer = world->graphics->SolidColorHelperShaderCreateVertexBufferInstances(debug_group.entities.size, false);
	data->groups.Add(debug_group);

	// Add the entities to the hash table
	for (size_t index = 0; index < group.entities.size; index++) {
		ECS_CRASH_CONDITION(data->entities_table.Find(group.entities[index]) == -1, "Graphics: Debug draw entity {#} already exists!", group.entities[index]);
		data->entities_table.Insert({}, group.entities[index]);
	}
}

static void GraphicsDebugResetSolidGroups(World* world, GraphicsDebugData* data) {
	// Deallocate the entity buffers and the vertex buffers
	for (unsigned int index = 0; index < data->groups.size; index++) {
		world->graphics->FreeResource(data->groups[index].instanced_vertex_buffer);
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

void GraphicsDebugInitialize(World* world, StaticThreadTaskInitializeInfo* info) {
	// Bind the data only if it doesn't exist already
	if (world->system_manager->TryGetData(GRAPHICS_DEBUG_DATA_STRING) == nullptr) {
		GraphicsDebugData debug_data;
		GraphicsDebugData* data = (GraphicsDebugData*)world->system_manager->BindData(GRAPHICS_DEBUG_DATA_STRING, &debug_data, sizeof(debug_data));
		data->allocator = MemoryManager(ALLOCATOR_CAPACITY, ECS_KB * 16, ALLOCATOR_BACKUP_CAPACITY, world->memory);

		data->colors.Initialize(&data->allocator, RANDOM_COLOR_COUNT);
		CreateColorizeBuffer(data->colors);

		data->entities_table.Initialize(&data->allocator, ENTITIES_TABLE_INITIAL_CAPACITY);
		data->groups.Initialize(&data->allocator, 8);
	}
}

void GraphicsDebugAddSolidGroup(World* world, GraphicsDebugSolidGroup group) {
	GraphicsDebugData* data = GetGraphicsDebugData(world);
	GraphicsDebugAddSolidGroup(world, data, group);
}

void GraphicsDebugSetSolidGroups(World* world, Stream<GraphicsDebugSolidGroup> groups) {
	GraphicsDebugData* data = GetGraphicsDebugData(world);
	GraphicsDebugResetSolidGroups(world, data);
	for (size_t index = 0; index < groups.size; index++) {
		GraphicsDebugAddSolidGroup(world, data, groups[index]);
	}
}

void GraphicsDebugResetSolidGroups(World* world) {
	GraphicsDebugResetSolidGroups(world, GetGraphicsDebugData(world));
}

struct GraphicsDebugDrawData {
	const GraphicsDebugData* data;
	const Matrix* camera_matrix;
	GraphicsSolidColorInstance* instance_vertex_data;
};

// During this pass, only the instance vertex buffer must be filled in
static void GraphicsDebugDraw_Impl(
	const ForEachEntityData* for_each_data,
	const RenderMesh* render_mesh,
	const Translation* translation,
	const Rotation* rotation,
	const Scale* scale
) {
	const GraphicsDebugDrawData* data = (const GraphicsDebugDrawData*)for_each_data->user_data;
	if (render_mesh->Validate()) {
		Matrix transform_matrix = GetEntityTransformMatrix(translation, rotation, scale);

		Matrix mvp_matrix = transform_matrix * *data->camera_matrix;
		mvp_matrix = MatrixGPU(mvp_matrix);
		//data->
	}
}

template<bool schedule_element>
static ECS_THREAD_TASK(GraphicsDebugDraw) {
	Iterator iterator = nullptr;
	IteratorInterface<const Entity>* iterator_pointer = nullptr;
	const GraphicsDebugData* data = nullptr;
	if constexpr (!schedule_element)
	{
		data = GetGraphicsDebugData(world);
		iterator = Iterator(data);
		iterator.ComputeRemainingCount();
		iterator_pointer = &iterator;
	}

	ForEachSelectionOptions options;
	options.condition = [](unsigned int thread_id, World* world, void* user_data) {
		const GraphicsDebugData* data = (const GraphicsDebugData*)user_data;
		return data->groups.size > 0;
	};
	
	auto kernel = ForEachEntitySelectionCommit<schedule_element,
		QueryRead<RenderMesh>,
		QueryOptional<QueryRead<Translation>>,
		QueryOptional<QueryRead<Rotation>>,
		QueryOptional<QueryRead<Scale>>
	>(iterator_pointer, thread_id, world);

	kernel.Function(GraphicsDebugDraw_Impl, (void*)data);
}

void RegisterGraphicsDebugTasks(ModuleTaskFunctionData* task_data) {
	TaskSchedulerElement element;
	element.task_group = ECS_THREAD_TASK_FINALIZE_LATE;
	element.initialize_task_function = GraphicsDebugInitialize;
	ECS_REGISTER_FOR_EACH_TASK(element, GraphicsDebugDraw, task_data);
}