#include "pch.h"
#include "ModuleFunction.h"

static void ApplyMovementTask(
	ForEachEntityData* for_each_data,
	Translation* translation
) {
	static bool was_used = false;

	translation->value.x -= 5.50f * for_each_data->world->delta_time;
	translation->value.x -= 5.50f * for_each_data->world->delta_time;
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

void ModuleTaskFunction(ModuleTaskFunctionData* data) {
	TaskSchedulerElement schedule_element;

	schedule_element.task_group = ECS_THREAD_TASK_FINALIZE_LATE;
	ECS_REGISTER_FOR_EACH_TASK(schedule_element, ApplyMovement, data);
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