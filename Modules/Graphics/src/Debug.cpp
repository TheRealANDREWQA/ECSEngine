#include "pch.h"
#include "Debug.h"
#include "ECSEngineWorld.h"
#include "GraphicsComponents.h"
#include "ECSEngineECSRuntimeResources.h"

#define GRAPHICS_DEBUG_DATA_STRING "GraphicsDebugData"
#define ALLOCATOR_CAPACITY ECS_MB
#define ALLOCATOR_BACKUP_CAPACITY ECS_MB
#define RANDOM_COLOR_COUNT ECS_KB * 4
#define ENTITIES_TABLE_INITIAL_CAPACITY ECS_KB

// Create an iterator for the collection such that it can be used by the ForEach
struct Iterator : IteratorInterface<const Entity> {
	ECS_INLINE Iterator(const GraphicsDebugData* _data) : data(_data), group_index(0), stream_index(0), IteratorInterface<const Entity>(0) {
		if (data != nullptr) {
			ComputeRemainingCount();
		}
	}
	
	const Entity* Get() {
		last_color = data->groups[group_index].color;
		const Entity* entity = data->groups[group_index].entities.buffer + stream_index;
		stream_index++;
		if (stream_index == data->groups[group_index].entities.size) {
			stream_index = 0;
			group_index++;
		}
		return entity;
	}

	ECS_INLINE Color GetLastColor() const {
		return last_color;
	}

	bool IsContiguous() const override {
		return false;
	}

	IteratorInterface<const Entity>* CreateSubIteratorImpl(AllocatorPolymorphic allocator, size_t count) override {
		Iterator* iterator = (Iterator*)Allocate(allocator, sizeof(Iterator));
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
	// Set such that it can be returned immediately
	Color last_color;
};

static GraphicsDebugSolidGroup* GraphicsDebugReserveSolidGroup(GraphicsDebugData* data, unsigned int entity_count) {
	unsigned int group_index = data->groups.ReserveRange();
	data->groups[group_index].entities.Initialize(&data->allocator, entity_count);
	return data->groups.buffer + group_index;
}

static Color GetGroupColor(const GraphicsDebugData* data, Color color, unsigned int group_index, bool randomize_color) {
	Color result_color = color;
	if (randomize_color) {
		result_color = data->colors[group_index % RANDOM_COLOR_COUNT];
	}
	return result_color;
}

static void AddEntitiesToTable(GraphicsDebugData* data, Stream<Entity> entities) {
	for (size_t index = 0; index < entities.size; index++) {
		ECS_CRASH_CONDITION(data->entities_table.Find(entities[index]) == -1, "Graphics: Debug draw entity {#} already exists!", entities[index]);
		data->entities_table.Insert({}, entities[index]);
	}
}

static void GraphicsDebugAddSolidGroup(World* world, GraphicsDebugData* data, GraphicsDebugSolidGroup group) {
	if (group.entities.size == 0) {
		return;
	}

	group.color = GetGroupColor(data, group.color, data->groups.size, group.randomize_color);

	// Reallocate the buffer
	group.entities = group.entities.Copy(&data->allocator);
	data->groups.Add(group);

	// Add the entities to the hash table
	AddEntitiesToTable(data, group.entities);
}

static void GraphicsDebugResetSolidGroups(World* world, GraphicsDebugData* data) {
	// Deallocate the entity buffers and the vertex buffers
	for (unsigned int index = 0; index < data->groups.size; index++) {
		data->groups[index].entities.Deallocate(&data->allocator);
	}

	data->groups.Clear();
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

ECS_INLINE static unsigned int GetTotalEntityCount(const GraphicsDebugData* data) {
	unsigned int count = 0;
	for (unsigned int index = 0; index < data->groups.size; index++) {
		count += data->groups[index].entities.size;
	}
	return count;
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
	data->groups.Reserve(groups.size);
	for (size_t index = 0; index < groups.size; index++) {
		GraphicsDebugAddSolidGroup(world, data, groups[index]);
	}
}

void GraphicsDebugSetSolidGroupsIterator(World* world, GraphicsDebugSolidGroupIterator* iterator) {
	GraphicsDebugData* data = GetGraphicsDebugData(world);
	GraphicsDebugResetSolidGroups(world, data);

	unsigned int group_count = iterator->GetGroupCount();
	data->groups.ReserveRange(group_count);
	for (unsigned int index = 0; index < group_count; index++) {
		unsigned int entity_count = iterator->GetCurrentGroupEntityCount();
		if (entity_count > 0) {
			GraphicsDebugSolidGroup* group_pointer = GraphicsDebugReserveSolidGroup(data, entity_count);
			GraphicsDebugSolidGroup group = iterator->FillCurrentGroupAndAdvanced(group_pointer->entities.buffer);
			group_pointer->color = GetGroupColor(data, group.color, index, group.randomize_color);
			group_pointer->randomize_color = group.randomize_color;
			group_pointer->entities = group.entities;
			AddEntitiesToTable(data, group_pointer->entities);
		}
	}
}

void GraphicsDebugSetSolidGroupsAllocated(World* world, GraphicsDebugData* data, Stream<GraphicsDebugSolidGroup> groups) {
	GraphicsDebugResetSolidGroups(world, data);
	data->groups.Reserve(groups.size);
	for (size_t index = 0; index < groups.size; index++) {
		if (groups[index].entities.size > 0) {
			unsigned int added_group_index = data->groups.Add(groups[index]);
			data->groups[added_group_index].color = GetGroupColor(data, groups[index].color, index, groups[index].randomize_color);
			AddEntitiesToTable(data, groups[index].entities);
		}
	}
}

void GraphicsDebugResetSolidGroups(World* world) {
	GraphicsDebugResetSolidGroups(world, GetGraphicsDebugData(world));
}

struct GraphicsDebugDrawData {
	const GraphicsDebugData* data;
	const Matrix* camera_matrix;
	VertexBuffer group_vertex_buffer;
	GraphicsSolidColorInstance* solid_color_instances;
	unsigned int index_buffer_count;
};


static bool GraphicsDebugDraw_GroupInitializeImpl(ForEachEntitySelectionSharedGroupingUntypedFunctorData* for_each_data) {
	GraphicsDebugDrawData* data = (GraphicsDebugDrawData*)for_each_data->user_data;
	const RenderMesh* render_mesh = for_each_data->world->entity_manager->GetSharedData<RenderMesh>(for_each_data->shared_instance);
	if (!render_mesh->Validate()) {
		return false;
	}

	// Bind the mesh now
	for_each_data->world->graphics->BindMesh(render_mesh->mesh->mesh);
	// Write the index buffer count now
	data->index_buffer_count = render_mesh->mesh->mesh.index_buffer.count;
	data->group_vertex_buffer = for_each_data->world->graphics->SolidColorHelperShaderCreateVertexBufferInstances(for_each_data->entities.size);
	data->solid_color_instances = for_each_data->world->graphics->SolidColorHelperShaderMap(data->group_vertex_buffer);
	return true;
}

static bool GraphicsDebugDraw_GroupFinalizeImpl(ForEachEntitySelectionSharedGroupingUntypedFunctorData* for_each_data) {
	GraphicsDebugDrawData* data = (GraphicsDebugDrawData*)for_each_data->user_data;
	for_each_data->world->graphics->SolidColorHelperShaderUnmap(data->group_vertex_buffer);
	for_each_data->world->graphics->SolidColorHelperShaderBindBuffer(data->group_vertex_buffer);
	for_each_data->world->graphics->DrawIndexedInstanced(data->index_buffer_count, for_each_data->entities.size);
	data->group_vertex_buffer.Release();
	return true;
}


// During this pass, only the instance vertex buffer must be filled in
static void GraphicsDebugDraw_EntityImpl(
	const ForEachEntitySelectionData* for_each_data,
	const RenderMesh* render_mesh,
	const Translation* translation,
	const Rotation* rotation,
	const Scale* scale
) {
	const GraphicsDebugDrawData* data = (const GraphicsDebugDrawData*)for_each_data->user_data;
	Iterator* iterator = (Iterator*)for_each_data->iterator;
	Matrix transform_matrix = GetEntityTransformMatrix(translation, rotation, scale);

	Matrix mvp_matrix = transform_matrix * *data->camera_matrix;
	mvp_matrix = MatrixGPU(mvp_matrix);
	data->solid_color_instances[for_each_data->index] = { mvp_matrix, iterator->GetLastColor() };
}

template<bool schedule_element>
static ECS_THREAD_TASK(GraphicsDebugDraw) {
	Iterator iterator = nullptr;
	IteratorInterface<const Entity>* iterator_pointer = nullptr;
	GraphicsDebugDrawData draw_data;
	memset(&draw_data, 0, sizeof(draw_data));

	if constexpr (!schedule_element) {
		draw_data.data = GetGraphicsDebugData(world);
		iterator = Iterator(draw_data.data);
		iterator.ComputeRemainingCount();
		iterator_pointer = &iterator;
		if (draw_data.data->groups.size > 0) {
			CameraCached world_camera;
			// This will check for both the runtime camera, but also for the camera component
			if (GetWorldCamera(world, world_camera)) {
				// Use this design in case in the future we will parallelize the call and the matrix needs to be shared
				Matrix* matrix = (Matrix*)world->task_manager->AllocateTempBuffer(thread_id, sizeof(Matrix));
				*matrix = world_camera.GetViewProjectionMatrix();
				draw_data.camera_matrix = matrix;

				// Bind the shader now, once
				world->graphics->BindHelperShader(ECS_GRAPHICS_SHADER_HELPER_SOLID_COLOR);
			}
		}
	}

	ForEachSelectionOptions options;
	options.condition = [](unsigned int thread_id, World* world, void* user_data) {
		const GraphicsDebugDrawData* data = (const GraphicsDebugDrawData*)user_data;
		return data->data->groups.size > 0;
	};
	
	auto kernel = ForEachEntitySelectionSharedGroupingCommit<schedule_element,
		QueryRead<RenderMesh>,
		QueryOptional<QueryRead<Translation>>,
		QueryOptional<QueryRead<Rotation>>,
		QueryOptional<QueryRead<Scale>>
	>(iterator_pointer, thread_id, world, options);

	kernel.Function(GraphicsDebugDraw_GroupInitializeImpl, GraphicsDebugDraw_GroupFinalizeImpl, GraphicsDebugDraw_EntityImpl, &draw_data);
}

void RegisterGraphicsDebugTasks(ModuleTaskFunctionData* task_data) {
	TaskSchedulerElement element;
	element.task_group = ECS_THREAD_TASK_FINALIZE_LATE;
	element.initialize_task_function = GraphicsDebugInitialize;
	ECS_REGISTER_FOR_EACH_TASK(element, GraphicsDebugDraw, task_data);
}