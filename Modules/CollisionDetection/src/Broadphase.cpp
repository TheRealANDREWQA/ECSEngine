#include "pch.h"
#include "Broadphase.h"
#include "Graphics/src/GraphicsComponents.h"
#include "CollisionDetectionComponents.h"
#include "FixedGrid.h"
#include "Narrowphase.h"

static void UpdateBroadphaseGrid(
	ForEachEntityData* for_each_data,
	const Translation* translation,
	const RenderMesh* mesh,
	const Rotation* rotation,
	const Scale* scale
) {
	if (mesh->Validate()) {
		ECS_STACK_CAPACITY_STREAM(CollisionInfo, collisions, 512);
		FixedGrid* fixed_grid = (FixedGrid*)for_each_data->user_data;
		float3 translation_value = float3::Splat(0.0f);
		Matrix rotation_matrix = MatrixIdentity();
		float3 scale_value = float3::Splat(1.0f);
		if (translation != nullptr) {
			translation_value = translation->value;
		}
		if (rotation != nullptr) {
			rotation_matrix = QuaternionToMatrix(rotation->value);
		}
		if (scale != nullptr) {
			scale_value = scale->value;
		}

		AABBScalar aabb = TransformAABB(mesh->mesh->mesh.bounds, translation_value, rotation_matrix, scale_value);
		fixed_grid->InsertEntry(for_each_data->thread_id, for_each_data->world, for_each_data->entity, 0, aabb);
	}
}

template<bool get_query>
ECS_THREAD_TASK(CollisionBroadphase) {
	FixedGrid* fixed_grid = nullptr;
	if constexpr (!get_query) {
		fixed_grid = world->entity_manager->GetGlobalComponent<FixedGrid>();
		fixed_grid->EndFrame();
		fixed_grid->StartFrame();
	}
	ForEachEntityCommit<get_query, QueryRead<Translation>, QueryRead<RenderMesh>, QueryOptional<QueryRead<Rotation>>, QueryOptional<QueryRead<Scale>>>(thread_id, world)
		.Function(UpdateBroadphaseGrid, fixed_grid);
}

ECS_THREAD_TASK(EmptyGridHandler) {}

void InitializeCollisionBroadphase(World* world, StaticThreadTaskInitializeInfo* info) {
	//if (info->previous_data.size > 0) {
	//	FixedGrid* previous_grid = (FixedGrid*)info->previous_data.buffer;
	//	previous_grid->Clear();
	//}

	if (!world->entity_manager->ExistsGlobalComponent<FixedGrid>()) {
		FixedGrid* fixed_grid = world->entity_manager->RegisterGlobalComponentCommit<FixedGrid>(nullptr);
		fixed_grid->Initialize(world->GetGlobalComponentAllocator(), { 128, 128, 128 }, { 2, 2, 2 }, 10, NarrowphaseGridHandler, nullptr, 0);
		fixed_grid->EnableLayerCollisions(0, 0);
	}
}

ECS_THREAD_TASK(CollisionBroadphaseEndFrame) {
	//FixedGrid* fixed_grid = world->entity_manager->GetGlobalComponent<FixedGrid>();
	//fixed_grid->EndFrame();
}

ECS_THREAD_TASK(CollisionBroadphaseDisplayDebugGrid) {
	FixedGrid* fixed_grid = world->entity_manager->GetGlobalComponent<FixedGrid>();

	DebugGrid grid;
	grid.color = ECS_COLOR_AQUA;
	grid.cell_size = fixed_grid->spatial_grid.half_cell_size / fixed_grid->spatial_grid.smaller_cell_size_factor;
	grid.translation = float3::Splat(0.0f);
	grid.dimensions = fixed_grid->spatial_grid.dimensions;

	// Create a temporary reference to the inserted cells, to avoid to have to copy that data
	static size_t TEMPORARY_BUFFER_STORAGE[32];
	CapacityStream<void> TEMPORARY_BUFFER(TEMPORARY_BUFFER_STORAGE, 0, sizeof(TEMPORARY_BUFFER_STORAGE));
	grid.valid_cells = DeckPowerOfTwo<uint3>::InitializeTempReference(fixed_grid->spatial_grid.inserted_cells, TEMPORARY_BUFFER);
	grid.has_valid_cells = true;
	world->debug_drawer->AddGrid(&grid);
}

void SetBroadphaseTasks(ECSEngine::ModuleTaskFunctionData* data) {
	TaskSchedulerElement broadphase_element;
	broadphase_element.task_group = ECS_THREAD_TASK_INITIALIZE_EARLY;
	broadphase_element.initialize_task_function = InitializeCollisionBroadphase;
	broadphase_element.preserve_data = false;
	ECS_REGISTER_FOR_EACH_TASK(broadphase_element, CollisionBroadphase, data);

	TaskSchedulerElement broadphase_end_frame;
	broadphase_end_frame.task_group = ECS_THREAD_TASK_FINALIZE_LATE;
	ECS_REGISTER_TASK(broadphase_end_frame, CollisionBroadphaseEndFrame, data);
}

void SetBroadphaseDebugTasks(ECSEngine::ModuleRegisterDebugDrawTaskElementsData* data)
{
	ModuleDebugDrawTaskElement grid_draw;
	grid_draw.base_element.task_group = ECS_THREAD_TASK_FINALIZE_EARLY;
	grid_draw.input_element.SetCtrlWith(ECS_KEY_G, ECS_BUTTON_PRESSED);
	ECS_SET_SCHEDULE_TASK_FUNCTION(grid_draw.base_element, CollisionBroadphaseDisplayDebugGrid);
	data->elements->AddAssert(grid_draw);
}
