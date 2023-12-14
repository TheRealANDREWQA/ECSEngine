#include "pch.h"
#include "ModuleFunction.h"
#include "Components.h"
#include "Graphics/src/Components.h"

static void ApplyMovementTask(
	ForEachEntityData* for_each_data,
	Translation* translation
) {
	static bool was_used = false;
	CollisionSettings* settings = (CollisionSettings*)for_each_data->world->system_manager->GetSystemSettings("CollisionDetection", STRING(CollisionSettings));

	translation->value.x -= settings->factor * for_each_data->world->delta_time;
	//translation->value.x -= 5.5f * for_each_data->world->delta_time;
	if (for_each_data->world->keyboard->IsPressed(ECS_KEY_P)) {
		translation->value.x = 0.0f;
	}
	if (for_each_data->world->mouse->IsPressed(ECS_MOUSE_LEFT)) {
		translation->value.x = 0.0f;
	}
	//translation->value.y -= 1.50f * for_each_data->world->delta_time;
	//translation->value.x -= 5.50f * for_each_data->world->delta_time;
	//if (for_each_data->thread_id == 2) {
	//	if (!was_used) {
	//		int* ptr = NULL;
	//		*ptr = 0;
	//		//for_each_data->world->entity_manager->GetComponent(Entity{ (unsigned int)-1 }, Translation::ID());
	//		was_used = true;
	//	}
	//}
}

template<bool get_query>
ECS_THREAD_TASK(ApplyMovement) {
	ForEachEntity<get_query, QueryWrite<Translation>>(thread_id, world).Function<QueryExclude<Scale>>(ApplyMovementTask);
}

static void UpdateBroadphaseGrid(ForEachEntityData* for_each_data, const Translation* translation, const RenderMesh* mesh, const Rotation* rotation, const Scale* scale) {

}

template<bool get_query>
ECS_THREAD_TASK(CollisionBroadphase) {
	ForEachEntity<get_query, QueryRead<Translation>, QueryRead<RenderMesh>, QueryOptional<QueryRead<Rotation>>, QueryOptional<QueryRead<Scale>>>(thread_id, world).Function(UpdateBroadphaseGrid);
}

ECS_THREAD_TASK(InitializeCollisionBroadphase) {

}

void ModuleTaskFunction(ModuleTaskFunctionData* data) {
	TaskSchedulerElement schedule_element;

	schedule_element.task_group = ECS_THREAD_TASK_FINALIZE_LATE;
	ECS_REGISTER_FOR_EACH_TASK(schedule_element, ApplyMovement, data);

	TaskSchedulerElement broadphase_element;
	broadphase_element.task_group = ECS_THREAD_TASK_INITIALIZE_EARLY;
	broadphase_element.initialize_task_function = InitializeCollisionBroadphase;
	ECS_REGISTER_FOR_EACH_TASK(broadphase_element, CollisionBroadphase, data);
}

#if 0

void ModuleUIFunction(ECSEngine::ModuleUIFunctionData* data) {

}

#endif

#if 0

void ModuleSerializeComponentFunction(ECSEngine::ModuleSerializeComponentFunctionData* data) {

}

#endif

#if 0

void ModuleRegisterLinkComponentFunction(ECSEngine::ModuleRegisterLinkComponentFunctionData* data) {

}

#endif

#if 0

void ModuleRegisterExtraInformationFunction(ECSEngine::ModuleRegisterExtraInformationFunctionData* data) {

}

#endif