#include "pch.h"
#include "Broadphase.h"
#include "Graphics/src/Components.h"
#include "Components.h"
#include "FixedGrid.h"

bool DISPLAY_DEBUG_GRID = false;

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
			rotation_matrix = QuaternionToMatrixLow(rotation->value);
		}
		if (scale != nullptr) {
			scale_value = scale->value;
		}

		AABBStorage aabb = TransformAABB(mesh->mesh->mesh.bounds, translation_value, rotation_matrix, scale_value).ToStorage();
		fixed_grid->InsertEntry(for_each_data->entity, 0, aabb, &collisions);
	}
}

template<bool get_query>
ECS_THREAD_TASK(CollisionBroadphase) {
	if constexpr (!get_query) {
		FixedGrid* fixed_grid = (FixedGrid*)_data;
		fixed_grid->StartFrame();
	}
	ForEachEntityCommit<get_query, QueryRead<Translation>, QueryRead<RenderMesh>, QueryOptional<QueryRead<Rotation>>, QueryOptional<QueryRead<Scale>>>(thread_id, world)
		.Function(UpdateBroadphaseGrid, _data);
}

ECS_THREAD_TASK(InitializeCollisionBroadphase) {
	StaticThreadTaskInitializeInfo* initialize_info = (StaticThreadTaskInitializeInfo*)_data;

	FixedGrid fixed_grid;
	fixed_grid.Initialize(GetAllocatorPolymorphic(world->memory), { 1024, 4, 1024 }, { 2, 2, 2 }, 10);
	initialize_info->frame_data->CopyOther(&fixed_grid, sizeof(fixed_grid));
}

ECS_THREAD_TASK(CollisionBroadphaseEndFrame) {
	FixedGrid* fixed_grid = (FixedGrid*)_data;
	fixed_grid->EndFrame();
}

void SetBroadphaseTasks(ECSEngine::ModuleTaskFunctionData* data) {
	TaskSchedulerElement broadphase_element;
	broadphase_element.task_group = ECS_THREAD_TASK_INITIALIZE_EARLY;
	broadphase_element.initialize_task_function = InitializeCollisionBroadphase;
	ECS_REGISTER_FOR_EACH_TASK(broadphase_element, CollisionBroadphase, data);

	TaskSchedulerElement broadphase_end_frame;
	broadphase_end_frame.task_group = ECS_THREAD_TASK_FINALIZE_LATE;
	broadphase_end_frame.initialize_data_task_name = STRING(CollisionBroadphase);
	ECS_REGISTER_TASK(broadphase_end_frame, CollisionBroadphaseEndFrame, data);
}