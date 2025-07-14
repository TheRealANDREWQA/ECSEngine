#include "pch.h"
#include "Scripting.h"
#include "ECSEngineModule.h"
#include "ECSEngineInputControllers.h"
#include "ECSEngineWorld.h"
#include "ECSEngineComponents.h"
#include "Logging.h"
#include "SolverData.h"
#include "ECSEngineECSRuntimeSystems.h"

static ECS_THREAD_TASK(CameraController) {
	EntityManager* entity_manager = world->entity_manager;
	CameraComponent* camera = entity_manager->TryGetGlobalComponent<CameraComponent>();
	if (camera != nullptr) {
		//const PhysicsSettings* settings = entity_manager->TryGetGlobalComponent<PhysicsSettings>();
		//if (settings != nullptr && settings->first_person) {
			if (world->mouse->IsVisible()) {
				world->mouse->SetCursorVisibility(false);
				world->mouse->EnableRawInput();
			}

			//int2 current_delta = world->mouse->GetPositionDelta();
			//int2 current_position = world->mouse->GetPosition();
			//int2 current_previous_position = world->mouse->GetPreviousPosition();
			//bool current_shift = world->keyboard->IsDown(ECS_KEY_LEFT_SHIFT);
			//bool current_ctrl = world->keyboard->IsDown(ECS_KEY_LEFT_CTRL);

			//int2 registered_delta;
			//int2 registered_position;
			//int2 registered_previos_position;
			//bool shift_is_pressed = false;
			//bool ctrl_is_pressed = false;
			//if (false) {
			//	RegisterRuntimeMonitoredBasicValue(world, world->mouse->GetPositionDelta());
			//	RegisterRuntimeMonitoredBasicValue(world, world->mouse->GetPosition());
			//	RegisterRuntimeMonitoredBasicValue(world, world->mouse->GetPreviousPosition());
			//	RegisterRuntimeMonitoredBasicValue(world, world->keyboard->IsDown(ECS_KEY_LEFT_SHIFT));
			//	RegisterRuntimeMonitoredBasicValue(world, world->keyboard->IsDown(ECS_KEY_LEFT_CTRL));
			//}
			//else {
			//	registered_delta = GetRuntimeMonitoredBasicValue<int2>(world);
			//	registered_position = GetRuntimeMonitoredBasicValue<int2>(world);
			//	registered_previos_position = GetRuntimeMonitoredBasicValue<int2>(world);
			//	shift_is_pressed = GetRuntimeMonitoredBasicValue<bool>(world);
			//	ctrl_is_pressed = GetRuntimeMonitoredBasicValue<bool>(world);

			//	if (registered_delta != current_delta || shift_is_pressed != current_shift || ctrl_is_pressed != current_ctrl) {
			//		shift_is_pressed = shift_is_pressed;
			//	}
			//}

			/*CameraComponent serialized_camera;
			if (false) {
				RegisterRuntimeMonitoredStruct(world, STRING(CameraComponent), camera);
				RegisterRuntimeMonitoredFloat(world, world->delta_time);
				RegisterRuntimeMonitoredFloat(world, world->inverse_delta_time);
			}
			else {
				GetRuntimeMonitoredStruct(world, STRING(CameraComponent), &serialized_camera, ECS_MALLOC_ALLOCATOR);
				float serialized_delta_time = GetRuntimeMonitoredFloat(world);
				float serialized_inverse_delta_time = GetRuntimeMonitoredFloat(world);
				if (serialized_camera.value.translation != camera->value.translation || serialized_camera.value.rotation != camera->value.rotation) {
					OutputDebugStringA("Coggers");
				}
				if (serialized_delta_time != world->delta_time || serialized_inverse_delta_time != world->inverse_delta_time) {
					OutputDebugStringA("Pog");
				}
			}*/

			FirstPersonWASDControllerModifiers(
				world->mouse,
				world->keyboard,
				5.0f,
				0.3f,
				world->delta_time,
				camera->value.translation,
				camera->value.rotation
			);
			camera->value.Recalculate();
		//}
	}
}

void AddScripts(ECSEngine::ModuleTaskFunctionData* data) {
	TaskSchedulerElement camera_controller;
	camera_controller.task_group = ECS_THREAD_TASK_INITIALIZE_EARLY;
	ECS_REGISTER_TASK(camera_controller, CameraController, data);
}