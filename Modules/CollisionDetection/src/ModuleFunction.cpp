#include "pch.h"
#include "ModuleFunction.h"

using namespace ECSEngine;

template<bool get_query>
ECS_THREAD_TASK(ApplyMovement) {
	ForEachEntity<get_query, QueryWrite<Translation>>(thread_id, world).Function([](
		ForEachEntityData* for_each_data, 
		Translation* translation
	) {
		translation->value.x -= 5.50f * for_each_data->world->delta_time;
	});

	if constexpr (!get_query) {
		auto task = [](unsigned int thread_id, World* world, void* _data) {
			size_t integer = (size_t)_data;
			for (size_t index = 0; index < 30000; index++) {
				integer *= integer;
			}
			if (world->delta_time == (float)integer) {
				__debugbreak();
			}
		};

		/*for (size_t index = 0; index < 10; index++) {
			world->task_manager->AddDynamicTaskGroup(task, "Task", (void*)1, 16, 0, false);
		}*/
	}
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