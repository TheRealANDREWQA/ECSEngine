#include "pch.h"
#include "Scripting.h"
#include "ECSEngineModule.h"
#include "ECSEngineInputControllers.h"
#include "ECSEngineWorld.h"
#include "ECSEngineComponents.h"
#include "Logging.h"
#include "SolverData.h"

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

			FirstPersonWASDControllerModifiers(
				world->mouse,
				world->keyboard,
				5.0f,
				15.0f,
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