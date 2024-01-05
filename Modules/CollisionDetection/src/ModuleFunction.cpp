#include "pch.h"
#include "ModuleFunction.h"
#include "Components.h"
#include "Broadphase.h"

static void ApplyMovementTask(
	ForEachEntityData* for_each_data,
	Translation* translation
) {
	static bool was_used = false;

	//translation->value.x -= settings->factor * for_each_data->world->delta_time;
	//translation->value.x -= 5.5f * for_each_data->world->delta_time;
	if (for_each_data->world->keyboard->IsPressed(ECS_KEY_P)) {
		translation->value.x = 0.0f;
	}
	//if (for_each_data->world->mouse->IsPressed(ECS_MOUSE_LEFT)) {
	//	translation->value.x = 0.0f;
	//}
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

void ModuleTaskFunction(ModuleTaskFunctionData* data) {
	TaskSchedulerElement schedule_element;

	schedule_element.task_group = ECS_THREAD_TASK_FINALIZE_LATE;
	ECS_REGISTER_FOR_EACH_TASK(schedule_element, ApplyMovement, data);
	SetBroadphaseTasks(data);
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

#if 1

void ModuleRegisterDebugDrawTaskElementsFunction(ECSEngine::ModuleRegisterDebugDrawTaskElementsData* data) {
	SetBroadphaseDebugTasks(data);
}

#endif

#if 1

static bool ModuleCompareConvexCollider(SharedComponentCompareFunctionData* data) {
	const ConvexCollider* first = (const ConvexCollider*)data->first;
	const ConvexCollider* second = (const ConvexCollider*)data->second;
	return CompareConvexCollider(first, second);
}

static void ModuleDeallocateConvexCollider(ComponentDeallocateFunctionData* data) {
	ConvexCollider* collider = (ConvexCollider*)data->data;
	collider->hull.Deallocate(data->allocator);
}

static void ModuleCopyConvexCollider(ComponentCopyFunctionData* data) {
	ConvexCollider* destination = (ConvexCollider*)data->destination;
	const ConvexCollider* source = (const ConvexCollider*)data->source;
	destination->hull.Copy(&source->hull, data->allocator, data->deallocate_previous);
}

void ModuleRegisterComponentFunctionsFunction(ECSEngine::ModuleRegisterComponentFunctionsData* data) {
	ModuleComponentFunctions convex_collider;
	convex_collider.compare_function = ModuleCompareConvexCollider;
	convex_collider.deallocate_function = ModuleDeallocateConvexCollider;
	convex_collider.copy_function = ModuleCopyConvexCollider;
	convex_collider.allocator_size = ECS_KB * 256;
	convex_collider.component_name = STRING(ConvexCollider);

	data->functions->AddAssert(convex_collider);
}

#endif